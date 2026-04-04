#pragma once

#include "LgfxDisplay.h"
#include "core/UiInvalidation.h"

namespace ui {

class UiApp {
public:
  bool begin();
  void runFrame(uint32_t nowMs);

private:
  void renderSmokeTest(uint32_t nowMs);

  LgfxDisplay _display;
  UiInvalidation _invalidation;
  TouchPoint _touch;
};

} // namespace ui
