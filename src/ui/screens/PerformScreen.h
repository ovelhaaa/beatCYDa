#pragma once

#include "../components/UiButton.h"
#include "../components/UiCard.h"
#include "../Display.h"
#include "../components/UiChip.h"
#include "../components/UiEuclideanRings.h"
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
  bool _fullDirty{true};
  bool _hasFrame{false};
  bool _ringsDirty{true};
  bool _controlsDirty{true};
  bool _trackStripDirty{true};
  bool _bpmDirty{true};
  bool _lastPlaying{false};
  uint16_t _lastBpm{0};
  uint8_t _lastActiveTrack{0};
  bool _lastTrackMutes[TRACK_COUNT]{};
  bool _trackChipDirty[TRACK_COUNT]{};
  UiButton _playButton{};
  UiButton _muteButton{};
  UiCard _statusCard{};
  UiChip _trackChips[TRACK_COUNT]{};
  UiRect _ringsRect{};
  UiEuclideanRings _rings{};
};

} // namespace ui
