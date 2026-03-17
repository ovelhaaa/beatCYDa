#include <Arduino.h>
#include <esp_pm.h>

#include "audio/AudioTask.h"
#include "audio/Engine.h"
#include "control/ControlManager.h"
#include "storage/PatternStorage.h"
#include "ui/Display.h"

namespace {
void setupPower() {
  setCpuFrequencyMhz(240);
  esp_pm_config_esp32_t pmConfig = {};
  pmConfig.max_freq_mhz = 240;
  pmConfig.min_freq_mhz = 240;
  pmConfig.light_sleep_enable = false;
  esp_pm_configure(&pmConfig);
}
} // namespace

void setup() {
  Serial.begin(115200, SERIAL_8N1, -1, 1);
  delay(1000);
  Serial.println("\nPENOSA CYD variant boot");

  setupPower();
  CtrlMgr.begin();
  PatternStore.begin();

  engine.init();

  hw_timer_t *timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimerISR, true);
  engine.updateTimerFreq(timer);
  timerAlarmEnable(timer);

  xTaskCreatePinnedToCore(displayTask, "CYD_UI", 12288, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(audioTask, "CYD_Audio", 8192, nullptr, 24, nullptr, 1);
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
