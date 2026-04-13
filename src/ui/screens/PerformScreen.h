#pragma once

#include "../components/UiButton.h"
#include "../Display.h"
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
  bool _playDirty{true};
  bool _muteDirty{true};
  bool _bpmDirty{true};
  bool _trackCarouselDirty{true};
  bool _lastPlaying{false};
  int _snapshotStep{-1};
  uint16_t _lastBpm{0};
  uint8_t _lastActiveTrack{0};
  bool _lastTrackMutes[TRACK_COUNT]{};
  UiButton _playButton{};
  UiButton _muteButton{};
  UiButton _bpmMinusButton{};
  UiButton _bpmPlusButton{};
  UiButton _trackPrevButton{};
  UiButton _trackNextButton{};
  bool _playPressed{false};
  bool _mutePressed{false};
  bool _bpmMinusPressed{false};
  bool _bpmPlusPressed{false};
  bool _trackPrevPressed{false};
  bool _trackNextPressed{false};
  UiRect _heroCardRect{};
  UiRect _bpmValueRect{};
  UiRect _trackLabelRect{};
  UiRect _ringsRect{};
  UiEuclideanRings _rings{};
};

} // namespace ui
