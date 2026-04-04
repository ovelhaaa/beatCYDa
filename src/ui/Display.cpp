#include "Display.h"
#include "LGFX_CYD.h"
#include "core/UiState.h"
#include "input/UiTouchRouter.h"
#include "render/LegacyRender.h"

#include "UiApp.h"
#include "../Config.h"
#include "../control/ControlManager.h"
#include "../control/InputManager.h"
#include <SPI.h>
#include <esp_system.h>
#include <string.h>

LGFX_CYD tft;

namespace {
void processStorageFeedback(UiRuntime &ui, uint32_t now) {
  ControlManager::StorageEvent event{};
  while (CtrlMgr.pollStorageEvent(event)) {
    ui.storageOpInProgress = false;
    ui.storageOpDeadlineMs = 0;

    if (event.type == ControlManager::StorageEventType::SAVE_SLOT_DONE) {
      setStatus(ui, event.success ? "Saved!" : "Save Fail");
    } else if (event.type == ControlManager::StorageEventType::LOAD_SLOT_DONE) {
      setStatus(ui, event.success ? "Loaded!" : "Load Fail");
      ui.invalidation.markAll();
    }
  }

  if (ui.storageOpInProgress && ui.storageOpDeadlineMs != 0 && now >= ui.storageOpDeadlineMs) {
    ui.storageOpInProgress = false;
    ui.storageOpDeadlineMs = 0;
    setStatus(ui, "Storage timeout");
    ui.invalidation.toastDirty = true;
  }
}

void updateFrameMetrics(UiRuntime &ui, uint32_t renderMicros) {
  ui.metrics.frameCount++;
  ui.metrics.lastRenderMicros = renderMicros;
  ui.metrics.lastFreeHeap = esp_get_free_heap_size();

  if ((ui.metrics.frameCount % 60u) == 0u) {
    Serial.printf("[UI] frame=%lu render_us=%lu free_heap=%lu\n",
                  static_cast<unsigned long>(ui.metrics.frameCount),
                  static_cast<unsigned long>(ui.metrics.lastRenderMicros),
                  static_cast<unsigned long>(ui.metrics.lastFreeHeap));
  }
}
} // namespace

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

  if (UseNewUi) {
    ui::UiApp app;
    if (!app.begin()) {
      Serial.println("[UI] New UI init failed, halting display task");
      vTaskDelete(nullptr);
      return;
    }

    for (;;) {
      app.runFrame(millis());
      vTaskDelay(pdMS_TO_TICKS(16));
    }
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
    processStorageFeedback(ui, now);
    ui.mode = ui.snapshot.mode;
    InputMgr.update();
    handleTouch(ui, InputMgr.state());

    // update
    tickHold(ui);

    // render
    const uint32_t renderStart = micros();
    renderLegacyFrame(ui, now, lastFull, lastRing);
    updateFrameMetrics(ui, micros() - renderStart);
    commitFrame(ui);

    vTaskDelay(pdMS_TO_TICKS(16));
  }
}
