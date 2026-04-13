#include "PerformScreen.h"

#include "../Display.h"
#include "../core/UiActions.h"
#include "../theme/UiTheme.h"

namespace ui {
namespace {
void setRect(UiRect &rect, int16_t x, int16_t y, int16_t w, int16_t h) {
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;
}

uint8_t clampTrackIndex(int track) {
  if (track < 0) {
    return 0;
  }
  if (track >= TRACK_COUNT) {
    return static_cast<uint8_t>(TRACK_COUNT - 1);
  }
  return static_cast<uint8_t>(track);
}
} // namespace

PerformScreen::PerformScreen() {
  _playButton.label = "▶";
  _playButton.variant = UiButtonVariant::Primary;
  _stopButton.label = "■";
  _stopButton.variant = UiButtonVariant::Secondary;

  _muteButton.label = "MUTE";
  _muteButton.variant = UiButtonVariant::Secondary;

  _bpmMinusButton.label = "-";
  _bpmMinusButton.variant = UiButtonVariant::Secondary;
  _bpmPlusButton.label = "+";
  _bpmPlusButton.variant = UiButtonVariant::Secondary;

  _rotateMinusButton.label = "<";
  _rotateMinusButton.variant = UiButtonVariant::Secondary;
  _rotatePlusButton.label = ">";
  _rotatePlusButton.variant = UiButtonVariant::Secondary;

  _rings.setCompact(false);
  _rings.setInteractive(true);
  _rings.setSingleTrack(false);

  layout();
}

void PerformScreen::layout() {
  setRect(_heroCardRect,
          theme::UiTheme::Metrics::PerformHeroCardX,
          theme::UiTheme::Metrics::PerformHeroCardY,
          theme::UiTheme::Metrics::PerformHeroCardW,
          theme::UiTheme::Metrics::PerformHeroCardH);

  setRect(_playButton.rect,
          theme::UiTheme::Metrics::PerformControlsX,
          theme::UiTheme::Metrics::PerformPlayY,
          theme::UiTheme::Metrics::PerformControlW / 2 - 1,
          theme::UiTheme::Metrics::PerformButtonH);
  setRect(_stopButton.rect,
          _playButton.rect.x + _playButton.rect.w + 2,
          theme::UiTheme::Metrics::PerformPlayY,
          theme::UiTheme::Metrics::PerformControlW - _playButton.rect.w - 2,
          theme::UiTheme::Metrics::PerformButtonH);
  setRect(_muteButton.rect,
          theme::UiTheme::Metrics::PerformControlsX,
          theme::UiTheme::Metrics::PerformMuteY,
          theme::UiTheme::Metrics::PerformControlW,
          theme::UiTheme::Metrics::PerformButtonH);

  setRect(_bpmMinusButton.rect,
          theme::UiTheme::Metrics::PerformControlsX,
          theme::UiTheme::Metrics::PerformStepperY,
          theme::UiTheme::Metrics::PerformStepperAdjustW,
          theme::UiTheme::Metrics::PerformStepperH);
  setRect(_bpmValueRect,
          theme::UiTheme::Metrics::PerformControlsX + theme::UiTheme::Metrics::PerformStepperAdjustW + 2,
          theme::UiTheme::Metrics::PerformStepperY,
          theme::UiTheme::Metrics::PerformStepperValueW,
          theme::UiTheme::Metrics::PerformStepperH);
  setRect(_bpmPlusButton.rect,
          _bpmValueRect.x + _bpmValueRect.w + 2,
          theme::UiTheme::Metrics::PerformStepperY,
          theme::UiTheme::Metrics::PerformStepperAdjustW,
          theme::UiTheme::Metrics::PerformStepperH);

  setRect(_rotateMinusButton.rect,
          theme::UiTheme::Metrics::PerformControlsX,
          theme::UiTheme::Metrics::PerformTrackCarouselY,
          theme::UiTheme::Metrics::PerformTrackCarouselArrowW,
          theme::UiTheme::Metrics::PerformTrackCarouselH);
  setRect(_rotateIconRect,
          theme::UiTheme::Metrics::PerformControlsX + theme::UiTheme::Metrics::PerformTrackCarouselArrowW + 2,
          theme::UiTheme::Metrics::PerformTrackCarouselY,
          theme::UiTheme::Metrics::PerformTrackLabelW,
          theme::UiTheme::Metrics::PerformTrackCarouselH);
  setRect(_rotatePlusButton.rect,
          _rotateIconRect.x + _rotateIconRect.w + 2,
          theme::UiTheme::Metrics::PerformTrackCarouselY,
          theme::UiTheme::Metrics::PerformTrackCarouselArrowW,
          theme::UiTheme::Metrics::PerformTrackCarouselH);

  setRect(_ringsRect,
          theme::UiTheme::Metrics::PerformRingsX,
          theme::UiTheme::Metrics::PerformRingsY,
          theme::UiTheme::Metrics::PerformRingsW,
          theme::UiTheme::Metrics::PerformRingsH);
  _rings.setRect(_ringsRect);
}

void PerformScreen::render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) {
  const bool repaintAll = _fullDirty || !_hasFrame;
  const uint8_t safeActiveTrack = clampTrackIndex(snapshot.activeTrack);
  const uint8_t safeLastActiveTrack = clampTrackIndex(_lastActiveTrack);

  if (repaintAll) {
    canvas.fillRect(0,
                    theme::UiTheme::Metrics::ContentTop,
                    theme::UiTheme::Metrics::ScreenW,
                    theme::UiTheme::Metrics::ContentH,
                    theme::UiTheme::Colors::Bg);
    canvas.fillRoundRect(_heroCardRect.x,
                         _heroCardRect.y,
                         _heroCardRect.w,
                         _heroCardRect.h,
                         theme::UiTheme::Metrics::RadiusMd,
                         theme::UiTheme::Colors::Surface);
    canvas.drawRoundRect(_heroCardRect.x,
                         _heroCardRect.y,
                         _heroCardRect.w,
                         _heroCardRect.h,
                         theme::UiTheme::Metrics::RadiusMd,
                         theme::UiTheme::Colors::Outline);
    _ringsDirty = true;
    _controlsDirty = true;
    _playDirty = true;
    _muteDirty = true;
    _bpmDirty = true;
    _rotateDirty = true;
  }

  if (_hasFrame) {
    if (_lastPlaying != snapshot.isPlaying) {
      _ringsDirty = true;
      _playDirty = true;
    }
    if (_snapshotStep != snapshot.currentStep) {
      _ringsDirty = true;
    }
    if (_lastBpm != snapshot.bpm) {
      _bpmDirty = true;
    }
    if (safeLastActiveTrack != safeActiveTrack) {
      _ringsDirty = true;
      _muteDirty = true;
      _rotateDirty = true;
    }
    for (int i = 0; i < TRACK_COUNT; ++i) {
      if (_lastTrackMutes[i] != snapshot.trackMutes[i]) {
        _ringsDirty = true;
        if (i == safeActiveTrack || i == safeLastActiveTrack) {
          _muteDirty = true;
        }
        _rotateDirty = true;
      }
    }
  }

  if (_controlsDirty || _playDirty || _muteDirty || _bpmDirty || _rotateDirty) {
    _controlsDirty = false;
  }

  if (_playDirty) {
    canvas.fillRect(_playButton.rect.x - 2,
                    _playButton.rect.y - 2,
                    _playButton.rect.w + 4,
                    _playButton.rect.h + 4,
                    theme::UiTheme::Colors::Bg);
    _playButton.pressed = _playPressed;
    _playButton.variant = snapshot.isPlaying ? UiButtonVariant::TransportActive : UiButtonVariant::Primary;
    _playButton.draw(canvas);
    _stopButton.pressed = _stopPressed;
    _stopButton.variant = snapshot.isPlaying ? UiButtonVariant::Danger : UiButtonVariant::Secondary;
    _stopButton.draw(canvas);
    _playDirty = false;
  }

  if (_muteDirty) {
    canvas.fillRect(_muteButton.rect.x - 2,
                    _muteButton.rect.y - 2,
                    _muteButton.rect.w + 4,
                    _muteButton.rect.h + 4,
                    theme::UiTheme::Colors::Bg);
    _muteButton.pressed = _mutePressed;
    _muteButton.label = snapshot.trackMutes[safeActiveTrack] ? "UNMUTE" : "MUTE";
    _muteButton.variant = snapshot.trackMutes[safeActiveTrack] ? UiButtonVariant::Danger : UiButtonVariant::Secondary;
    _muteButton.draw(canvas);
    _muteDirty = false;
  }

  if (_bpmDirty) {
    canvas.fillRect(_bpmMinusButton.rect.x - 2,
                    _bpmMinusButton.rect.y - 2,
                    _bpmPlusButton.rect.x + _bpmPlusButton.rect.w - _bpmMinusButton.rect.x + 4,
                    _bpmMinusButton.rect.h + 4,
                    theme::UiTheme::Colors::Bg);

    _bpmMinusButton.pressed = _bpmMinusPressed;
    _bpmPlusButton.pressed = _bpmPlusPressed;
    _bpmMinusButton.draw(canvas);
    _bpmPlusButton.draw(canvas);

    canvas.fillRoundRect(_bpmValueRect.x,
                         _bpmValueRect.y,
                         _bpmValueRect.w,
                         _bpmValueRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::SurfacePressed);
    canvas.drawRoundRect(_bpmValueRect.x,
                         _bpmValueRect.y,
                         _bpmValueRect.w,
                         _bpmValueRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::Outline);
    canvas.setTextSize(theme::UiTheme::Typography::BodySize);
    canvas.setTextColor(theme::UiTheme::Colors::TextPrimary, theme::UiTheme::Colors::SurfacePressed);
    canvas.setCursor(_bpmValueRect.x + 8, _bpmValueRect.y + 10);
    canvas.printf("%03u", static_cast<unsigned>(snapshot.bpm));
    _bpmDirty = false;
  }

  if (_rotateDirty) {
    canvas.fillRect(_rotateMinusButton.rect.x - 2,
                    _rotateMinusButton.rect.y - 2,
                    _rotatePlusButton.rect.x + _rotatePlusButton.rect.w - _rotateMinusButton.rect.x + 4,
                    _rotateMinusButton.rect.h + 4,
                    theme::UiTheme::Colors::Bg);

    _rotateMinusButton.pressed = _rotateMinusPressed;
    _rotatePlusButton.pressed = _rotatePlusPressed;
    _rotateMinusButton.draw(canvas);
    _rotatePlusButton.draw(canvas);

    canvas.fillRoundRect(_rotateIconRect.x,
                         _rotateIconRect.y,
                         _rotateIconRect.w,
                         _rotateIconRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::SurfacePressed);
    canvas.drawRoundRect(_rotateIconRect.x,
                         _rotateIconRect.y,
                         _rotateIconRect.w,
                         _rotateIconRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::Outline);
    canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
    canvas.setTextColor(theme::UiTheme::Colors::TextPrimary, theme::UiTheme::Colors::SurfacePressed);
    canvas.setCursor(_rotateIconRect.x + 14, _rotateIconRect.y + 12);
    canvas.printf("↻");
    _rotateDirty = false;
  }

  if (_ringsDirty) {
    canvas.fillRect(_ringsRect.x - 2,
                    _ringsRect.y - 2,
                    _ringsRect.w + 4,
                    _ringsRect.h + 4,
                    theme::UiTheme::Colors::Surface);
    _rings.draw(canvas, snapshot);
    _ringsDirty = false;
  }

  _lastPlaying = snapshot.isPlaying;
  _snapshotStep = snapshot.currentStep;
  _lastBpm = snapshot.bpm;
  _lastActiveTrack = safeActiveTrack;
  for (int i = 0; i < TRACK_COUNT; ++i) {
    _lastTrackMutes[i] = snapshot.trackMutes[i];
  }

  _hasFrame = true;
  _fullDirty = false;
}

bool PerformScreen::handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) {
  const uint8_t safeActiveTrack = clampTrackIndex(snapshot.activeTrack);
  const bool playPressedNow = tp.pressed && _playButton.hitTest(tp.x, tp.y);
  const bool stopPressedNow = tp.pressed && _stopButton.hitTest(tp.x, tp.y);
  const bool mutePressedNow = tp.pressed && _muteButton.hitTest(tp.x, tp.y);
  const bool bpmMinusPressedNow = tp.pressed && _bpmMinusButton.hitTest(tp.x, tp.y);
  const bool bpmPlusPressedNow = tp.pressed && _bpmPlusButton.hitTest(tp.x, tp.y);
  const bool rotateMinusPressedNow = tp.pressed && _rotateMinusButton.hitTest(tp.x, tp.y);
  const bool rotatePlusPressedNow = tp.pressed && _rotatePlusButton.hitTest(tp.x, tp.y);

  if (playPressedNow != _playPressed) {
    _playPressed = playPressedNow;
    _playDirty = true;
  }
  if (stopPressedNow != _stopPressed) {
    _stopPressed = stopPressedNow;
    _playDirty = true;
  }
  if (mutePressedNow != _mutePressed) {
    _mutePressed = mutePressedNow;
    _muteDirty = true;
  }
  if (bpmMinusPressedNow != _bpmMinusPressed) {
    _bpmMinusPressed = bpmMinusPressedNow;
    _bpmDirty = true;
  }
  if (bpmPlusPressedNow != _bpmPlusPressed) {
    _bpmPlusPressed = bpmPlusPressedNow;
    _bpmDirty = true;
  }
  if (rotateMinusPressedNow != _rotateMinusPressed) {
    _rotateMinusPressed = rotateMinusPressedNow;
    _rotateDirty = true;
  }
  if (rotatePlusPressedNow != _rotatePlusPressed) {
    _rotatePlusPressed = rotatePlusPressedNow;
    _rotateDirty = true;
  }

  if (!tp.justPressed) {
    return playPressedNow || stopPressedNow || mutePressedNow || bpmMinusPressedNow || bpmPlusPressedNow ||
           rotateMinusPressedNow || rotatePlusPressedNow;
  }

  if (_playButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::TOGGLE_PLAY, 0, 1);
    _playDirty = true;
    _rings.invalidateAll();
    _ringsDirty = true;
    return true;
  }

  if (_stopButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::TOGGLE_PLAY, 0, 2);
    _playDirty = true;
    _rings.invalidateAll();
    _ringsDirty = true;
    return true;
  }

  if (_muteButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::TOGGLE_MUTE, 0, safeActiveTrack);
    _muteDirty = true;
    _rings.invalidateTrack(safeActiveTrack);
    _ringsDirty = true;
    return true;
  }

  if (_bpmMinusButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::NUDGE_BPM, 0, -1);
    _bpmDirty = true;
    return true;
  }

  if (_bpmPlusButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::NUDGE_BPM, 0, 1);
    _bpmDirty = true;
    return true;
  }

  if (_rotateMinusButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::SET_ROTATION, safeActiveTrack, snapshot.trackRotations[safeActiveTrack] - 1);
    _rotateDirty = true;
    _rings.invalidateTrack(safeActiveTrack);
    _ringsDirty = true;
    return true;
  }

  if (_rotatePlusButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::SET_ROTATION, safeActiveTrack, snapshot.trackRotations[safeActiveTrack] + 1);
    _rotateDirty = true;
    _rings.invalidateTrack(safeActiveTrack);
    _ringsDirty = true;
    return true;
  }

  uint8_t ringTrack = 0;
  if (_rings.hitTestTrack(tp.x, tp.y, ringTrack)) {
    dispatchUiAction(UiActionType::SELECT_TRACK, 0, ringTrack);
    _muteDirty = true;
    _rings.invalidateAll();
    _ringsDirty = true;
    return true;
  }

  return false;
}

void PerformScreen::invalidate() {
  _fullDirty = true;
  _ringsDirty = true;
  _controlsDirty = true;
  _playDirty = true;
  _muteDirty = true;
  _bpmDirty = true;
  _rotateDirty = true;
  _rings.invalidateAll();
}

} // namespace ui
