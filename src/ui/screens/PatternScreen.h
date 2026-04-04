#pragma once

#include "../components/UiButton.h"
#include "../components/UiCard.h"
#include "../components/UiChip.h"
#include "../components/UiMacroRow.h"
#include "../Display.h"
#include "IScreen.h"

struct UiStateSnapshot;

namespace ui {

class PatternScreen : public IScreen {
public:
  PatternScreen();

  void layout() override;
  void render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) override;
  bool handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) override;
  void invalidate() override;
  UiScreenId id() const override { return UiScreenId::Pattern; }

private:
  void startHold(int rowIndex, int direction);
  void stopHold();
  bool handleHoldTick(const TouchPoint &tp, const UiStateSnapshot &snapshot);
  void dispatchRowDelta(const UiStateSnapshot &snapshot, int rowIndex, int amount);

  bool _dirty{true};
  UiCard _headerCard{};
  UiChip _trackChips[TRACK_COUNT]{};
  UiMacroRow _rows[4]{};
  UiButton _randomButton{};
  UiButton _clearButton{};

  int _holdRow{-1};
  int _holdDirection{0};
  uint32_t _holdNextTickMs{0};
  uint8_t _holdTickCount{0};
};

} // namespace ui
