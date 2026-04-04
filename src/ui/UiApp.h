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
  bool begin();
  void runFrame(uint32_t nowMs);

private:
  void renderSmokeTest(uint32_t nowMs);

  LgfxDisplay _display;
  UiInvalidation _invalidation;
  TouchPoint _touch;

  UiButton _playButton{"PLAY", UiButtonVariant::Primary, false, false, {12, 58, 72, 34}};
  UiCard _statusCard{"STATUS", "NEW UI", true, {92, 58, 104, 58}};
  UiChip _trackChip{0, true, false, true, {204, 58, 56, 34}};
  UiMacroRow _macroRow{"STEPS", "16", {208, 106, 24, 24}, {282, 106, 24, 24}, {92, 100, 216, 38}, true, false, 72};
  UiToast _toast{"Sprint 2 em andamento", UiToastSeverity::Info, 1400, {12, 196, 220, 28}};
  UiModal _modal{"Confirmar", "Salvar projeto?", {"OK", UiButtonVariant::Primary, false, false, {122, 156, 56, 28}},
                 {"X", UiButtonVariant::Secondary, false, false, {184, 156, 56, 28}}, {96, 120, 152, 72}, false};
};

} // namespace ui
