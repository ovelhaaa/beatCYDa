#ifndef MIDI_OUT_ENGINE_H
#define MIDI_OUT_ENGINE_H

#include "../audio/VoiceManager.h"
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

enum class MidiMessageType : uint8_t {
  NOTE_OFF = 0,
  NOTE_ON,
  CONTROL_CHANGE,
  START,
  STOP,
};

struct MidiMessage {
  MidiMessageType type = MidiMessageType::NOTE_ON;
  uint8_t status = 0;
  uint8_t data1 = 0;
  uint8_t data2 = 0;
  uint8_t length = 0;
  VoiceID sourceVoice = VOICE_COUNT;
  uint32_t sequence = 0;
};

class MidiOutputTarget {
public:
  virtual ~MidiOutputTarget() = default;
  virtual void handleMidiMessage(const MidiMessage &message) = 0;
};

class MidiOutEngine {
public:
  void init();
  void setTarget(MidiOutputTarget *newTarget);

  void sendTransportStart();
  void sendTransportStop();

  void triggerDrum(VoiceID voice, float velocity);
  void triggerBass(uint8_t midiNote, float velocity, float gateMs);
  void noteOffVoice(VoiceID voice);
  void releaseBass();
  void allNotesOff();

  void process(float dtMs);

  bool popMessage(MidiMessage &outMessage);
  size_t pendingCount() const;

private:
  static constexpr size_t MESSAGE_QUEUE_SIZE = 96;
  static constexpr uint8_t MIDI_CHANNEL_BASS = 0;
  static constexpr uint8_t MIDI_CHANNEL_DRUMS = 9;

  MidiMessage messageQueue[MESSAGE_QUEUE_SIZE];
  size_t readIndex = 0;
  size_t writeIndex = 0;
  uint32_t sequenceCounter = 0;
  bool bassNoteActive = false;
  uint8_t activeBassNote = 36;
  float activeBassGateMs = 0.0f;
  MidiOutputTarget *target = nullptr;
  mutable portMUX_TYPE queueMux = portMUX_INITIALIZER_UNLOCKED;

  void emitMessage(MidiMessageType type, uint8_t status, uint8_t data1,
                   uint8_t data2, uint8_t length, VoiceID sourceVoice);
  uint8_t clampVelocity(float velocity) const;
  uint8_t drumNoteForVoice(VoiceID voice) const;
};

#endif
