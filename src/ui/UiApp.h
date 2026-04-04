#pragma once

#include "LgfxDisplay.h"
#include "components/UiToast.h"
#include "core/UiInvalidation.h"
#include "screens/PerformScreen.h"

namespace ui {

class UiApp {
public:
  bool begin();
  void runFrame(uint32_t nowMs);

private:
  LgfxDisplay _display;
  UiInvalidation _invalidation;
  TouchPoint _touch;
  PerformScreen _performScreen;
  UiToast _toast{"Sprint 3 ativo", UiToastSeverity::Info, 1400, {12, 170, 180, 24}};
  bool _toastVisible{true};
  uint32_t _toastUntil{0};
};

} // namespace ui
