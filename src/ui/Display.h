#pragma once

#include "../audio/Engine.h"
#include "../control/InputManager.h"
#include "LGFX_CYD.h"
struct UiStateSnapshot {
  int bpm = 120;
  bool isPlaying = true;
  int currentStep = 0;
  int activeTrack = 0;
  UiMode mode = UiMode::PATTERN_EDIT;
  bool trackMutes[TRACK_COUNT] = {};
  int trackSteps[TRACK_COUNT] = {};
  int trackHits[TRACK_COUNT] = {};
  int trackRotations[TRACK_COUNT] = {};
  uint8_t patterns[TRACK_COUNT][64] = {};
  int patternLens[TRACK_COUNT] = {};
  VoiceParams voiceParams[TRACK_COUNT] = {};
  float voiceGain[TRACK_COUNT] = {};
  float masterVolume = 0.5f;
  BassGrooveParams bassParams = {};

  void capture();
};

void displayTask(void *parameter);
