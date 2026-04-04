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
  bool _dirty{true};
  int8_t _activeFader{-1};
  UiCard _headerCard{};
  UiFader _faders[TRACK_COUNT]{};
};

} // namespace ui
