#pragma once

#include <stdint.h>

/**
 * @brief Command Types for IPC between Control (Core 0) and Audio (Core 1)
 */
enum class CommandType {
  NOTE_ON,      // voiceId, pitch (uint8), velocity (float)
  NOTE_OFF,     // voiceId
  SET_PARAM,    // voiceId, paramId, value (float)
  TRANSPORT,    // play/stop/rewind
  GLOBAL_PARAM, // tempo, swing, master volume
  SYSTEM,       // save/load?
  UI_ACTION     // Generic UI Action Wrapper
};

/**
 * @brief Parameter IDs for Voice Synthesis
 * Keeping it generic for now.
 */
enum class VoiceParam {
  PARAM_PITCH,
  PARAM_DECAY,
  PARAM_TIMBRE,
  PARAM_MODE,
  PARAM_LEVEL,
  PARAM_FX_SEND,
  // Bass Specific
  PARAM_DRIVE,
  PARAM_SNAP,
  PARAM_HARMONICS
};

/**
 * @brief POD Structure for lock-free queue
 * Must stay small to fit many in the queue.
 */
struct IPCCommand {
  CommandType type;
  uint8_t voiceId; // 0-15
  uint8_t paramId; // Mapped to VoiceParam or Global Param ID
  float value;     // Main payload
  float auxValue;  // Velocity, Q, etc.

  // Helper constructors
  static IPCCommand NoteOn(uint8_t voice, float pitch, float velocity) {
    return {CommandType::NOTE_ON, voice, 0, pitch, velocity};
  }
  static IPCCommand NoteOff(uint8_t voice) {
    return {CommandType::NOTE_OFF, voice, 0, 0.0f, 0.0f};
  }
  static IPCCommand SetParam(uint8_t voice, VoiceParam param, float val) {
    return {CommandType::SET_PARAM, voice, (uint8_t)param, val, 0.0f};
  }
  static IPCCommand Transport(bool play) {
    return {CommandType::TRANSPORT, 0, 0, play ? 1.0f : 0.0f, 0.0f};
  }
};
