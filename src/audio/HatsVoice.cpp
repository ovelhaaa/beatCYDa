#include "HatsVoice.h"

void HatsVoice::init(float sampleRate) {
  srInv = 1.0f / sampleRate;
  active = false;
  for (int i = 0; i < 4; i++)
    phase[i] = 0.0f;
  svfLow = 0.0f;
  svfBand = 0.0f;
  svfQ = 0.7f;
  params = {0.5f, 0.5f, false};
}

void HatsVoice::setParams(const HatsParams &p) { params = p; }

void IRAM_ATTR HatsVoice::trigger(float velocity) {
  active = true;
  env = velocity;

  // Envelope Voicing:
  // Closed: Tight (40ms - 100ms)
  // Open: Long (300ms - 900ms)
  float minMs = params.open ? 300.0f : 40.0f;
  float maxMs = params.open ? 900.0f : 100.0f;
  float ms = minMs + params.decay * (maxMs - minMs);

  // Recursive Coeff
  envMul = expf(-6907.8f * srInv / ms);

  // Tuning & Ratios (808-ish Logic)
  // Base Cluster: ~300Hz - 600Hz range
  float baseFreq = 300.0f + params.timbre * 300.0f;

  // Specific Metallic Ratios
  float ratios[4] = {1.0f, 1.48f, 2.15f, 3.71f};

  for (int i = 0; i < 4; i++) {
    phase[i] = 0.0f;
    inc[i] = baseFreq * ratios[i] * srInv;
  }
}

// Inline Metal Generator: Sum of 4 inharmonic sines
// NO fmodf, NO division, NO branching
inline float HatsVoice::getMetal() {
  float sum = 0.0f;
  // Unroll loop for speed
  sum += sinLUT[(int)(phase[0] * LUT_SIZE) & LUT_MASK];
  sum += sinLUT[(int)(phase[1] * LUT_SIZE) & LUT_MASK];
  sum += sinLUT[(int)(phase[2] * LUT_SIZE) & LUT_MASK];
  sum += sinLUT[(int)(phase[3] * LUT_SIZE) & LUT_MASK];
  return sum;
}

IRAM_ATTR float HatsVoice::process() {
  if (!active)
    return 0.0f;

  // 1. Envelope
  env *= envMul;
  if (env < 0.001f) {
    active = false;
    return 0.0f;
  }

  // 2. Oscillators (Phase Accumulation with Pitch Drift)
  // Subtle random drift per oscillator for organic metallic texture
  for (int i = 0; i < 4; i++) {
    // Add tiny random pitch drift (±0.5% per sample)
    float drift = 1.0f + fastRandom() * 0.005f;
    phase[i] += inc[i] * drift;
    if (phase[i] >= 1.0f)
      phase[i] -= 1.0f;
  }

  // 3. Generation
  float metal = getMetal() * 0.25f; // Normalize 4 oscs
  float noise = fastRandom();

  // 4. Mix (Timbre Control)
  // Low Timbre = More Noise (Sandy)
  // High Timbre = More Metal (Ringy)
  float mix = metal * (0.3f + params.timbre * 0.7f) +
              noise * (0.6f - params.timbre * 0.4f);

  // 5. SVF Filter (Resonant HPF for sparkle)
  // Higher timbre = higher cutoff + more resonance
  float cutoff = 0.15f + params.timbre * 0.25f; // 0.15-0.4 range
  float q = 0.5f + params.timbre * 0.5f;        // Q: 0.5-1.0
  float f = cutoff * 2.0f;
  if (f > 0.9f)
    f = 0.9f;

  float hp = mix - svfLow - q * svfBand;
  svfBand += f * hp;
  svfLow += f * svfBand;
  float out = hp; // Use highpass output for brightness

  return out * env;
}
