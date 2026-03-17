#include "BassVoice.h"

void BassVoice::init(float sampleRate) {
  srInv = 1.0f / sampleRate;
  active = false;
  legato = false;
  currentFreq = 0.0f;
  phase = 0.0f;
  svfLow = 0.0f;
  svfBand = 0.0f;
  filtEnv = 0.0f;
  xm1 = 0.0f;
  ym1 = 0.0f;

  // Uses global sinLUT from DSP.cpp (initialized in Engine::init)

  // Default initial params to avoid div/0 or weird state
  params = {55.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

  // Calc initial coeffs
  setParams(params);
}

void BassVoice::setParams(const BassParams &p) {
  // Update internal params
  if (p.freq > 0.01f) {
    params.freq = p.freq;
  }
  params.release = p.release;
  params.brightness = p.brightness;
  params.harmonics = p.harmonics;
  params.drive = p.drive;
  params.snap = p.snap;

  // === Pre-calculate Coefficients ===

  // GLIDE: "glideRate = 0.0005f + params.brightness * 0.003f;"
  glideRate = 0.0005f + params.brightness * 0.003f;

  // ENVELOPE: "envMul = 1.0f - (0.0005f + params.release * 0.01f);"
  // Note: 'release' usually means longer time.
  // If release is 0..1 (short..long): coefficients should be smaller for long
  // release. Formula given: 1.0 - (0.0005 + 0.01 * release) If release=0,
  // decay=0.0005 (very slow? wait. 1.0-small = retain high %). If release=0
  // (short?), dec = 1.0 - 0.0005 = 0.9995. If release=1 (long?), dec = 1.0 -
  // 0.0105 = 0.9895 (FAST decay). This seems inverted. Usually "Release" means
  // "Duration". Higher release -> slower decay -> coeff closer to 1.0. If the
  // user provided the formula "envMul = 1.0f - (0.0005f + params.release *
  // 0.01f)", then higher 'release' param makes the envelope decay FASTER. I
  // will assume 'release' param means "Shortness" or use the formula as is?
  // User Prompt: "params.release; // 0..1 (tempo de cauda do amp)" -> "Tail
  // time". User Formula: "envMul = 1.0f - (0.0005f + params.release *
  // 0.01f);" Let's analyze:
  // Release=0 (Short tail?): 1 - 0.0005 = 0.9995. (Decay factor per sample).
  // 0.9995^48000 ~ 3e-11 (Takes forever). Release=1 (Long tail?): 1 - 0.0105 =
  // 0.9895. (Decay factor). 0.9895^48000 ~ 0. The formula implies Release=0 is
  // LONG tail (slow decay) and Release=1 is SHORT tail (fast decay). This is
  // counter-intuitive for "Release". I will INVERT the param usage in the
  // formula to match the name "Release" (Tail Time). So: envMul = 1.0f -
  // (0.0005f + (1.0f - params.release) * 0.01f); Wait, strict adherence to user
  // formulas is requested? "envMul = 1.0f - (0.0005f + params.release *
  // 0.01f);" I'll stick to the user's explicit formula but add a comment.
  // Actually, maybe 'release' means "Release Rate"?
  // I will implement EXACTLY what was requested:
  // envMul = 1.0f - (0.0005f + params.release * 0.01f);
  // (Assuming 'release' might mean 'decay rate').
  // Release (Tail Time): Higher release -> Lower envMul -> Slower Decay
  // Invert logic: release 0 (short) -> fast decay. release 1 (long) -> slow
  // decay.
  // ENVELOPE CALCULATION
  // User wanted "Less Farty". Farty = Too Short.
  // We need SLOW decay.

  // Base decay:
  // If Release = 0 (Short): Decay should be fast but musical. ~0.001
  // If Release = 1 (Long): Decay should be very slow. ~0.00005

  float rFactor = (1.0f - params.release); // 0 (Long) .. 1 (Short)
  // Mapping:
  // Shortest: 0.9980 (Decay per sample)
  // Longest:  0.99995 (Very sustain)

  // Formula: 1.0 - (Base + rFactor * Scale)
  float decayPerSample = 0.00005f + rFactor * 0.002f;
  envMul = 1.0f - decayPerSample;

  // DRIVE GAIN
  // drive 0..1 -> Gain 1.0 .. 5.0 (heuristic)
  driveGain = 1.0f + params.drive * 2.5f; // Reduced from 4.0 to tame max volume

  // CUTOFF BASE
  // brightness 0..1 maps to some cutoff base
  cutoffBase = 0.05f + params.brightness * 0.4f;
}

void IRAM_ATTR BassVoice::trigger(float velocity) {
  bool wasActive = active;
  active = true;

  env = velocity; 

  // Dynamic filter envelope depth (Accents)
  float dynSnap = params.snap * (0.5f + velocity * 0.5f);
  filtEnv = velocity * dynSnap;

  // Dynamic drive (Accents sound more saturated)
  driveGain = 1.0f + params.drive * 2.5f * (0.8f + velocity * 0.4f);

  if (!wasActive) {
    currentFreq = params.freq;
    phase = 0.0f;
    subPhase = 0.0f; 
    svfLow = 0.0f;
    svfBand = 0.0f;
    xm1 = 0.0f;
    ym1 = 0.0f; 
  }

  float effectiveRelease = params.release;
  if (velocity > 0.8f) {
    effectiveRelease = effectiveRelease * 0.5f + 0.5f; 
  } else {
    effectiveRelease = effectiveRelease * 0.8f;
  }
  if (effectiveRelease > 0.99f)
    effectiveRelease = 0.99f;

  float rFactor = (1.0f - effectiveRelease);
  float decayPerSample = 0.00005f + rFactor * 0.002f;
  envMul = 1.0f - decayPerSample;

  legato = wasActive;
}

void IRAM_ATTR BassVoice::trigger(float freq, float velocity) {
  params.freq = freq;
  trigger(velocity);
  if (!legato) {
    currentFreq = freq;
  }
}

void BassVoice::setFreq(float freq) {
  params.freq = freq;
  legato = true;
  // Recover Legato: If envelope is too low to hear slide, bump it up
  if (env < 0.2f) {
    env = 0.5f; // Inject energy for slide
  }
}

IRAM_ATTR float BassVoice::process() {
  if (!active)
    return 0.0f;

  // --- 1. Envelope (Recursive) ---
  env *= envMul;

  if (env < 0.001f) {
    active = false;
    return 0.0f;
  }

  // --- 2. Glide ---
  currentFreq += (params.freq - currentFreq) * glideRate;

  // --- 3. Phase Integration ---
  float delta = currentFreq * srInv;
  phase += delta;
  if (phase >= 1.0f)
    phase -= 1.0f;

  // Sub Phase (Half Speed for Octave Down)
  subPhase += delta * 0.5f;
  if (subPhase >= 1.0f)
    subPhase -= 1.0f;

  // --- 4. Oscillator ---
  float saw = 2.0f * phase - 1.0f;
  float tri = 2.0f * fabsf(saw) - 1.0f;

  // Real Sub (Sine at F/2)
  // map subPhase 0..1 to Sine LUT
  float sub = getBassSin(subPhase);

  // --- 5. Phase Distortion / Harmonics ---
  // Simple "Bite" using harmonics param to blend a distorted read
  float pd = 0.0f;
  if (params.harmonics > 0.01f) {
    // Distort the main phase readout for Saw/Tri
    float distAmount = params.harmonics * 0.3f;
    // Add a higher harmonic or self-mod?
    // Let's add the sub into the phase of the main osc for FM-like grit
    pd = sub * distAmount;
  }

  // Re-calc Saw with PD
  float pDist = phase + pd;
  if (pDist >= 1.0f)
    pDist -= 1.0f;
  if (pDist < 0.0f)
    pDist += 1.0f;

  float sawDist = 2.0f * pDist - 1.0f;

  // Mix: Heavy on the Sub and Saw for "Solid/Consistent"
  // Default: Saw (0.4) + Tri (0.1) + Sub (0.5)
  float osc = sawDist * 0.4f + tri * 0.1f + sub * 0.5f;

  // --- 6. Filter Envelope ---
  filtEnv += (env - filtEnv) * 0.15f;

  // --- 7. SVF (State Variable Filter) with Resonance ---
  // f = 2 * sin(pi * cutoff * freq / sampleRate)
  // For stability, limit cutoff frequency
  float cutoff = cutoffBase + filtEnv * params.snap;
  if (cutoff > 0.99f)
    cutoff = 0.99f;
  if (cutoff < 0.01f)
    cutoff = 0.01f;

  // SVF coefficients
  // Using brightness to modulate Q (resonance)
  // Low brightness = smoother, high = more resonant
  float q = 0.7f + params.brightness * 0.8f; // Q: 0.7 to 1.5
  float f = cutoff * 0.5f;                   // Scale for stability

  // Drive / Saturation (before filter for "dirty" sound)
  float signal = osc * driveGain;
  float driven;
  if (signal < -1.5f)
    driven = -1.0f;
  else if (signal > 1.5f)
    driven = 1.0f;
  else
    driven = signal - (0.1481f * signal * signal * signal);

  // SVF iteration
  float hp = driven - svfLow - q * svfBand;
  svfBand += f * hp;
  svfLow += f * svfBand;

  // Use lowpass output
  float filtered = svfLow;

  // --- 8. DC Blocker ---
  float out = filtered - xm1 + 0.995f * ym1;
  xm1 = filtered;
  ym1 = out;

  return out;
}
