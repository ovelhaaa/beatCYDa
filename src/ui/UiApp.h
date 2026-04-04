#pragma once

#include "LgfxDisplay.h"
#include "components/UiButton.h"
#include "components/UiCard.h"
#include "components/UiChip.h"
#include "components/UiMacroRow.h"
#include "components/UiModal.h"
#include "components/UiToast.h"
#include "core/UiInvalidation.h"

namespace ui {

class UiApp {
public:
  UiApp();
  bool begin();
  void runFrame(uint32_t nowMs);

private:
  void renderSmokeTest(uint32_t nowMs);

  LgfxDisplay _display;
  UiInvalidation _invalidation;
  TouchPoint _touch;

  UiButton _playButton{};
  UiCard _statusCard{};
  UiChip _trackChip{};
  UiMacroRow _macroRow{};
  UiToast _toast{};
  UiModal _modal{};
};

} // namespace ui
