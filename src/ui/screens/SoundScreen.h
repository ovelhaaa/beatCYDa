#pragma once

#include "../components/UiCard.h"
#include "../components/UiChip.h"
#include "../components/UiMacroRow.h"
#include "../Display.h"
#include "IScreen.h"

struct UiStateSnapshot;

namespace ui {

class SoundScreen : public IScreen {
public:
  SoundScreen();

  void layout() override;
  void render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) override;
  bool handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) override;
  void invalidate() override;
  UiScreenId id() const override { return UiScreenId::Sound; }

private:
  void startHold(int rowIndex, int direction);
  void stopHold();
  bool handleHoldTick(const TouchPoint &tp, const UiStateSnapshot &snapshot);
  void dispatchRowDelta(const UiStateSnapshot &snapshot, int rowIndex, int amount);
  void applyRowLabels(const UiStateSnapshot &snapshot);
  bool isBassTrack(const UiStateSnapshot &snapshot) const;
  void applyLayoutMode(bool bassLayout);

  bool _dirty{true};
  bool _hasLastSnapshot{false};
  UiStateSnapshot _lastSnapshot{};
  UiCard _identityCard{};
  UiChip _trackChips[TRACK_COUNT]{};
  UiRect _soundTypeChipRect{};
  UiRect _bassTabRects[3]{};
  UiMacroRow _rows[4]{};
  bool _layoutIsBass{false};

  int _holdRow{-1};
  int _holdDirection{0};
  int _lastHoldRow{-1};
  int _lastHoldDirection{0};
  uint32_t _holdNextTickMs{0};
  uint8_t _holdTickCount{0};
  uint8_t _bassPage{0};
  uint8_t _lastBassPage{0};
};

} // namespace ui
