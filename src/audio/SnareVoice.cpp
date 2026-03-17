#include "SnareVoice.h"
#include "esp_system.h" // For esp_random()
// System LUT (1024 floating point sine table assumed in DSP.cpp)
// Using macros from DSP.h or redefining safely if needed
#ifndef LUT_SIZE
#define LUT_SIZE 1024
#define LUT_MASK 1023
#endif

// Fast exponential decay approximation for envelopes
// Used only in trigger
static inline float calculateEnvMul(float ms, float srInv) {
  // decay to 0.001 (-60dB)
  // coeff = exp(-6.9 / (ms * 0.001 * sr))
  // simplified: exp(-6900 / (ms * sr)) ??
  // Actually: -6.907 / (ms_seconds * sr)
  // = -6.907 * srInv / (ms * 0.001)
  // = -6907.8 * srInv / ms
  if (ms < 0.1f)
    ms = 0.1f;
  return expf(-6907.8f * srInv / ms);
}

void SnareVoice::init(float sampleRate) {
  srInv = 1.0f / sampleRate;
  active = false;
  rngState = esp_random(); // Hardware RNG for unique randomness each boot

  // Safe Defaults
  params = {0.5f, 0.5f, 0.5f, 0};

  // Reset States
  wire1 = 0.0f;
  wire2 = 0.0f;
  dcX1 = 0.0f;
  dcY1 = 0.0f;
  phase = 0.0f;
  phase2 = 0.0f;
}

void SnareVoice::setParams(const SnareParams &p) { params = p; }

void SnareVoice::trigger(float velocity) {
  active = true;

  // 1. Reset Phase (Consistency)
  phase = 0.0f;
  phase2 = 0.0f;

  // 2. Initialize Envelopes
  envBody = velocity;
  envNoise = velocity; // Wire
  envClick = velocity;
  envPitch = 1.0f;
  envRim = velocity; // If mode 1

  // Velocity mapping
  // Low velocity = shorter decay, darker timbre
  float dynDecay = params.decay * (0.4f + velocity * 0.6f);
  float dynTimbre = params.timbre * (0.5f + velocity * 0.5f);
  float dynTone = params.tone;

  // 3. Pre-calculate Decay Times (ms)
  float bodyMs = 80.0f + dynDecay * 170.0f;
  float wireMs = 60.0f + dynDecay * 390.0f;
  float clickMs = 1.5f + dynTimbre * 1.5f;
  float pitchMs = 15.0f;

  if (params.mode == 1) { // Rim
    float rimMs = 20.0f + dynDecay * 100.0f;
    envMulRim = calculateEnvMul(rimMs, srInv);

    float rimFreq = 800.0f + dynTone * 1200.0f;
    phaseInc = rimFreq * srInv;
  } else if (params.mode == 2) { // Clap
    float clapMs = 100.0f + dynDecay * 300.0f;
    envMulNoise = calculateEnvMul(clapMs, srInv);
    wire1 = 0.0f;

    clapBurstCount = 4;
    clapBurstSamples = 0;
    for (int i = 0; i < 4; i++) {
      clapBurstEnvs[i] = 0.0f;
    }
    clapBurstEnvs[0] = velocity;
  } else {                       // Snare (0)
    envMulBody = calculateEnvMul(bodyMs, srInv);
    envMulNoise = calculateEnvMul(wireMs, srInv);
    envMulClick = calculateEnvMul(clickMs, srInv);
    envMulPitch = calculateEnvMul(pitchMs, srInv);

    // 4. Tuning (Membrane)
    baseFreq = 170.0f + dynTone * 70.0f + (velocity * 20.0f); // Hit harder = higher pitch
    phaseInc = baseFreq * srInv;
    float f2 = baseFreq * 1.48f;
    phaseInc2 = f2 * srInv;

    // 5. Wire Filter Coefficients (Timbre modulated)
    filterC1 = 0.12f + dynTimbre * 0.10f;
    filterC2 = 0.28f + dynTimbre * 0.15f;

    // 6. Gains
    wireGain = 1.0f + dynTimbre * 0.8f;
    clickGain = 0.5f + dynTimbre * 1.0f;
  }
}

IRAM_ATTR float SnareVoice::process() {
  if (!active)
    return 0.0f;

  float out = 0.0f;

  if (params.mode == 0)
    out = renderSnare();
  else if (params.mode == 1)
    out = renderRim();
  else
    out = renderClap();

  // DC Blocker (Essential for wire/click offsets)
  float y = out - dcX1 + 0.995f * dcY1;
  dcX1 = out;
  dcY1 = y;

  return y;
}

IRAM_ATTR float SnareVoice::renderSnare() {
  // --- Envelopes ---
  envBody *= envMulBody;
  envNoise *= envMulNoise;
  envClick *= envMulClick;
  envPitch *= envMulPitch;

  if (envBody < 0.001f && envNoise < 0.001f) {
    active = false;
    return 0.0f;
  }

  // --- Pitch Snap ---
  float snap = envPitch * envPitch;
  float currentInc = phaseInc * (1.0f + snap * 0.5f); // 50% Pitch drop range

  // --- Oscillators (Membrane) ---
  phase += currentInc;
  if (phase >= 1.0f)
    phase -= 1.0f;

  phase2 += phaseInc2; // Osc2 fixed pitch usually works for drum heads
  if (phase2 >= 1.0f)
    phase2 -= 1.0f;

  float b1 = sinLUT[(int)(phase * LUT_SIZE) & LUT_MASK];
  float b2 = sinLUT[(int)(phase2 * LUT_SIZE) & LUT_MASK];

  // Mix & Dispersion
  float m = (b1 * 0.65f) + (b2 * 0.35f);
  // Non-linear dispersion (adds harmonics)
  m += m * m * 0.2f;

  float membrane = fastSat(m * envBody * 1.1f);

  // --- Click (Deterministic) ---
  // Saw-like transient: env * (1 - 2*phase)
  // Short burst at start
  float click = 0.0f;
  if (envClick > 0.001f) {
    click = envClick * (1.0f - 2.0f * phase);
    click = fastSat(click * clickGain);
  }

  // --- Wire / Noise ---
  // RNG
  float n = (float)(xorShift() & 65535) / 32768.0f - 1.0f;

  // Bandpass (Subtractive 1-pole)
  wire1 += (n - wire1) * filterC1;
  wire2 += (n - wire2) * filterC2;
  float wireSig = wire2 - wire1;

  float wire = wireSig * envNoise * wireGain;

  // --- Final Mix ---
  float out = (membrane * 0.9f) + wire + (click * 0.6f);

  return fastSat(out);
}

IRAM_ATTR float SnareVoice::renderRim() {
  envRim *= envMulRim;
  if (envRim < 0.001f) {
    active = false;
    return 0.0f;
  }

  phase += phaseInc;
  if (phase >= 1.0f)
    phase -= 1.0f;

  float tone = sinLUT[(int)(phase * LUT_SIZE) & LUT_MASK];
  // Slight noise ring
  float n = (float)(xorShift() & 65535) / 32768.0f - 1.0f;

  float out = (tone * 0.8f) + (n * 0.2f);
  return fastSat(out * envRim * 2.0f);
}

// Improved Clap with burst envelope
IRAM_ATTR float SnareVoice::renderClap() {
  // Master envelope decay
  envNoise *= envMulNoise;

  // Check if all bursts are done
  bool allDone = true;
  for (int i = 0; i < 4; i++) {
    if (clapBurstEnvs[i] > 0.001f)
      allDone = false;
  }
  if (allDone && envNoise < 0.001f) {
    active = false;
    return 0.0f;
  }

  // Trigger next burst after inter-burst interval (~15-25ms = 720-1200 samples
  // @ 48kHz)
  if (clapBurstSamples > 0) {
    clapBurstSamples--;
  } else if (clapBurstCount > 0) {
    int burstIdx = 4 - clapBurstCount;
    if (burstIdx < 4) {
      clapBurstEnvs[burstIdx] =
          envNoise * (0.9f - burstIdx * 0.15f); // Each burst slightly quieter
    }
    clapBurstCount--;
    // Variable spacing: gets tighter towards the end
    int baseSpacing = 900 - (4 - clapBurstCount) * 150;  // 900, 750, 600, 450
    clapBurstSamples = baseSpacing + (xorShift() % 200); // Add randomness
  }

  // Generate filtered noise
  float n = (float)(xorShift() & 65535) / 32768.0f - 1.0f;
  wire1 += (n - wire1) * 0.35f; // Bandpass-ish filter

  // Sum all active bursts
  float burstSum = 0.0f;
  float burstDecay = 0.92f; // Fast decay for each burst
  for (int i = 0; i < 4; i++) {
    if (clapBurstEnvs[i] > 0.001f) {
      burstSum += wire1 * clapBurstEnvs[i];
      clapBurstEnvs[i] *= burstDecay;
    }
  }

  float out = burstSum * 2.0f;
  return fastSat(out);
}
