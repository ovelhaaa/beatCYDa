#pragma once

#include "Display.h"
#include "LgfxDisplay.h"
#include "components/UiButton.h"
#include "core/UiInvalidation.h"
#include "core/UiScreenId.h"
#include "screens/PerformScreen.h"
#include "screens/PatternScreen.h"
#include "screens/SoundScreen.h"
#include "screens/MixScreen.h"
#include "screens/ProjectScreen.h"

namespace ui {

class UiApp {
public:
  UiApp();
  bool begin();
  void runFrame(uint32_t nowMs);

private:
  void renderChrome(uint32_t nowMs);
  void renderBottomNav();
  void handleBottomNavTouch();
  void updateUiStats(uint32_t nowMs);
  bool detectModelChanges();
  IScreen *activeScreen();

  LgfxDisplay _display;
  TouchPoint _touch;
  UiStateSnapshot _snapshot;
  UiStateSnapshot _previousSnapshot;
  bool _hasPreviousSnapshot{false};
  UiScreenId _activeScreen{UiScreenId::Perform};
  UiScreenId _previousScreen{UiScreenId::Perform};
  UiInvalidation _invalidation{};
  uint32_t _statsLastMs{0};
  uint32_t _frameCounter{0};
  uint16_t _uiFps{0};
  uint32_t _freeHeap{0};

  UiButton _navPerform{};
  UiButton _navPattern{};
  UiButton _navSound{};
  UiButton _navMix{};
  UiButton _navProject{};

  PerformScreen _performScreen{};
  PatternScreen _patternScreen{};
  SoundScreen _soundScreen{};
  MixScreen _mixScreen{};
  ProjectScreen _projectScreen{};
};

} // namespace ui
