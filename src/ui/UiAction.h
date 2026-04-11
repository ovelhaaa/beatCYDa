#ifndef UIACTION_H
#define UIACTION_H

#include <Arduino.h> // Ensure types widely available
#include <stdint.h>

enum class UiMode {
  PERFORMANCE,  // Home: Mutes, Global Transport
  PATTERN_EDIT, // Seq: Steps/Hits/Rot | Bass: Gen Params
  SOUND_EDIT,   // DSP: Tune/Decay/etc

  // Legacy / Context Specific (May be removed or accessed via deep menus)
  JAM,
  EUCLID_EDIT,
  BASS_EDIT,
  SOUND,
  PREFS_VIEW,
  MIXER,
  SYSTEM
};

enum class UiActionType {
  // Navigation / Mode
  CHANGE_MODE,  // value = UiMode
  SELECT_TRACK, // value = track index

  // Hardware Interactions (SIMPLIFIED)
  ENC_ROTATE, // value = delta
  ENC_CLICK,
  ENC_LONG,
  BTN_CLICK,
  BTN_LONG,

  // Transport
  TOGGLE_MUTE, // value = track index
  TOGGLE_PLAY, // value = 0 (toggle), 1 (play), 2 (stop)
  NUDGE_BPM,   // value = delta
  SET_BPM,     // value = absolute bpm
  SAVE_SLOT,   // index = slot
  LOAD_SLOT,   // index = slot

  // Parameter Sets (Absolute - primarily for BLE/Web)
  SET_STEPS,      // index = track, value = count
  SET_HITS,       // index = track, value = count
  SET_ROTATION,   // index = track, value = shift
  SET_BASS_PARAM, // index = paramId, value = val
  SET_SOUND_PARAM, // index = paramId, value = val
  SET_VOICE_GAIN, // index = track, value = gain (0-100)
  RANDOMIZE_TRACK // index = track, value ignored
};

enum class BassParamId : uint8_t {
  DENSITY = 0, // Legacy
  RANGE = 1,   // Legacy
  SCALE = 2,   // Legacy
  ROOT = 3,    // Legacy
  // IDs dedicados para fluxo de edição moderno (PatternScreen), sem colidir
  // com os índices legados usados em páginas antigas do SoundScreen.
  MOTIF_INDEX = 11,
  SWING = 12,
  GHOST_PROB = 13,
  ACCENT_PROB = 14
};

struct UiAction {
  UiActionType type;
  int index; // Track index, Param ID, etc.
  int value; // Value payload

  // Helper Constructors
  static UiAction ChangeMode(int mode) {
    return {UiActionType::CHANGE_MODE, 0, mode};
  }
  static UiAction SelectTrack(int track) {
    return {UiActionType::SELECT_TRACK, 0, track};
  }
  static UiAction ToggleMute(int track) {
    return {UiActionType::TOGGLE_MUTE, 0, track};
  }
  static UiAction TogglePlay() { return {UiActionType::TOGGLE_PLAY, 0, 0}; }

  static UiAction Rotate(int delta) {
    return {UiActionType::ENC_ROTATE, 0, delta};
  }
  static UiAction Click() { return {UiActionType::ENC_CLICK, 0, 0}; }
  static UiAction Hold() { return {UiActionType::ENC_LONG, 0, 0}; }
};

#endif
