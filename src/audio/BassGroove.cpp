#include "BassGroove.h"
#include "Engine.h"
#include "VoiceManager.h"
#include "esp_system.h" // For esp_random()
#include <stdlib.h>

// External reference to the global VoiceManager
extern VoiceManager voiceManager;

// Scales
// Minor (Natural/Aeolian): R 2 b3 4 5 b6 b7
static const int scaleMinor[] = {0, 2, 3, 5, 7, 8, 10};

// Major (Ionian): R 2 3 4 5 6 7
static const int scaleMajor[] = {0, 2, 4, 5, 7, 9, 11};

// Others placeholders (fallback to Minor for now)
static const int scaleDorian[] = {0, 2, 3, 5, 7, 9, 10};   // R 2 b3 4 5 6 b7
static const int scalePhrygian[] = {0, 1, 3, 5, 7, 8, 10}; // R b2 b3 4 5 b6 b7

// XorShift helper - inline for speed
static inline uint32_t xorShift(uint32_t &state) {
  state ^= (state << 13);
  state ^= (state >> 17);
  state ^= (state << 5);
  return state;
}

void BassGroove::init(float sr) {
  sampleRate = sr;

  // Default Params
  params.rootNote = 36; // C2
  params.scaleType = BassScale::MINOR;
  params.octaveOffset = 0;
  params.mode = GrooveMode::FOLLOW_KICK;
  params.density = 0.05f; // User requested "Fewer notes"
  params.minIntervalMs = 9.0f;
  params.range = 5;
  params.slideProb = 0.3f; // Slightly increased for Legato feel

  // Reset State
  lastNote = 36;
  timeSinceLastTriggerMs = 1000.0f; // Ready to fire
  kickReceived = false;

  // Initialize RNG with hardware random for uniqueness
  rngState = esp_random();
  if (rngState == 0)
    rngState = 0x5EEDCAFE; // Avoid zero state

  // Cache default scale
  cachedScale = scaleMinor;
  cachedScaleLen = 7;

  // 303 Init
  degree = 0;
  octave = 0;
  altState = false;
}

void BassGroove::updateParams(const BassGrooveParams &newParams) {
  params = newParams;
  // Clamp values if necessary
  if (params.density < 0.0f)
    params.density = 0.0f;
  if (params.density > 0.8f) // Cap max density to prevent chaos
    params.density = 0.8f;

  // Update cached scale pointer
  switch (params.scaleType) {
  case BassScale::MAJOR:
    cachedScale = scaleMajor;
    cachedScaleLen = 7;
    break;
  case BassScale::DORIAN:
    cachedScale = scaleDorian;
    cachedScaleLen = 7;
    break;
  case BassScale::PHRYGIAN:
    cachedScale = scalePhrygian;
    cachedScaleLen = 7;
    break;
  case BassScale::MINOR:
  default:
    cachedScale = scaleMinor;
    cachedScaleLen = 7;
    break;
  }
}

void BassGroove::onKick() { kickReceived = true; }

void BassGroove::onTick(int currentStep) {
  if (timeSinceLastTriggerMs < params.minIntervalMs) {
    kickReceived = false;
    return;
  }

  // --- 1. RHYTHM LOGIC ---
  float p = params.density;
  bool isDownBeat = (currentStep % 16 == 0);
  bool isQuarterStep = (currentStep % 4 == 0);
  bool isPreferredOffbeat =
      (currentStep == 2 || currentStep == 6 || currentStep == 10 || currentStep == 14);

  // Keep downbeat protection across every mode.
  if (isDownBeat) {
    p = 0.95f;
    degree = 0;
    octave = 0;
  } else {
    switch (params.mode) {
    case GrooveMode::FOLLOW_KICK:
      if (kickReceived) {
        // Boost probability when kick is present.
        p = 0.78f + (params.density * 0.22f);
      } else if (isPreferredOffbeat) {
        // Moderate fallback on offbeat syncopation.
        p = params.density * 0.55f;
      } else {
        p = params.density * 0.28f;
      }
      break;

    case GrooveMode::OFFBEAT:
      if (isQuarterStep) {
        // Avoid strong beat positions.
        p = params.density * 0.2f;
      } else if (isPreferredOffbeat) {
        // Prefer classic offbeat placements.
        p = params.density * 1.25f;
      } else {
        p = params.density * 0.7f;
      }
      break;

    case GrooveMode::RANDOM:
    default:
      // Mostly linear density mapping with no kick dependency.
      p = params.density;
      break;
    }
  }

  // Clamp
  if (p > 1.0f)
    p = 1.0f;
  if (p < 0.0f)
    p = 0.0f;

  if (((float)xorShift(rngState) / 4294967295.0f) < p) {
    // Pass context to trigger
    bool isAccent = isDownBeat || isQuarterStep ||
                    (((float)xorShift(rngState) / 4294967295.0f) < 0.3f);
    trigger(isAccent); // Helper overload
  }

  kickReceived = false;
}

void BassGroove::process(float dt_ms) { timeSinceLastTriggerMs += dt_ms; }

void BassGroove::trigger() { trigger(false); }

void BassGroove::trigger(bool forceAccent) {
  uint8_t note = generateNote();
  float freq = midiToFreq(note);

  // --- 2. SLIDE LOGIC ---
  // If dense enough, slide.
  bool slide = randomUnit() < params.slideProb;
  if (params.slideProb <= 0.01f)
    slide = false;

  // --- 3. VELOCITY / LENGTH LOGIC ---
  // User wants "Less Farty" -> longer notes.
  // We use Velocity to control Release in BassVoice. Higher Vel = Longer.
  float velocity = 0.5f;

  if (forceAccent) {
    velocity = 0.9f + randomUnit() * 0.1f; // 0.9..1.0 (Long)
  } else {
    // Ghost notes: Make them slightly longer than before (0.4 was too short?)
    velocity = 0.6f + randomUnit() * 0.3f; // 0.6..0.9
  }

  if (slide && voiceManager.isVoiceActive(VoiceID::VOICE_BASS)) {
    voiceManager.setFreq(VoiceID::VOICE_BASS, freq);
  } else {
    voiceManager.triggerFreq(VoiceID::VOICE_BASS, freq, velocity);
  }

  VoiceParams bassParams = voiceManager.getParams(VOICE_BASS);
  float gateMs = 90.0f + (bassParams.decay * 510.0f);
  if (slide)
    gateMs += 80.0f;
  if (forceAccent)
    gateMs += 120.0f;
  gateMs = constrain(gateMs, 60.0f, 1200.0f);
  engine.midiOut.triggerBass(note, velocity, gateMs);

  lastNote = note;
  timeSinceLastTriggerMs = 0.0f;
}

uint8_t BassGroove::generateNote() {
  // --- 4. HARMONIC LOGIC ---
  // Walker with Gravity towards Root/Fifth

  int degreeOut = degree;

  // Weights (Probabilities out of 100)
  int r = xorShift(rngState) % 100;

  // Decision Tree
  // 60% Chance: Stay/Move to Root or Fifth
  // 40% Chance: Walk +/- 1

  if (r < 50) {
    // STABLE: Pick Root or Fifth
    if ((xorShift(rngState) & 1) == 0)
      degreeOut = 0; // Root (Grounding)
    else
      degreeOut = 4; // Fifth (approx, mapped later)
  } else if (r < 80) {
    // WALK SMALL
    int step = (xorShift(rngState) & 1) ? 1 : -1;
    degreeOut = degree + step;
  } else {
    // JUMP (Octave or Third)
    degreeOut = degree + (xorShift(rngState) % 3) + 2;
  }

  // Apply changes to state
  degree = degreeOut;

  // Use cached scale (updated in updateParams())
  int scaleLen = cachedScaleLen;

  // Wrap degree to scale
  // Custom Wrap logic to handle negative walking
  while (degree < 0) {
    degree += scaleLen;
    octave--;
  }
  while (degree >= scaleLen) {
    degree -= scaleLen;
    octave++;
  }

  // Boundary check on Octave
  if (octave < -1)
    octave = -1;
  if (octave > 1)
    octave = 1;

  degreeOut = degree; // Finalize

  // Calculate MIDI Note using cached scale
  int note = params.rootNote + cachedScale[degreeOut] +
             (octave + params.octaveOffset) * 12;

  // Safety Clamp
  if (note < 0)
    note = 0;
  if (note > 127)
    note = 127;

  return (uint8_t)note;
}

float BassGroove::midiToFreq(uint8_t note) {
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

bool BassGroove::checkProb(float p) {
  if (p <= 0.0f)
    return false;
  if (p >= 1.0f)
    return true;
  return randomUnit() < p;
}

float BassGroove::randomUnit() {
  return (float)xorShift(rngState) / 4294967295.0f;
}
