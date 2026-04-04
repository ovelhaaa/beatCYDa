#pragma once

#include "../LgfxDisplay.h"
#include "../components/UiButton.h"
#include "../components/UiCard.h"
#include "../components/UiChip.h"
#include "../components/UiMacroRow.h"
#include "../core/UiInvalidation.h"
#include "../core/UiScreenId.h"

namespace ui {

class PerformScreen {
public:
  void begin();
  void handleTouch(const TouchPoint &touch, UiInvalidation &invalidation);
  void render(lgfx::LGFX_Device &canvas, UiInvalidation &invalidation);

private:
  void drawTopBar(lgfx::LGFX_Device &canvas);
  void drawContent(lgfx::LGFX_Device &canvas);
  void drawBottomNav(lgfx::LGFX_Device &canvas);

  UiScreenId _screen{UiScreenId::Perform};
  uint8_t _activeTrack{0};
  bool _isPlaying{true};

  UiButton _playButton{"PLAY", UiButtonVariant::Primary, false, false, {12, 42, 66, 30}};
  UiButton _muteButton{"MUTE", UiButtonVariant::Secondary, false, false, {84, 42, 66, 30}};
  UiCard _bpmCard{"BPM", "120", true, {156, 42, 72, 30}};
  UiMacroRow _macroRow{"STEPS", "16", {222, 42, 24, 30}, {284, 42, 24, 30}, {232, 42, 48, 30}, false, false, 0};

  UiChip _trackChips[5] = {
      {0, true, false, true, {12, 84, 56, 32}},
      {1, true, false, false, {72, 84, 56, 32}},
      {2, true, false, false, {132, 84, 56, 32}},
      {3, true, false, false, {192, 84, 56, 32}},
      {4, true, false, false, {252, 84, 56, 32}},
  };

  UiButton _navButtons[5] = {
      {"PERF", UiButtonVariant::Primary, false, false, {8, 202, 56, 30}},
      {"PAT", UiButtonVariant::Secondary, false, false, {70, 202, 56, 30}},
      {"SND", UiButtonVariant::Secondary, false, false, {132, 202, 56, 30}},
      {"MIX", UiButtonVariant::Secondary, false, false, {194, 202, 56, 30}},
      {"PRJ", UiButtonVariant::Secondary, false, false, {256, 202, 56, 30}},
  };
};

} // namespace ui
