#pragma once

#include "Display.h"
#include "LgfxDisplay.h"
#include "components/UiButton.h"
#include "core/UiScreenId.h"
#include "screens/PerformScreen.h"

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

  LgfxDisplay _display;
  TouchPoint _touch;
  UiStateSnapshot _snapshot;
  UiScreenId _activeScreen{UiScreenId::Perform};

  UiButton _navPerform{};
  UiButton _navPattern{};
  UiButton _navSound{};
  UiButton _navMix{};
  UiButton _navProject{};

  PerformScreen _performScreen{};
};

} // namespace ui
