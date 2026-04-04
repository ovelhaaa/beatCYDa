#pragma once

#include "../components/UiButton.h"
#include "../components/UiCard.h"
#include "../Display.h"
#include "../components/UiChip.h"
#include "IScreen.h"

struct UiStateSnapshot;

namespace ui {

class PerformScreen : public IScreen {
public:
  PerformScreen();

  void layout() override;
  void render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) override;
  bool handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) override;
  void invalidate() override;
  UiScreenId id() const override { return UiScreenId::Perform; }

private:
  bool _dirty{true};
  UiButton _playButton{};
  UiButton _muteButton{};
  UiCard _statusCard{};
  UiChip _trackChips[TRACK_COUNT]{};
};

} // namespace ui
