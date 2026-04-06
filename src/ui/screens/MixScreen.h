#pragma once

#include "../components/UiCard.h"
#include "../components/UiFader.h"
#include "../Display.h"
#include "IScreen.h"

struct UiStateSnapshot;

namespace ui {

class MixScreen : public IScreen {
public:
  MixScreen();

  void layout() override;
  void render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) override;
  bool handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) override;
  void invalidate() override;
  UiScreenId id() const override { return UiScreenId::Mix; }

private:
  bool _fullDirty{true};
  bool _hasFrame{false};
  int8_t _activeFader{-1};
  int8_t _lastCapturedFader{-1};
  uint8_t _lastRenderedFaderValues[TRACK_COUNT]{};
  uint8_t _lastRenderedMaster{0};
  bool _faderDirty[TRACK_COUNT]{};
  bool _masterDirty{true};
  UiCard _headerCard{};
  UiFader _faders[TRACK_COUNT]{};
};

} // namespace ui
