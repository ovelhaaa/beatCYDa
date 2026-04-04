#include "Display.h"
#include "LGFX_CYD.h"
#include "core/UiState.h"
#include "input/UiTouchRouter.h"
#include "render/LegacyRender.h"
#include "../control/InputManager.h"
#include <SPI.h>
#include <string.h>

LGFX_CYD tft;

void UiStateSnapshot::capture() {
  bpm = engine.bpm.load(std::memory_order_relaxed);
  isPlaying = engine.isPlaying.load(std::memory_order_relaxed);
  currentStep = engine.currentStep.load(std::memory_order_relaxed);
  activeTrack = engine.uiActiveTrack.load(std::memory_order_relaxed);
  mode = engine.uiMode.load(std::memory_order_relaxed);
  bassParams = engine.bassGroove.getParams();
  masterVolume = voiceManager.getMasterVolume();

  for (int i = 0; i < TRACK_COUNT; ++i) {
    trackMutes[i] = engine.trackMutes[i].load(std::memory_order_relaxed);
    trackSteps[i] = engine.tracks[i].steps;
    trackHits[i] = engine.tracks[i].hits;
    trackRotations[i] = engine.tracks[i].rotationOffset;
    voiceParams[i] = voiceManager.getParams(static_cast<VoiceID>(i));
    voiceGain[i] = voiceManager.getVoiceGain(static_cast<VoiceID>(i));
  }

  if (engine.lockPattern()) {
    for (int i = 0; i < TRACK_COUNT; ++i) {
      patternLens[i] = min(64, static_cast<int>(engine.tracks[i].patternLen));
      memcpy(patterns[i], engine.tracks[i].pattern, patternLens[i]);
    }
    engine.unlockPattern();
  }
}

void displayTask(void *parameter) {
  while (!engine.engineReady.load()) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  pinMode(CYDConfig::TftBacklight, OUTPUT);
  digitalWrite(CYDConfig::TftBacklight, HIGH);

  tft.init();
  tft.setRotation(CYDConfig::ScreenRotation);
  InputMgr.begin();

  initLegacyRender();

  UiRuntime ui;
  uint32_t lastFull = 0;
  uint32_t lastRing = 0;

  for (;;) {
    const uint32_t now = millis();

    // capture
    ui.snapshot.capture();

    // input
    updateStatusTimeout(ui, now);
    ui.mode = ui.snapshot.mode;
    InputMgr.update();
    handleTouch(ui, InputMgr.state());

    // update
    tickHold(ui);

    // render
    renderLegacyFrame(ui, now, lastFull, lastRing);
    commitFrame(ui);

    vTaskDelay(pdMS_TO_TICKS(16));
  }
}
