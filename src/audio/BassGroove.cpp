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

// Motifs (degrees in current scale, -1 = silence)
static const int8_t motifDegrees[4][4][16] = {
    // Minor
    {{0, -1, 2, -1, 4, -1, 2, -1, 0, -1, 3, -1, 4, -1, 2, -1},
     {0, -1, 0, 2, -1, 3, -1, 2, 4, -1, 3, -1, 2, -1, 0, -1},
     {0, 2, -1, 3, -1, 4, -1, 3, 2, -1, 0, -1, 2, -1, 4, -1},
     {0, -1, 4, -1, 3, -1, 2, -1, 0, -1, 2, -1, 3, -1, 4, -1}},
    // Major
    {{0, -1, 2, -1, 4, -1, 5, -1, 4, -1, 2, -1, 0, -1, 2, -1},
     {0, -1, 0, 2, -1, 4, -1, 5, 4, -1, 2, -1, 1, -1, 0, -1},
     {0, 2, -1, 4, -1, 5, -1, 4, 2, -1, 0, -1, 1, -1, 2, -1},
     {0, -1, 5, -1, 4, -1, 2, -1, 0, -1, 1, -1, 2, -1, 4, -1}},
    // Dorian
    {{0, -1, 2, -1, 3, -1, 5, -1, 4, -1, 2, -1, 0, -1, 3, -1},
     {0, -1, 0, 2, -1, 3, -1, 5, 3, -1, 2, -1, 1, -1, 0, -1},
     {0, 2, -1, 3, -1, 5, -1, 4, 2, -1, 0, -1, 1, -1, 3, -1},
     {0, -1, 4, -1, 3, -1, 2, -1, 0, -1, 1, -1, 3, -1, 5, -1}},
    // Phrygian
    {{0, -1, 1, -1, 3, -1, 4, -1, 3, -1, 1, -1, 0, -1, 1, -1},
     {0, -1, 0, 1, -1, 3, -1, 4, 3, -1, 1, -1, 0, -1, 1, -1},
     {0, 1, -1, 3, -1, 4, -1, 3, 1, -1, 0, -1, 1, -1, 3, -1},
     {0, -1, 4, -1, 3, -1, 1, -1, 0, -1, 1, -1, 3, -1, 4, -1}}};

static const float motifAccents[16] = {1.0f, 0.0f, 0.72f, 0.0f, 0.95f, 0.0f,
                                       0.68f, 0.0f, 0.88f, 0.0f, 0.74f, 0.0f,
                                       0.92f, 0.0f, 0.80f, 0.0f};

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
  params.phraseVariation = 0.5f;
  params.motifIndex = 0;

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
  phraseStep = 0;
  phraseVariant = 0;
  hasPendingMotifDegree = false;
  pendingMotifDegree = 0;
}

void BassGroove::updateParams(const BassGrooveParams &newParams) {
  params = newParams;
  // Clamp values if necessary
  if (params.density < 0.0f)
    params.density = 0.0f;
  if (params.density > 0.8f) // Cap max density to prevent chaos
    params.density = 0.8f;
  if (params.phraseVariation < 0.0f)
    params.phraseVariation = 0.0f;
  if (params.phraseVariation > 1.0f)
    params.phraseVariation = 1.0f;

  if (params.motifIndex >= 4)
    params.motifIndex = 0;

  if (static_cast<uint8_t>(params.mode) > static_cast<uint8_t>(GrooveMode::MOTIF))
    params.mode = GrooveMode::FOLLOW_KICK;

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
  const int wrappedStep = ((currentStep % 64) + 64) % 64;
  phraseStep = wrappedStep % 16;
  phraseVariant = (wrappedStep / 16) % 2;
  altState = (phraseVariant == 1);

  bool hasKick = kickReceived;
  kickReceived = false;

  if (timeSinceLastTriggerMs < params.minIntervalMs) {
    return;
  }

  // --- 1. RHYTHM LOGIC ---
  float p = params.density;
  bool isDownBeat = (phraseStep == 0);
  bool isQuarterStep = (wrappedStep % 4 == 0);
  bool isPreferredOffbeat = (wrappedStep % 4 == 2);

  // Keep downbeat protection across every mode.
  if (isDownBeat) {
    p = 0.95f;
    degree = 0;
    octave = 0;
  } else {
    switch (params.mode) {
    case GrooveMode::FOLLOW_KICK:
      if (hasKick) {
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

  if (params.mode == GrooveMode::MOTIF) {
    const uint8_t scaleIdx = static_cast<uint8_t>(params.scaleType);
    const uint8_t motifIdx = params.motifIndex & 0x03;
    const int stepIdx = phraseStep & 0x0F;
    const int motifDegree = motifDegrees[scaleIdx][motifIdx][stepIdx];

    if (motifDegree >= 0) {
      hasPendingMotifDegree = true;
      pendingMotifDegree = motifDegree;
      const float accent = motifAccents[stepIdx];
      const bool motifAccent = (accent >= 0.85f) || isDownBeat;
      trigger(motifAccent);
    }
    return;
  }

  if (((float)xorShift(rngState) / 4294967295.0f) < p) {
    // Pass context to trigger
    bool isAccent = isDownBeat || isQuarterStep ||
                    (((float)xorShift(rngState) / 4294967295.0f) < 0.3f);
    trigger(isAccent); // Helper overload
  }

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
  int scaleLen = cachedScaleLen;

  if (hasPendingMotifDegree) {
    hasPendingMotifDegree = false;
    int degreeOut = pendingMotifDegree % scaleLen;
    if (degreeOut < 0)
      degreeOut += scaleLen;

    degree = degreeOut;
    octave = 0;

    int note = params.rootNote + cachedScale[degreeOut] + params.octaveOffset * 12;
    note = constrain(note, 0, 127);
    return (uint8_t)note;
  }

  // --- 4. HARMONIC LOGIC ---
  // Walker with Gravity towards Root/Fifth
  int rangeSemitones = params.range;
  if (rangeSemitones < 0)
    rangeSemitones = 0;

  auto semitonesForDegreeSpan = [&](int spanDegrees) {
    if (spanDegrees <= 0)
      return 0;
    int octs = spanDegrees / scaleLen;
    int deg = spanDegrees % scaleLen;
    return (octs * 12) + cachedScale[deg];
  };

  // Convert "range in semitones (approx)" to reachable scale-degree span.
  int maxDegreeSpanFromRange = 0;
  while (semitonesForDegreeSpan(maxDegreeSpanFromRange + 1) <= rangeSemitones) {
    maxDegreeSpanFromRange++;
  }

  int currentAbsDegree = octave * scaleLen + degree;
  int candidateAbsDegree = currentAbsDegree;

  const float varAmt = params.phraseVariation;
  const bool isPhraseClosure = (phraseStep == 15);
  const bool preferCadence = isPhraseClosure || (phraseVariant == 1 && phraseStep >= 12);
  const int stableBias = preferCadence ? static_cast<int>(30.0f * varAmt) : 0;

  // Weights (Probabilities out of 100)
  int r = xorShift(rngState) % 100;

  // Decision Tree
  // 60% Chance: Stay/Move to Root or Fifth
  // 40% Chance: Walk +/- 1

  if (r < (50 + stableBias)) {
    // STABLE: Pick Root or Fifth
    const int rootChance = 50 + static_cast<int>(35.0f * varAmt);
    if ((xorShift(rngState) % 100) < rootChance)
      candidateAbsDegree = octave * scaleLen; // Root (Grounding)
    else
      candidateAbsDegree = octave * scaleLen + 4; // Fifth (approx, mapped later)
  } else if (r < (80 + static_cast<int>(10.0f * varAmt))) {
    // WALK SMALL
    int step = ((xorShift(rngState) & 1) ? 1 : -1);
    if (altState && randomUnit() < (0.25f + (0.35f * varAmt))) {
      step = -step;
    }
    candidateAbsDegree = currentAbsDegree + step;
  } else {
    // JUMP (Octave or Third)
    const int jump = (xorShift(rngState) % 3) + 2;
    candidateAbsDegree = currentAbsDegree + ((altState && varAmt > 0.2f) ? -jump : jump);
  }

  // Force cadence tendency at phrase closure.
  if (isPhraseClosure && randomUnit() < (0.7f + 0.25f * varAmt)) {
    candidateAbsDegree = octave * scaleLen;
    if (randomUnit() < (0.45f - 0.25f * varAmt)) {
      candidateAbsDegree = octave * scaleLen + 4;
    }
  }

  // 1) Limit max displacement in scale degrees per step.
  int deltaDegrees = candidateAbsDegree - currentAbsDegree;
  if (deltaDegrees > maxDegreeSpanFromRange)
    deltaDegrees = maxDegreeSpanFromRange;
  if (deltaDegrees < -maxDegreeSpanFromRange)
    deltaDegrees = -maxDegreeSpanFromRange;
  candidateAbsDegree = currentAbsDegree + deltaDegrees;

  // 2) Limit walker excursion around center (degree=0 / octave=0).
  if (candidateAbsDegree > maxDegreeSpanFromRange)
    candidateAbsDegree = maxDegreeSpanFromRange;
  if (candidateAbsDegree < -maxDegreeSpanFromRange)
    candidateAbsDegree = -maxDegreeSpanFromRange;

  // 3) Map absolute degree back to cached scale degree + relative octave.
  int mappedOctave = candidateAbsDegree / scaleLen;
  int mappedDegree = candidateAbsDegree % scaleLen;
  if (mappedDegree < 0) {
    mappedDegree += scaleLen;
    mappedOctave--;
  }

  degree = mappedDegree;
  octave = mappedOctave;

  int degreeOut = degree; // Finalize

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
