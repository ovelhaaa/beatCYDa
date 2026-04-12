#pragma once

#include "../components/UiButton.h"
#include "../components/UiCard.h"
#include "../components/UiChip.h"
#include "../components/UiMacroRow.h"
#include "../components/UiEuclideanRings.h"
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
  void copyActiveTrack(const UiStateSnapshot &snapshot);
  void pasteToActiveTrack(const UiStateSnapshot &snapshot);

  struct PatternClipboard {
    uint8_t steps{16};
    uint8_t hits{4};
    int8_t rotation{0};
    uint8_t gainPercent{100};
    bool hasData{false};
  };

  bool _dirty{true};
  bool _hasLastSnapshot{false};
  UiStateSnapshot _lastSnapshot{};
  UiCard _headerCard{};
  UiChip _trackChips[TRACK_COUNT]{};
  UiMacroRow _rows[4]{};
  UiButton _randomButton{};
  UiButton _clearButton{};
  UiButton _copyButton{};
  UiButton _pasteButton{};
  UiButton _toolsButton{};
  UiRect _toolsModalRect{};
  bool _toolsModalVisible{false};
  UiEuclideanRings _ringsPreview{};
  PatternClipboard _clipboard{};

  int _holdRow{-1};
  int _holdDirection{0};
  int _lastHoldRow{-1};
  int _lastHoldDirection{0};
  uint32_t _holdNextTickMs{0};
  uint8_t _holdTickCount{0};
};

} // namespace ui
