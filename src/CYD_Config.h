#pragma once
#include <Arduino.h>
#include <driver/i2s.h>

#define TRACK_COUNT 5
#define SAMPLE_RATE 48000
#define BUFFER_LEN 256

namespace CYDConfig {
constexpr uint16_t ScreenWidth = 320;
constexpr uint16_t ScreenHeight = 240;
constexpr uint8_t ScreenRotation = 1;

// Stock ESP32-2432S028R pinout. Some board revisions reroute touch/SD.
// The variant keeps everything centralized here so a single board-specific
// edit is enough if your unit differs.
constexpr bool UseSharedSpiWiring = false;

constexpr int TftSclk = 14;
constexpr int TftMiso = 12;
constexpr int TftMosi = 13;
constexpr int TftCs = 15;
constexpr int TftDc = 2;
constexpr int TftRst = -1;
constexpr int TftBacklight = 21;

constexpr int TouchSclk = UseSharedSpiWiring ? TftSclk : 25;
constexpr int TouchMiso = UseSharedSpiWiring ? TftMiso : 39;
constexpr int TouchMosi = UseSharedSpiWiring ? TftMosi : 32;
constexpr int TouchCs = 33;
constexpr int TouchIrq = -1;

constexpr int SdSclk = 18;
constexpr int SdMiso = 19;
constexpr int SdMosi = 23;
constexpr int SdCs = 5;

constexpr bool UseInternalDac = false;
constexpr int InternalAmpPin = 26;
constexpr i2s_port_t AudioPort = I2S_NUM_0;
constexpr int ExtI2SBck = 21;
constexpr int ExtI2SWs = 22;
constexpr int ExtI2SData = 3;

constexpr uint16_t TouchMinX = 240;
constexpr uint16_t TouchMaxX = 3800;
constexpr uint16_t TouchMinY = 3700;
constexpr uint16_t TouchMaxY = 200;

constexpr uint16_t UiFrameMs = 33;
constexpr uint16_t TouchPollMs = 16;
constexpr uint16_t HoldMs = 500;
constexpr uint16_t DragThreshold = 8;

constexpr int PatternSlots = 8;
constexpr const char *PatternsDir = "/patterns";
constexpr const char *SamplesDir = "/samples";
} // namespace CYDConfig
