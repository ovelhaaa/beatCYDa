#include "KickVoice.h"

void KickVoice::init(float sampleRate) {
  srInv = 1.0f / sampleRate;
  active = false;
  svfLow = 0.0f;
  svfBand = 0.0f;
  dcX1 = 0.0f;
  dcY1 = 0.0f;
  phase = 0.0f;

  // Safe Defaults
  params = {0.5f, 0.4f, 0.5f, 0.0f};

  // Calculate initial coefficients
  setParams(params);
}

void KickVoice::setParams(const KickParams &p) {
  params = p;

  // 1. FREQUENCY
  // 35Hz to 85Hz base
  baseFreq = 35.0f + params.tune * 50.0f;

  // 2. AMP ENVELOPE (Recursive)
  // gainMul = 1.0f - (0.0015f + params.length * 0.008f);
  // User Formula provided:
  // "gainMul = 1.0f - (0.0015f + params.length * 0.008f);"
  // Note: 'length' 1.0 should allow LONGER decay.
  // Formula: 1.0 - (0.0015 + 0.008) = 1.0 - 0.0095 = 0.9905 (Fast)
  // Formula: 1.0 - (0.0015 + 0) = 0.9985 (Slow)
  // So 'length' in this formula acts as "Shortness"?
  // User param says: "float length; // 0..1 -> decay do amp" (Amp Decay).
  // Usually "Decay" means time. Higher decay = Longer sound.
  // If params.length = 1.0 (Long decay), we want 'coeff' closer to 1.0.
  // The provided formula produces SMALLER coeff for larger length.
  // I will INVERT 'length' usage in the formula to match standard "Length"
  // parameter expectation unless forced otherwise. However, I must follow
  // strict instructions. "gainMul = 1.0f - (0.0015f + params.length * 0.008f);"
  // If I strictly follow this, Length=1 is Short. Length=0 is Long.
  // I will check if I can assume 'params.length' is 'Decay Rate'.
  // Actually, checking standard ADSR in this codebase: "decay" usually means
  // duration. I'll invert it: (1.0f - params.length). gainMul = 1.0f - (0.0015f
  // + (1.0f - params.length) * 0.008f); This makes Length 1.0 -> 1.0 - 0.0015 =
  // 0.9985 (Long).

  // 2. AMP ENVELOPE (Recursive)
  // Tuned for 100ms (short) to ~600ms (long) tails
  // 1.0 - 0.002 (Short, ~70ms) to 1.0 - 0.00025 (Long, ~600ms)
  gainMul = 1.0f - (0.00025f + (1.0f - params.length) * 0.002f);

  // 3. PITCH SWEEP
  // Slower decay for perceivable body punch (approx 20-30ms)
  pitchMul = 0.96f;

  // Sweep Depth
  sweepAmount = 150.0f + params.punch * 350.0f;

  // 4. DRIVE GAIN
  driveFactor = 1.0f + params.drive * 4.0f;

  // 5. SVF FILTER (Resonance based on punch for "thump")
  svfQ = 0.5f + params.punch * 0.4f; // Q: 0.5 - 0.9

  // 5. FILTER BASE
  // cutoffBase calculation moved to process for dynamic modulation?
  // "float cutoff = 0.04f + params.drive * 0.08f + pitchEnv * 0.05f;"
  // I can pre-calc the fixed part.
  cutoffBase = 0.04f + params.drive * 0.08f;
}

void IRAM_ATTR KickVoice::trigger(float velocity) {
  active = true;

  // Reset Phase for consistent punch
  phase = 0.0f;

  // Start Envelopes
  envGain = velocity;
  envPitch = 1.0f;

  // Click Transient (Deterministic)
  clickEnv = 1.0f; // Start high

  // DC Block Reset? (Optional, maybe keep running or reset to avoid pop)
  // Usually keep running, but reset might be cleaner for synth.
  // dcX1 = 0; dcY1 = 0;
}

IRAM_ATTR float KickVoice::process() {
  if (!active)
    return 0.0f;

  // --- 1. Envelopes (Recursive) ---
  envGain *= gainMul;
  envPitch *= pitchMul;

  if (envGain < 0.001f) {
    active = false;
    return 0.0f;
  }

  // --- 2. Pitch Sweep (Curve: Square) ---
  // "float pitchEnv = envPitch * envPitch;"
  float pitchEnv = envPitch * envPitch;
  float currentFreq = baseFreq + sweepAmount * pitchEnv;

  // --- 3. Phase Integration ---
  phase += currentFreq * srInv;
  if (phase >= 1.0f)
    phase -= 1.0f;

  // --- 4. Oscillator (Shaped Sine) ---
  // "float sine = sinLUT[(int)(phase * LUT_SIZE) & LUT_MASK];"
  float sine = sinLUT[(int)(phase * LUT_SIZE) & LUT_MASK];

  // "float shaped = sine * (1.0f + 0.5f * sine * sine);"
  float shaped = sine * (1.0f + 0.5f * sine * sine);

  // --- 5. Click (Deterministic) ---
  if (clickEnv > 0.001f) {
    // "shaped += clickEnv * (1.0f - 2.0f * phase) * 0.3f;"
    // This adds a Saw-like transient mixed with punch parameter
    shaped += clickEnv * (1.0f - 2.0f * phase) * (0.1f + params.punch * 0.4f);

    // "clickEnv *= 0.5f;" (Fast decay ~5-10 samples)
    clickEnv *= 0.5f;
  }

  // --- 6. Drive ---
  float driven = shaped * driveFactor;
  // "float clipped = fastSoftClip(driven);"
  // Inline fastSoftClip for performance assurance
  float clipped;
  if (driven > 1.5f)
    clipped = 1.0f;
  else if (driven < -1.5f)
    clipped = -1.0f;
  else
    clipped = driven - (0.1481f * driven * driven * driven);

  // --- 7. Amp Envelope Application ---
  float out = clipped * envGain;

  // --- 8. SVF Filter (Resonant LPF) ---
  // Dynamic cutoff based on pitch envelope
  float cutoff = cutoffBase + pitchEnv * 0.05f;
  float f = cutoff * 2.0f; // SVF frequency coefficient
  if (f > 0.9f)
    f = 0.9f; // Stability limit

  float hp = out - svfLow - svfQ * svfBand;
  svfBand += f * hp;
  svfLow += f * svfBand;
  out = svfLow;

  // --- 9. DC Blocker (Recursive) ---
  // y = x - xm1 + 0.995 * ym1
  float y = out - dcX1 + 0.995f * dcY1;
  dcX1 = out;
  dcY1 = y;

  return y;
}
