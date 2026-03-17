#ifndef VOICE_MANAGER_H
#define VOICE_MANAGER_H

#include "../../include/IPC.h"
#include "BassVoice.h"
#include "HatsVoice.h"
#include "KickVoice.h"
#include "SnareVoice.h"
#include <Arduino.h>
#include <atomic>
#include <stdint.h>

enum VoiceID : uint8_t {
  VOICE_KICK = 0,
  VOICE_SNARE,
  VOICE_HAT_C,
  VOICE_HAT_O,
  VOICE_BASS,
  VOICE_COUNT
};

struct VoiceParams {
  float pitch;  // 0..1 (Freq/Tone)
  float decay;  // 0..1
  float timbre; // 0..1
  uint8_t mode; // Custom mode (Snare: 0,1,2)
  // Extended Params (Bass)
  float drive;
  float snap;
  float harmonics;
};

enum class VoiceEventType : uint8_t {
  TRIGGER,
  TRIGGER_FREQ,
  SET_FREQ,
  CHOKE,
};

struct VoiceEvent {
  VoiceID id;
  VoiceEventType type;
  float velocity;
  float freq; // For Bass
};

struct VoiceStateSnapshot {
  VoiceParams params[VOICE_COUNT];
  float voiceGains[VOICE_COUNT];
  float masterVolume;
  float masterDrive;
};

class VoiceManager {
public:
  void init(float sampleRate);

  // ==== Control Thread (UI / Sequencer) ====
  // These are thread-safe (write to double buffer or queue)
  void setParams(VoiceID id, const VoiceParams &p);
  void setParam(VoiceID id, VoiceParam param, float value);
  VoiceParams getParams(VoiceID id) const {
    return (id < VOICE_COUNT) ? uiState.params[id] : VoiceParams{};
  }
  void trigger(VoiceID id, float velocity = 1.0f);
  void triggerFreq(VoiceID id, float freq, float velocity = 1.0f);

  // Legato / Slide support
  void setFreq(VoiceID id, float freq);
  bool isVoiceActive(VoiceID id);

  void choke(VoiceID id); // Not strictly in user request but useful? User had
                          // choke in audio logic.
  // User request: "void choke(VoiceID id);" in class definition.

  // ==== Audio Thread ====
  // Call once per buffer
  void syncParams();
  // Call per sample
  float process();

  void setMasterVolume(float vol);
  float getMasterVolume() const { return uiState.masterVolume; }

  void setMasterDrive(float drive);
  float getMasterDrive() const { return uiState.masterDrive; }

  void setVoiceGain(VoiceID id, float gain);
  float getVoiceGain(VoiceID id) const {
    return (id < VOICE_COUNT) ? uiState.voiceGains[id] : 0.0f;
  }

private:
  // DSP instances
  KickVoice kick;
  SnareVoice snare;
  HatsVoice hatC;
  HatsVoice hatO;
  BassVoice bass;

  // ==== Parameter Publication ====
  VoiceStateSnapshot uiState;
  VoiceStateSnapshot publishedStates[2];
  VoiceStateSnapshot audioState;
  std::atomic<uint8_t> publishedStateIndex{0};
  uint8_t stagingStateIndex = 1;
  uint8_t syncedStateIndex = 0xFF;

  // ==== Event Queue (SPSC) ====
  static constexpr int EVENT_QUEUE_SIZE = 64; // Increased for safety
  VoiceEvent eventQueue[EVENT_QUEUE_SIZE];
  std::atomic<uint8_t> evtWrite{0};
  std::atomic<uint8_t> evtRead{0};

  // Helpers
  void publishUiState();
  void applyAudioState();
  bool enqueueEvent(const VoiceEvent &event);
  uint8_t advanceEventIndex(uint8_t index) const;
  void handleEvent(const VoiceEvent &e);
};

extern VoiceManager voiceManager;

#endif
