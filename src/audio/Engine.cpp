#include "Engine.h"
#include "DSP.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <Arduino.h>
#include <algorithm>
#include <vector>

#ifndef constrain
#define constrain(amt, low, high)                                              \
  ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif

Engine engine;

// Musical Scales
const int SCALES[4][8] = {
    {0, 2, 3, 5, 7, 8, 10, 12}, // Minor
    {0, 2, 4, 5, 7, 9, 11, 12}, // Major
    {0, 2, 3, 5, 7, 9, 10, 12}, // Dorian
    {0, 1, 3, 5, 7, 8, 10, 12}  // Phrygian
};

const float NOTE_FREQS[12] = {65.41f,  69.30f,  73.42f,  77.78f,
                              82.41f,  87.31f,  92.50f,  98.00f,
                              103.83f, 110.00f, 116.54f, 123.47f};

namespace {
constexpr uint8_t BASS_ROOT_NOTE_MIN = 24;
constexpr uint8_t BASS_ROOT_NOTE_MAX = 48;
}

Engine::Engine() {
  // Global constructor - minimal work
  patternMutex = NULL;
  for (int i = 0; i < TRACK_COUNT; i++) {
    trackVersions[i] = 0;
    trackMutes[i] = false;
  }
  mainTimer = NULL;
}

void Engine::init() {
  if (patternMutex == NULL) {
    patternMutex = xSemaphoreCreateRecursiveMutex();
  }

  // Initialize Sine LUT
  initLUT();

  initFrequencyCache();
  midiOut.init();
  // VoiceManager init called in main.cpp
  voiceManager.init(SAMPLE_RATE);

  // Initialize Bass Groove
  bassGroove.init(SAMPLE_RATE);

  // Default Bass Parameters (Basic Bassline on Startup)
  BassGrooveParams bgp;
  bgp.rootNote = 36; // C2
  bgp.scaleType = BassScale::MINOR;
  bgp.octaveOffset = 0;
  bgp.mode = GrooveMode::FOLLOW_KICK; // Sync with Kick for solid groove
  bgp.density = 0.6f;                 // Reasonable activity
  bgp.minIntervalMs = 150.0f;         // Don't get too crazy fast
  bgp.range = 7;                      // +/- 5th
  bgp.slideProb = 0.2f;               // Occasional slide
  bgp.phraseVariation = 0.6f;         // Gentle A/B and cadence contrast
  bgp.motifIndex = 0;
  bgp.swing = 0.0f;
  bgp.ghostProb = 0.2f;
  bgp.accentProb = 0.3f;

  bassGroove.updateParams(bgp);
  globalRoot.store(bassMidiToRootClass(bgp.rootNote));
  globalScale.store(globalScaleFromBass(bgp.scaleType));

  VoiceParams bassParams = voiceManager.getParams(VOICE_BASS);
  bassParams.pitch = bassRootNoteToNormalizedPitch(bgp.rootNote);
  voiceManager.setParams(VOICE_BASS, bassParams);

  // NVS Init and Boot Counter
  preferences.begin("engine", false);

  // Mark as initialized (legacy/debug)
  if (!preferences.getBool("init_v4", false)) {
    preferences.putBool("init_v4", true);
  }

  int bootCounter = preferences.getInt("boot_cnt", 0);
  bootCounter = (bootCounter + 1) % 6; // Cycle 0-5
  preferences.putInt("boot_cnt", bootCounter);
  preferences.end();

  int bootSlot = bootCounter;

  loadSlot(bootSlot);
  applySlotToActive(bootSlot);

  engineReady.store(true);
}

// initFactoryPresets removed/inlined above
void Engine::setFactoryPreset(int slotId) {
  // Clear first
  for (int i = 0; i < TRACK_COUNT; i++) {
    slots[slotId].t_hits[i] = 0;
    slots[slotId].t_steps[i] = 16;
    slots[slotId].synthPitch[i] = 0.5f;
    slots[slotId].synthDecay[i] = 0.5f;
    slots[slotId].synthTimbre[i] = 0.0f;
    slots[slotId].synthDrive[i] = 0.0f;
    slots[slotId].synthSnap[i] = 0.0f;
    slots[slotId].synthHarmonics[i] = 0.0f;
  }
  slots[slotId].root = 0;
  slots[slotId].scale = 0;
  slots[slotId].bassDensity = 0.6f;
  slots[slotId].bassRange = 7;
  slots[slotId].bassMode = static_cast<int>(GrooveMode::FOLLOW_KICK);
  slots[slotId].bassMotifIndex = 0;
  slots[slotId].bpm = 120;
  slots[slotId].snareMode = 0;

  switch (slotId) {
  case 0: // Industrial
    slots[0].root = 0;
    slots[0].scale = 1;
    slots[0].bpm = 125;
    slots[0].t_hits[0] = 4;
    slots[0].t_hits[2] = 4;
    slots[0].synthDecay[0] = 0.6f;
    slots[0].synthTimbre[0] = 0.8f;
    break;
  case 1: // Glitch
    slots[1].root = 2;
    slots[1].scale = 2;
    slots[1].bpm = 110;
    slots[1].t_hits[0] = 5;
    slots[1].t_steps[0] = 13;
    slots[1].t_hits[1] = 3;
    slots[1].t_steps[1] = 7;
    for (int i = 2; i < TRACK_COUNT; i++)
      slots[1].t_hits[i] = 4;
    break;
  case 2: // Dub
    slots[2].root = 5;
    slots[2].scale = 0;
    slots[2].bpm = 110;
    slots[2].snareMode = 2;
    for (int i = 0; i < TRACK_COUNT; i++)
      slots[2].t_hits[i] = 4;
    slots[2].synthDecay[0] = 0.8f;
    slots[2].synthTimbre[0] = 0.2f;
    break;
  case 3: // Agro
    slots[3].root = 0;
    slots[3].scale = 3;
    slots[3].bpm = 140;
    for (int i = 0; i < TRACK_COUNT; i++)
      slots[3].t_hits[i] = 8;
    for (int i = 0; i < TRACK_COUNT; i++)
      slots[3].synthTimbre[i] = 1.0f;
    break;
  case 4: // Ambient
    slots[4].root = 7;
    slots[4].scale = 0;
    slots[4].bpm = 80;
    for (int i = 0; i < TRACK_COUNT; i++) {
      slots[4].t_hits[i] = 2;
      slots[4].synthDecay[i] = 0.9f;
    }
    break;
  case 5: // Minimal
    slots[5].root = 9;
    slots[5].scale = 1;
    slots[5].bpm = 122;
    for (int i = 0; i < TRACK_COUNT; i++) {
      slots[5].t_hits[i] = 3;
      slots[5].synthDecay[i] = 0.1f;
      slots[5].synthTimbre[i] = 0.9f;
    }
    break;
  }

  slots[slotId].synthPitch[VOICE_BASS] =
      bassRootNoteToNormalizedPitch(bassRootClassToMidi(slots[slotId].root));
}

uint8_t Engine::bassRootClassToMidi(int rootClass) const {
  int normalized = rootClass % 12;
  if (normalized < 0)
    normalized += 12;
  return (uint8_t)(36 + normalized);
}

int Engine::bassMidiToRootClass(uint8_t rootNote) const { return rootNote % 12; }

float Engine::bassRootNoteToNormalizedPitch(uint8_t rootNote) const {
  int clamped = constrain((int)rootNote, (int)BASS_ROOT_NOTE_MIN,
                          (int)BASS_ROOT_NOTE_MAX);
  return (float)(clamped - BASS_ROOT_NOTE_MIN) /
         (float)(BASS_ROOT_NOTE_MAX - BASS_ROOT_NOTE_MIN);
}

uint8_t Engine::normalizedPitchToBassRootNote(float normalizedPitch) const {
  float clamped = constrain(normalizedPitch, 0.0f, 1.0f);
  return (uint8_t)lroundf(BASS_ROOT_NOTE_MIN +
                          clamped * (BASS_ROOT_NOTE_MAX - BASS_ROOT_NOTE_MIN));
}

BassScale Engine::bassScaleFromGlobal(int scale) const {
  switch (scale) {
  case 1:
    return BassScale::MAJOR;
  case 2:
    return BassScale::DORIAN;
  case 3:
    return BassScale::PHRYGIAN;
  case 0:
  default:
    return BassScale::MINOR;
  }
}

int Engine::globalScaleFromBass(BassScale scale) const {
  switch (scale) {
  case BassScale::MAJOR:
    return 1;
  case BassScale::DORIAN:
    return 2;
  case BassScale::PHRYGIAN:
    return 3;
  case BassScale::MINOR:
  default:
    return 0;
  }
}

void Engine::syncBassGrooveFromGlobals() {
  BassGrooveParams bp = bassGroove.getParams();
  int octaveBase = (bp.rootNote / 12) * 12;
  int desiredRootClass = globalRoot.load() % 12;
  if (desiredRootClass < 0)
    desiredRootClass += 12;

  int candidate = octaveBase + desiredRootClass;
  while (candidate < BASS_ROOT_NOTE_MIN)
    candidate += 12;
  while (candidate > BASS_ROOT_NOTE_MAX)
    candidate -= 12;

  bp.rootNote = (uint8_t)constrain(candidate, (int)BASS_ROOT_NOTE_MIN,
                                   (int)BASS_ROOT_NOTE_MAX);
  bp.scaleType = bassScaleFromGlobal(globalScale.load());
  bassGroove.updateParams(bp);

  VoiceParams bassParams = voiceManager.getParams(VOICE_BASS);
  bassParams.pitch = bassRootNoteToNormalizedPitch(bp.rootNote);
  voiceManager.setParams(VOICE_BASS, bassParams);
}

void Engine::syncGlobalsFromBassGroove() {
  BassGrooveParams bp = bassGroove.getParams();
  globalRoot.store(bassMidiToRootClass(bp.rootNote));
  globalScale.store(globalScaleFromBass(bp.scaleType));

  VoiceParams bassParams = voiceManager.getParams(VOICE_BASS);
  bassParams.pitch = bassRootNoteToNormalizedPitch(bp.rootNote);
  voiceManager.setParams(VOICE_BASS, bassParams);
}

void Engine::syncBassGrooveFromVoicePitch(float normalizedPitch) {
  BassGrooveParams bp = bassGroove.getParams();
  bp.rootNote = normalizedPitchToBassRootNote(normalizedPitch);
  bassGroove.updateParams(bp);
  syncGlobalsFromBassGroove();
}

bool Engine::lockPattern() {
  return xSemaphoreTakeRecursive(patternMutex, portMAX_DELAY) == pdTRUE;
}

void Engine::unlockPattern() { xSemaphoreGiveRecursive(patternMutex); }

void Engine::initFrequencyCache() {
  for (int root = 0; root < 12; root++) {
    float baseFreq = NOTE_FREQS[root];
    for (int semitone = 0; semitone < 96; semitone++) {
      cachedNoteFreqs[root][semitone] = baseFreq * powf(2.0f, semitone / 12.0f);
    }
  }
}

void Engine::recalculatePatternUnlocked(int id) {
  if (id < 0 || id >= TRACK_COUNT)
    return;

  if (tracks[id].steps > 64)
    tracks[id].steps = 64;
  int steps = tracks[id].steps;
  int hits = tracks[id].hits;

  uint8_t *pat = tracks[id].pattern;

  // 1. GERAÇÃO EUCLIDIANA (Bresenham - O(n), sem alocação)
  if (steps > 0) {
    if (hits <= 0) {
      memset(pat, 0, steps);
    } else if (hits >= steps) {
      memset(pat, 1, steps);
    } else {
      // Bresenham-style Euclidean rhythm
      int bucket = 0;
      for (int i = 0; i < steps; i++) {
        bucket += hits;
        if (bucket >= steps) {
          bucket -= steps;
          pat[i] = 1;
        } else {
          pat[i] = 0;
        }
      }
    }
    tracks[id].patternLen = steps;
  } else {
    tracks[id].patternLen = 0;
  }

  int patLen = tracks[id].patternLen;

  // 2. ROTAÇÃO AUTOMÁTICA (Garantir Downbeat)
  if (autoRotateDownbeat.load() && patLen > 0 && hits > 0 && hits < steps) {
    int rotations = 0;
    while (pat[0] == 0 && rotations < steps) {
      // Manual rotate left by 1
      uint8_t first = pat[0];
      memmove(pat, pat + 1, patLen - 1);
      pat[patLen - 1] = first;
      rotations++;
    }
  }

  // 3. ROTAÇÃO MANUAL (UI Action)
  int manualRot = tracks[id].rotationOffset;
  if (patLen > 0 && manualRot != 0) {
    int rot = manualRot % patLen;
    if (rot < 0)
      rot += patLen;

    if (rot > 0) {
      // Rotate right by 'rot' positions
      // Use temp buffer for simplicity
      uint8_t temp[64];
      memcpy(temp, pat, patLen);
      for (int i = 0; i < patLen; i++) {
        pat[(i + rot) % patLen] = temp[i];
      }
    }
  }

  // Update Version for UI
  trackVersions[id].fetch_add(1, std::memory_order_relaxed);

  // 4. APLICAR DINÂMICA (Accents, Humanization e Ghost Notes)
  bool firstHitFound = false;
  
  for (int s = 0; s < patLen; s++) {
    if (pat[s] != 0) {
      // Smart Accent: Accentuate the first hit of the rhythm (it moves with rotation)
      if (!firstHitFound) {
        pat[s] = 127; 
        firstHitFound = true;
      } else {
        // Normal hit
        pat[s] = 85;
      }
      
      // Humanization: Add +/- 5 velocity to all active hits
      int hum = (esp_random() % 11) - 5;
      int v = (int)pat[s] + hum;
      v = constrain(v, 1, 127);
      pat[s] = (uint8_t)v;
    } else {
      // Empty step. Inject Ghost Notes for Snare (1) and Hats (2, 3)
      if (id == 1 || id == 2 || id == 3) {
        // ~15% chance to play a ghost note on empty steps
        if ((esp_random() % 100) < 15) {
          int ghostVel = 20 + (esp_random() % 15); // Velocity between 20 and 34
          pat[s] = (uint8_t)ghostVel;
        }
      }
    }
  }

  trackVersions[id].fetch_add(1, std::memory_order_relaxed);
}

void Engine::recalculatePattern(int id) {
  if (lockPattern()) {
    recalculatePatternUnlocked(id);
    unlockPattern();
  }
}

void Engine::randomize() {
  if (lockPattern()) {
    for (int i = 0; i < TRACK_COUNT; i++) {
      int var = (esp_random() % 5) - 2;
      tracks[i].hits += var;
      if (tracks[i].hits < 0)
        tracks[i].hits = 0;
      if (tracks[i].hits > tracks[i].steps)
        tracks[i].hits = tracks[i].steps;

      // Random rotation using fixed array
      int patLen = tracks[i].patternLen;
      if (patLen > 0) {
        int r = esp_random() % patLen;
        if (r > 0) {
          uint8_t temp[64];
          memcpy(temp, tracks[i].pattern, patLen);
          for (int j = 0; j < patLen; j++) {
            tracks[i].pattern[(j + r) % patLen] = temp[j];
          }
        }
      }
    }
    for (int i = 0; i < TRACK_COUNT; i++)
      recalculatePatternUnlocked(i);
    unlockPattern();
  }
}

void Engine::captureActiveToSlot(int slotId) {
  if (slotId < 0 || slotId >= 16)
    return;
  for (int i = 0; i < TRACK_COUNT; i++) {
    slots[slotId].t_hits[i] = tracks[i].hits;
    slots[slotId].t_steps[i] = tracks[i].steps;
    VoiceParams vp = voiceManager.getParams((VoiceID)i);
    slots[slotId].synthPitch[i] = vp.pitch;
    slots[slotId].synthDecay[i] = vp.decay;
    slots[slotId].synthTimbre[i] = vp.timbre;
    slots[slotId].synthDrive[i] = vp.drive;
    slots[slotId].synthSnap[i] = vp.snap;
    slots[slotId].synthHarmonics[i] = vp.harmonics;
  }
  slots[slotId].root = globalRoot.load();
  slots[slotId].scale = globalScale.load();
  BassGrooveParams bg = bassGroove.getParams();
  slots[slotId].bassDensity = bg.density;
  slots[slotId].bassRange = bg.range;
  slots[slotId].bassMode = static_cast<int>(bg.mode);
  slots[slotId].bassMotifIndex = bg.motifIndex;
  slots[slotId].snareMode = voiceManager.getParams(VOICE_SNARE).mode;
  slots[slotId].bpm = bpm.load(); // Capture BPM
}

void Engine::saveSlot(int slotId, bool capture) {
  if (slotId < 0 || slotId >= 16)
    return;
  if (capture) {
    captureActiveToSlot(slotId);
  }
  String ns = "slot" + String(slotId);
  preferences.begin(ns.c_str(), false);
  for (int i = 0; i < TRACK_COUNT; i++) {
    preferences.putInt(("h" + String(i)).c_str(), slots[slotId].t_hits[i]);
    preferences.putInt(("s" + String(i)).c_str(), slots[slotId].t_steps[i]);
    preferences.putFloat(("p" + String(i)).c_str(),
                         slots[slotId].synthPitch[i]);
    preferences.putFloat(("d" + String(i)).c_str(),
                         slots[slotId].synthDecay[i]);
    preferences.putFloat(("c" + String(i)).c_str(),
                         slots[slotId].synthTimbre[i]);
    preferences.putFloat(("dr" + String(i)).c_str(),
                         slots[slotId].synthDrive[i]);
    preferences.putFloat(("sn" + String(i)).c_str(),
                         slots[slotId].synthSnap[i]);
    preferences.putFloat(("ha" + String(i)).c_str(),
                         slots[slotId].synthHarmonics[i]);
  }
  preferences.putInt("root", slots[slotId].root);
  preferences.putInt("scale", slots[slotId].scale);
  preferences.putFloat("bassDensity", slots[slotId].bassDensity);
  preferences.putInt("bassRange", slots[slotId].bassRange);
  preferences.putInt("bassMode", slots[slotId].bassMode);
  preferences.putInt("bassMotif", slots[slotId].bassMotifIndex);
  preferences.putInt("snareMode", slots[slotId].snareMode);
  preferences.putInt("bpm", slots[slotId].bpm);
  preferences.putBool("is_saved", true); // Mark as saved
  preferences.end();
}

void Engine::loadSlot(int slotId) {
  if (slotId < 0 || slotId >= 16)
    return;

  String ns = "slot" + String(slotId);
  preferences.begin(ns.c_str(), false); // RW needed? No, logic is read. But
                                        // begin param is readOnly. false = RW.

  // Check if this slot has ever been saved
  if (!preferences.getBool("is_saved", false)) {
    // Not saved -> Load Factory Default
    setFactoryPreset(slotId);
  } else {
    // Saved -> Load from NVS
    for (int i = 0; i < TRACK_COUNT; i++) {
      slots[slotId].t_hits[i] =
          preferences.getInt(("h" + String(i)).c_str(), 0);
      slots[slotId].t_steps[i] =
          preferences.getInt(("s" + String(i)).c_str(), 16);
      slots[slotId].synthPitch[i] =
          preferences.getFloat(("p" + String(i)).c_str(), 0.5f);
      slots[slotId].synthDecay[i] =
          preferences.getFloat(("d" + String(i)).c_str(), 0.5f);
      slots[slotId].synthTimbre[i] =
          preferences.getFloat(("c" + String(i)).c_str(), 0.0f);
      slots[slotId].synthDrive[i] =
          preferences.getFloat(("dr" + String(i)).c_str(), 0.0f);
      slots[slotId].synthSnap[i] =
          preferences.getFloat(("sn" + String(i)).c_str(), 0.0f);
      slots[slotId].synthHarmonics[i] =
          preferences.getFloat(("ha" + String(i)).c_str(), 0.0f);
    }
    slots[slotId].root = preferences.getInt("root", 0);
    slots[slotId].scale = preferences.getInt("scale", 0);
    slots[slotId].bassDensity = preferences.getFloat("bassDensity", 0.6f);
    slots[slotId].bassRange = preferences.getInt("bassRange", 7);
    slots[slotId].bassMode = preferences.getInt("bassMode",
                                               static_cast<int>(GrooveMode::FOLLOW_KICK));
    slots[slotId].bassMotifIndex = preferences.getInt("bassMotif", 0);
    slots[slotId].snareMode = preferences.getInt("snareMode", 0);
    slots[slotId].bpm = preferences.getInt("bpm", 120);
  }
  preferences.end();
}

void Engine::loadAllSlots() {
  for (int s = 0; s < 16; s++) {
    loadSlot(s);
  }
}

void Engine::applySlotToActive(int slotId) {
  if (slotId < 0 || slotId >= 16)
    return;
  if (lockPattern()) {
    globalRoot.store(slots[slotId].root);
    globalScale.store(slots[slotId].scale);

    for (int i = 0; i < TRACK_COUNT; i++) {
      tracks[i].hits = slots[slotId].t_hits[i];
      tracks[i].steps = slots[slotId].t_steps[i];

      // Push to VoiceManager
      VoiceParams vp;
      vp.pitch = slots[slotId].synthPitch[i];
      vp.decay = slots[slotId].synthDecay[i];
      vp.timbre = slots[slotId].synthTimbre[i];
      vp.drive = slots[slotId].synthDrive[i];
      vp.snap = slots[slotId].synthSnap[i];
      vp.harmonics = slots[slotId].synthHarmonics[i];
      vp.mode = (i == 1) ? slots[slotId].snareMode : 0;

      voiceManager.setParams((VoiceID)i, vp);
    }

    BassGrooveParams bg = bassGroove.getParams();
    bg.rootNote =
        normalizedPitchToBassRootNote(slots[slotId].synthPitch[VOICE_BASS]);
    int slotRootClass = slots[slotId].root % 12;
    if (slotRootClass < 0)
      slotRootClass += 12;
    int octaveBase = (bg.rootNote / 12) * 12;
    int candidateRoot = octaveBase + slotRootClass;
    while (candidateRoot < BASS_ROOT_NOTE_MIN)
      candidateRoot += 12;
    while (candidateRoot > BASS_ROOT_NOTE_MAX)
      candidateRoot -= 12;
    bg.rootNote = (uint8_t)constrain(candidateRoot, (int)BASS_ROOT_NOTE_MIN,
                                     (int)BASS_ROOT_NOTE_MAX);
    bg.scaleType = bassScaleFromGlobal(slots[slotId].scale);
    bg.density = slots[slotId].bassDensity;
    bg.range = slots[slotId].bassRange;
    bg.mode = static_cast<GrooveMode>(slots[slotId].bassMode);
    bg.motifIndex = (uint8_t)(slots[slotId].bassMotifIndex & 0x03);
    bassGroove.updateParams(bg);
    syncGlobalsFromBassGroove();

    for (int i = 0; i < TRACK_COUNT; i++)
      recalculatePatternUnlocked(i);
    unlockPattern();
  }
}

void Engine::updateTimerFreq(hw_timer_t *timer) {
  if (timer) {
    mainTimer = timer;
  }
  if (!mainTimer)
    return;

  int bpmVal = bpm.load();
  if (bpmVal > 300)
    bpmVal = 300;
  if (bpmVal < 30)
    bpmVal = 30;
  timerAlarmWrite(mainTimer, 60000000ULL / (bpmVal * 4), true);
}

namespace {
void setVoiceParamNormalized(int track, int paramIdx, float normalized) {
  if (track < 0 || track >= TRACK_COUNT) {
    return;
  }

  const float value = constrain(normalized, 0.0f, 1.0f);
  VoiceParam target = VoiceParam::PARAM_PITCH;
  bool handled = true;

  if (track == VOICE_KICK) {
    switch (paramIdx) {
    case 0:
      target = VoiceParam::PARAM_PITCH;
      break;
    case 1:
      target = VoiceParam::PARAM_DECAY;
      break;
    case 2:
      target = VoiceParam::PARAM_TIMBRE;
      break;
    case 3:
      target = VoiceParam::PARAM_DRIVE;
      break;
    default:
      handled = false;
      break;
    }
  } else if (track == VOICE_BASS) {
    switch (paramIdx) {
    case 0:
      target = VoiceParam::PARAM_PITCH;
      break;
    case 1:
      target = VoiceParam::PARAM_DECAY;
      break;
    case 2:
      target = VoiceParam::PARAM_TIMBRE;
      break;
    case 3:
      target = VoiceParam::PARAM_DRIVE;
      break;
    default:
      handled = false;
      break;
    }
  } else {
    switch (paramIdx) {
    case 0:
      target = VoiceParam::PARAM_PITCH;
      break;
    case 1:
      target = VoiceParam::PARAM_DECAY;
      break;
    case 2:
      target = VoiceParam::PARAM_TIMBRE;
      break;
    default:
      handled = false;
      break;
    }
  }

  if (!handled) {
    return;
  }

  voiceManager.setParam((VoiceID)track, target, value);
  if (target == VoiceParam::PARAM_PITCH) {
    engine.tracks[track].pitch = value;
  } else if (target == VoiceParam::PARAM_DECAY) {
    engine.tracks[track].decay = value;
  } else if (target == VoiceParam::PARAM_TIMBRE) {
    engine.tracks[track].color = value;
  } else if (target == VoiceParam::PARAM_DRIVE) {
    engine.tracks[track].drive = value;
  }

  if (track == VOICE_BASS && target == VoiceParam::PARAM_PITCH) {
    engine.syncBassGrooveFromVoicePitch(value);
  }
}

void setBassControlAbsolute(int paramIdx, int value) {
  BassGrooveParams params = engine.bassGroove.getParams();
  const int clamped = constrain(value, 0, 100);

  switch (paramIdx) {
  case 0:
    params.density = constrain(clamped / 100.0f, 0.0f, 1.0f);
    break;
  case 1:
    params.range = constrain(1 + (clamped * 11 / 100), 1, 12);
    break;
  case 2:
    params.scaleType = static_cast<BassScale>(constrain(clamped * 4 / 100, 0, 3));
    break;
  case 3:
    params.rootNote = static_cast<uint8_t>(constrain(24 + (clamped * 24 / 100),
                                                     (int)BASS_ROOT_NOTE_MIN,
                                                     (int)BASS_ROOT_NOTE_MAX));
    break;
  case static_cast<int>(BassParamId::MOTIF_INDEX):
    params.motifIndex = static_cast<uint8_t>(constrain(clamped * 4 / 100, 0, 3));
    params.mode = GrooveMode::MOTIF;
    break;
  case static_cast<int>(BassParamId::SWING):
    params.swing = clamped / 100.0f;
    break;
  case static_cast<int>(BassParamId::GHOST_PROB):
    params.ghostProb = clamped / 100.0f;
    break;
  case static_cast<int>(BassParamId::ACCENT_PROB):
    params.accentProb = clamped / 100.0f;
    break;
  default:
    return;
  }

  engine.bassGroove.updateParams(params);
  engine.syncGlobalsFromBassGroove();
}
} // namespace

void Engine::handleUiAction(UiAction action) {
  if (action.type == UiActionType::TOGGLE_PLAY) {
    if (action.value == 0) {
      if (isPlaying.load()) {
        stop();
      } else {
        play();
      }
    } else {
      if (action.value == 1) {
        play();
      } else {
        stop();
      }
    }
    return;
  }

  if (action.type == UiActionType::CHANGE_MODE) {
    uiMode.store((UiMode)action.value);
    return;
  }

  if (action.type == UiActionType::SELECT_TRACK) {
    uiActiveTrack.store(constrain(action.value, 0, TRACK_COUNT - 1));
    return;
  }

  if (action.type == UiActionType::TOGGLE_MUTE) {
    const int track = constrain(action.value, 0, TRACK_COUNT - 1);
    trackMutes[track].store(!trackMutes[track].load());
    return;
  }

  if (action.type == UiActionType::SET_BPM ||
      action.type == UiActionType::NUDGE_BPM) {
    int nextBpm = bpm.load();
    if (action.type == UiActionType::SET_BPM) {
      nextBpm = action.value;
    } else {
      nextBpm += action.value;
    }
    bpm.store(constrain(nextBpm, 40, 240));
    updateTimerFreq(nullptr);
    return;
  }

  if (action.type == UiActionType::SET_STEPS ||
      action.type == UiActionType::SET_HITS ||
      action.type == UiActionType::SET_ROTATION) {
    const int track = constrain(action.index, 0, TRACK_COUNT - 1);
    if (lockPattern()) {
      if (action.type == UiActionType::SET_STEPS) {
        tracks[track].steps = constrain(action.value, 1, 64);
        tracks[track].hits =
            constrain(tracks[track].hits, 0, tracks[track].steps);
      } else if (action.type == UiActionType::SET_HITS) {
        tracks[track].hits = constrain(action.value, 0, tracks[track].steps);
      } else {
        tracks[track].rotationOffset =
            constrain(action.value, 0, max(0, tracks[track].steps - 1));
      }
      recalculatePatternUnlocked(track);
      unlockPattern();
    }
    return;
  }

  if (action.type == UiActionType::SET_BASS_PARAM) {
    setBassControlAbsolute(action.index, action.value);
    return;
  }

  if (action.type == UiActionType::SET_SOUND_PARAM) {
    const int track = uiActiveTrack.load();
    uiActiveParamIndex.store(action.index);
    setVoiceParamNormalized(track, action.index, action.value / 100.0f);
    return;
  }

  if (action.type == UiActionType::SET_VOICE_GAIN) {
    const VoiceID track = static_cast<VoiceID>(constrain(action.index, 0, TRACK_COUNT - 1));
    voiceManager.setVoiceGain(track, action.value / 100.0f);
    return;
  }

  if (action.type == UiActionType::RANDOMIZE_TRACK) {
    const int track = constrain(action.index, 0, TRACK_COUNT - 1);
    if (lockPattern()) {
      const int steps = constrain(tracks[track].steps, 1, 64);
      const int minHits = (track == VOICE_BASS) ? 1 : 0;
      tracks[track].hits = minHits + static_cast<int>(esp_random() % (steps - minHits + 1));
      tracks[track].rotationOffset = static_cast<int>(esp_random() % steps);
      recalculatePatternUnlocked(track);
      unlockPattern();
    }
    return;
  }
}

void Engine::play() {
  if (!isPlaying.load()) {
    currentStep.store(0);
    isPlaying.store(true);
    midiOut.sendTransportStart();
  }
}

void Engine::stop() {
  if (isPlaying.load()) {
    isPlaying.store(false);
    midiOut.sendTransportStop();
    midiOut.allNotesOff();
  }
}
