#include "VoiceManager.h"

VoiceManager voiceManager;

void VoiceManager::init(float sr) {
  kick.init(sr);
  snare.init(sr);
  hatC.init(sr);
  hatO.init(sr);
  bass.init(sr);

  uiState = {};
  uiState.masterVolume = 0.5f;
  uiState.masterDrive = 0.0f;

  for (int i = 0; i < VOICE_COUNT; i++) {
    uiState.params[i] = {0.5f, 0.5f, 0.0f, 0, 0.0f, 0.0f, 0.0f};
    uiState.voiceGains[i] = (i == VOICE_BASS) ? 0.1f : 0.5f;
  }

  publishedStates[0] = uiState;
  publishedStates[1] = uiState;
  audioState = uiState;

  publishedStateIndex.store(0, std::memory_order_relaxed);
  stagingStateIndex = 1;
  syncedStateIndex = 0xFF;
  evtWrite.store(0, std::memory_order_relaxed);
  evtRead.store(0, std::memory_order_relaxed);

  applyAudioState();
  syncedStateIndex = publishedStateIndex.load(std::memory_order_relaxed);
}

void VoiceManager::setParams(VoiceID id, const VoiceParams &p) {
  if (id >= VOICE_COUNT)
    return;

  uiState.params[id] = p;
  publishUiState();
}

void VoiceManager::setParam(VoiceID id, VoiceParam param, float value) {
  if (id >= VOICE_COUNT)
    return;

  switch (param) {
  case VoiceParam::PARAM_PITCH:
    uiState.params[id].pitch = value;
    break;
  case VoiceParam::PARAM_DECAY:
    uiState.params[id].decay = value;
    break;
  case VoiceParam::PARAM_TIMBRE:
    uiState.params[id].timbre = value;
    break;
  case VoiceParam::PARAM_MODE:
    if (id == VOICE_SNARE) {
      uiState.params[id].mode = (uint8_t)value;
    }
    break;
  case VoiceParam::PARAM_DRIVE:
    uiState.params[id].drive = value;
    break;
  case VoiceParam::PARAM_SNAP:
    uiState.params[id].snap = value;
    break;
  case VoiceParam::PARAM_HARMONICS:
    uiState.params[id].harmonics = value;
    break;
  default:
    break;
  }

  publishUiState();
}

void VoiceManager::trigger(VoiceID id, float velocity) {
  enqueueEvent({id, VoiceEventType::TRIGGER, velocity, 0.0f});
}

void VoiceManager::triggerFreq(VoiceID id, float freq, float velocity) {
  enqueueEvent({id, VoiceEventType::TRIGGER_FREQ, velocity, freq});
}

void VoiceManager::choke(VoiceID id) {
  enqueueEvent({id, VoiceEventType::CHOKE, 0.0f, 0.0f});
}

void VoiceManager::syncParams() {
  const uint8_t publishedIndex =
      publishedStateIndex.load(std::memory_order_acquire);
  if (publishedIndex == syncedStateIndex)
    return;

  audioState = publishedStates[publishedIndex];
  syncedStateIndex = publishedIndex;
  applyAudioState();
}

void VoiceManager::applyAudioState() {
  KickParams kp;
  kp.tune = audioState.params[VOICE_KICK].pitch;
  kp.length = audioState.params[VOICE_KICK].decay;
  kp.punch = audioState.params[VOICE_KICK].timbre;
  kp.drive = audioState.params[VOICE_KICK].drive;
  kick.setParams(kp);

  SnareParams sp;
  sp.tone = audioState.params[VOICE_SNARE].pitch;
  sp.decay = audioState.params[VOICE_SNARE].decay;
  sp.timbre = audioState.params[VOICE_SNARE].timbre;
  sp.mode = audioState.params[VOICE_SNARE].mode;
  snare.setParams(sp);

  HatsParams hc;
  hc.decay = audioState.params[VOICE_HAT_C].decay;
  hc.timbre = audioState.params[VOICE_HAT_C].timbre;
  hc.open = false;
  hatC.setParams(hc);

  HatsParams ho;
  ho.decay = audioState.params[VOICE_HAT_O].decay;
  ho.timbre = audioState.params[VOICE_HAT_O].timbre;
  ho.open = true;
  hatO.setParams(ho);

  BassParams bp;
  bp.freq = 0.0f;
  bp.release = audioState.params[VOICE_BASS].decay;
  bp.brightness = audioState.params[VOICE_BASS].timbre;
  bp.harmonics = audioState.params[VOICE_BASS].harmonics;
  bp.drive = audioState.params[VOICE_BASS].drive;
  bp.snap = audioState.params[VOICE_BASS].snap;
  bass.setParams(bp);
}

bool VoiceManager::enqueueEvent(const VoiceEvent &event) {
  const uint8_t writeIndex = evtWrite.load(std::memory_order_relaxed);
  const uint8_t nextIndex = advanceEventIndex(writeIndex);
  const uint8_t readIndex = evtRead.load(std::memory_order_acquire);

  if (nextIndex == readIndex)
    return false;

  eventQueue[writeIndex] = event;
  evtWrite.store(nextIndex, std::memory_order_release);
  return true;
}

uint8_t VoiceManager::advanceEventIndex(uint8_t index) const {
  return (uint8_t)((index + 1) % EVENT_QUEUE_SIZE);
}

void VoiceManager::handleEvent(const VoiceEvent &e) {
  switch (e.type) {
  case VoiceEventType::TRIGGER:
    switch (e.id) {
    case VOICE_KICK:
      kick.trigger(e.velocity);
      break;
    case VOICE_SNARE:
      snare.trigger(e.velocity);
      break;
    case VOICE_HAT_C:
      hatC.trigger(e.velocity);
      break;
    case VOICE_HAT_O:
      hatO.trigger(e.velocity);
      hatC.choke();
      break;
    case VOICE_BASS:
      bass.trigger(e.velocity);
      break;
    default:
      break;
    }
    break;

  case VoiceEventType::TRIGGER_FREQ:
    if (e.id == VOICE_BASS) {
      bass.trigger(e.freq, e.velocity);
    } else {
      handleEvent({e.id, VoiceEventType::TRIGGER, e.velocity, 0.0f});
    }
    break;

  case VoiceEventType::SET_FREQ:
    if (e.id == VOICE_BASS) {
      bass.setFreq(e.freq);
    }
    break;

  case VoiceEventType::CHOKE:
    if (e.id == VOICE_HAT_C) {
      hatC.choke();
    } else if (e.id == VOICE_HAT_O) {
      hatO.choke();
    }
    break;
  }
}

bool VoiceManager::isVoiceActive(VoiceID id) {
  if (id == VOICE_BASS)
    return bass.isActive();
  return false;
}

void VoiceManager::setFreq(VoiceID id, float freq) {
  enqueueEvent({id, VoiceEventType::SET_FREQ, 0.0f, freq});
}

void VoiceManager::setMasterVolume(float vol) {
  if (vol < 0.0f)
    vol = 0.0f;
  if (vol > 1.0f)
    vol = 1.0f;

  uiState.masterVolume = vol;
  publishUiState();
}

void VoiceManager::setMasterDrive(float drive) {
  if (drive < 0.0f)
    drive = 0.0f;
  if (drive > 1.0f)
    drive = 1.0f;

  uiState.masterDrive = drive;
  publishUiState();
}

void VoiceManager::setVoiceGain(VoiceID id, float gain) {
  if (id >= VOICE_COUNT)
    return;

  if (gain < 0.0f)
    gain = 0.0f;
  if (gain > 1.2f)
    gain = 1.2f;

  uiState.voiceGains[id] = gain;
  publishUiState();
}

void VoiceManager::publishUiState() {
  publishedStates[stagingStateIndex] = uiState;
  publishedStateIndex.store(stagingStateIndex, std::memory_order_release);
  stagingStateIndex ^= 1;
}

float VoiceManager::process() {
  uint8_t readIndex = evtRead.load(std::memory_order_relaxed);
  uint8_t writeIndex = evtWrite.load(std::memory_order_acquire);
  while (readIndex != writeIndex) {
    handleEvent(eventQueue[readIndex]);
    readIndex = advanceEventIndex(readIndex);
    evtRead.store(readIndex, std::memory_order_release);
    writeIndex = evtWrite.load(std::memory_order_acquire);
  }

  float k = kick.process() * audioState.voiceGains[VOICE_KICK];
  float s = snare.process() * audioState.voiceGains[VOICE_SNARE];
  float hc = hatC.process() * audioState.voiceGains[VOICE_HAT_C];
  float ho = hatO.process() * audioState.voiceGains[VOICE_HAT_O];

  float kickEnv = kick.getEnvelope();
  float ducking = 1.0f - (kickEnv * 0.6f);
  if (ducking < 0.0f)
    ducking = 0.0f;

  float b = bass.process() * audioState.voiceGains[VOICE_BASS];
  b *= ducking;

  float mix = k + s + hc + ho + b;
  float driveGain = 1.0f + (audioState.masterDrive * 2.0f);
  mix *= driveGain;

  return mix * audioState.masterVolume;
}
