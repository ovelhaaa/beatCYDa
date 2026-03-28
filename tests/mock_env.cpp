#include "VoiceManager.h"
#include "Engine.h"

VoiceManager voiceManager;
Engine engine;

bool VoiceManager::isVoiceActive(VoiceID) { return false; }
void VoiceManager::setFreq(VoiceID, float) {}
void VoiceManager::triggerFreq(VoiceID, float, float) {}

void MidiOutEngine::triggerBass(uint8_t, float, float) {}
Engine::Engine() {}
