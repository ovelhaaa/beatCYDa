#include "MidiOutEngine.h"
#include <math.h>

namespace {
constexpr uint8_t MIDI_STATUS_NOTE_OFF = 0x80;
constexpr uint8_t MIDI_STATUS_NOTE_ON = 0x90;
constexpr uint8_t MIDI_STATUS_CC = 0xB0;
constexpr uint8_t MIDI_STATUS_START = 0xFA;
constexpr uint8_t MIDI_STATUS_STOP = 0xFC;
constexpr uint8_t MIDI_CC_ALL_NOTES_OFF = 123;
}

void MidiOutEngine::init() {
  portENTER_CRITICAL(&queueMux);
  readIndex = 0;
  writeIndex = 0;
  sequenceCounter = 0;
  bassNoteActive = false;
  activeBassNote = 36;
  activeBassGateMs = 0.0f;
  portEXIT_CRITICAL(&queueMux);
}

void MidiOutEngine::setTarget(MidiOutputTarget *newTarget) {
  portENTER_CRITICAL(&queueMux);
  target = newTarget;
  portEXIT_CRITICAL(&queueMux);
}

void MidiOutEngine::sendTransportStart() {
  emitMessage(MidiMessageType::START, MIDI_STATUS_START, 0, 0, 1, VOICE_COUNT);
}

void MidiOutEngine::sendTransportStop() {
  emitMessage(MidiMessageType::STOP, MIDI_STATUS_STOP, 0, 0, 1, VOICE_COUNT);
}

void MidiOutEngine::triggerDrum(VoiceID voice, float velocity) {
  uint8_t note = drumNoteForVoice(voice);
  if (note == 0)
    return;

  emitMessage(MidiMessageType::NOTE_ON, MIDI_STATUS_NOTE_ON | MIDI_CHANNEL_DRUMS,
              note, clampVelocity(velocity), 3, voice);
}

void MidiOutEngine::triggerBass(uint8_t midiNote, float velocity, float gateMs) {
  bool releasePrevious = false;
  uint8_t previousNote = 0;
  float normalizedGateMs = gateMs;

  if (normalizedGateMs < 30.0f)
    normalizedGateMs = 30.0f;
  if (normalizedGateMs > 2000.0f)
    normalizedGateMs = 2000.0f;

  portENTER_CRITICAL(&queueMux);
  if (bassNoteActive) {
    releasePrevious = true;
    previousNote = activeBassNote;
  }
  bassNoteActive = true;
  activeBassNote = midiNote;
  activeBassGateMs = normalizedGateMs;
  portEXIT_CRITICAL(&queueMux);

  if (releasePrevious) {
    emitMessage(MidiMessageType::NOTE_OFF,
                MIDI_STATUS_NOTE_OFF | MIDI_CHANNEL_BASS, previousNote, 0, 3,
                VOICE_BASS);
  }

  emitMessage(MidiMessageType::NOTE_ON, MIDI_STATUS_NOTE_ON | MIDI_CHANNEL_BASS,
              midiNote, clampVelocity(velocity), 3, VOICE_BASS);
}

void MidiOutEngine::noteOffVoice(VoiceID voice) {
  if (voice == VOICE_BASS) {
    releaseBass();
    return;
  }

  uint8_t note = drumNoteForVoice(voice);
  if (note == 0)
    return;

  emitMessage(MidiMessageType::NOTE_OFF,
              MIDI_STATUS_NOTE_OFF | MIDI_CHANNEL_DRUMS, note, 0, 3, voice);
}

void MidiOutEngine::releaseBass() {
  bool shouldRelease = false;
  uint8_t note = 0;

  portENTER_CRITICAL(&queueMux);
  if (bassNoteActive) {
    shouldRelease = true;
    note = activeBassNote;
    bassNoteActive = false;
    activeBassGateMs = 0.0f;
  }
  portEXIT_CRITICAL(&queueMux);

  if (shouldRelease) {
    emitMessage(MidiMessageType::NOTE_OFF,
                MIDI_STATUS_NOTE_OFF | MIDI_CHANNEL_BASS, note, 0, 3,
                VOICE_BASS);
  }
}

void MidiOutEngine::allNotesOff() {
  releaseBass();
  emitMessage(MidiMessageType::CONTROL_CHANGE,
              MIDI_STATUS_CC | MIDI_CHANNEL_BASS, MIDI_CC_ALL_NOTES_OFF, 0, 3,
              VOICE_BASS);
  emitMessage(MidiMessageType::CONTROL_CHANGE,
              MIDI_STATUS_CC | MIDI_CHANNEL_DRUMS, MIDI_CC_ALL_NOTES_OFF, 0, 3,
              VOICE_COUNT);
}

void MidiOutEngine::process(float dtMs) {
  bool shouldRelease = false;
  uint8_t note = 0;

  if (dtMs <= 0.0f)
    return;

  portENTER_CRITICAL(&queueMux);
  if (bassNoteActive) {
    activeBassGateMs -= dtMs;
    if (activeBassGateMs <= 0.0f) {
      shouldRelease = true;
      note = activeBassNote;
      bassNoteActive = false;
      activeBassGateMs = 0.0f;
    }
  }
  portEXIT_CRITICAL(&queueMux);

  if (shouldRelease) {
    emitMessage(MidiMessageType::NOTE_OFF,
                MIDI_STATUS_NOTE_OFF | MIDI_CHANNEL_BASS, note, 0, 3,
                VOICE_BASS);
  }
}

bool MidiOutEngine::popMessage(MidiMessage &outMessage) {
  bool hasMessage = false;

  portENTER_CRITICAL(&queueMux);
  if (readIndex != writeIndex) {
    outMessage = messageQueue[readIndex];
    readIndex = (readIndex + 1) % MESSAGE_QUEUE_SIZE;
    hasMessage = true;
  }
  portEXIT_CRITICAL(&queueMux);

  return hasMessage;
}

size_t MidiOutEngine::pendingCount() const {
  size_t count = 0;

  portENTER_CRITICAL(&queueMux);
  if (writeIndex >= readIndex) {
    count = writeIndex - readIndex;
  } else {
    count = (MESSAGE_QUEUE_SIZE - readIndex) + writeIndex;
  }
  portEXIT_CRITICAL(&queueMux);

  return count;
}

void MidiOutEngine::emitMessage(MidiMessageType type, uint8_t status,
                                uint8_t data1, uint8_t data2, uint8_t length,
                                VoiceID sourceVoice) {
  MidiMessage message;
  message.type = type;
  message.status = status;
  message.data1 = data1;
  message.data2 = data2;
  message.length = length;
  message.sourceVoice = sourceVoice;

  MidiOutputTarget *currentTarget = nullptr;

  portENTER_CRITICAL(&queueMux);
  message.sequence = sequenceCounter++;

  size_t nextWriteIndex = (writeIndex + 1) % MESSAGE_QUEUE_SIZE;
  if (nextWriteIndex == readIndex) {
    readIndex = (readIndex + 1) % MESSAGE_QUEUE_SIZE;
  }

  messageQueue[writeIndex] = message;
  writeIndex = nextWriteIndex;
  currentTarget = target;
  portEXIT_CRITICAL(&queueMux);

  if (currentTarget != nullptr) {
    currentTarget->handleMidiMessage(message);
  }
}

uint8_t MidiOutEngine::clampVelocity(float velocity) const {
  float clamped = velocity;
  if (clamped < 0.0f)
    clamped = 0.0f;
  if (clamped > 1.0f)
    clamped = 1.0f;

  return (uint8_t)lroundf(1.0f + (clamped * 126.0f));
}

uint8_t MidiOutEngine::drumNoteForVoice(VoiceID voice) const {
  switch (voice) {
  case VOICE_KICK:
    return 36;
  case VOICE_SNARE:
    return 38;
  case VOICE_HAT_C:
    return 42;
  case VOICE_HAT_O:
    return 46;
  case VOICE_BASS:
  case VOICE_COUNT:
  default:
    return 0;
  }
}
