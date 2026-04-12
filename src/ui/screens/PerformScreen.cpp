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

const char *trackLabel(int track) {
  switch (track) {
  case 0:
    return "KICK";
  case 1:
    return "SNARE";
  case 2:
    return "CH";
  case 3:
    return "OH";
  default:
    return "BASS";
  }
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
  _playButton.label = "PLAY";
  _playButton.variant = UiButtonVariant::Primary;

  _muteButton.label = "MUTE";
  _muteButton.variant = UiButtonVariant::Secondary;

  _statusCard.title = "PERFORM";
  _statusCard.value = "READY";
  _statusCard.active = true;

  _rings.setCompact(false);
  _rings.setInteractive(true);
  _rings.setSingleTrack(false);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    _trackChips[i].trackIndex = static_cast<uint8_t>(i);
    _trackChips[i].active = (i == 0);
    _trackChips[i].selected = (i == 0);
  }

  layout();
}

void PerformScreen::layout() {
  setRect(_playButton.rect,
          theme::UiTheme::Metrics::PerformControlsX,
          theme::UiTheme::Metrics::PerformPlayY,
          theme::UiTheme::Metrics::PerformControlW,
          theme::UiTheme::Metrics::PerformButtonH);
  setRect(_muteButton.rect,
          theme::UiTheme::Metrics::PerformControlsX,
          theme::UiTheme::Metrics::PerformMuteY,
          theme::UiTheme::Metrics::PerformControlW,
          theme::UiTheme::Metrics::PerformButtonH);
  setRect(_statusCard.rect,
          theme::UiTheme::Metrics::PerformRingsX,
          theme::UiTheme::Metrics::PerformBpmY,
          theme::UiTheme::Metrics::PerformControlW,
          theme::UiTheme::Metrics::PerformBpmH);

  setRect(_ringsRect,
          theme::UiTheme::Metrics::PerformRingsX,
          theme::UiTheme::Metrics::PerformRingsY,
          theme::UiTheme::Metrics::PerformRingsW,
          theme::UiTheme::Metrics::PerformRingsH);
  _rings.setRect(_ringsRect);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    const int col = i % theme::UiTheme::Metrics::PerformTrackStripColumns;
    const int row = i / theme::UiTheme::Metrics::PerformTrackStripColumns;
    setRect(_trackChips[i].rect,
            theme::UiTheme::Metrics::PerformTrackStripX +
                (col * (theme::UiTheme::Metrics::PerformTrackChipW + theme::UiTheme::Metrics::PerformTrackChipGap)),
            theme::UiTheme::Metrics::PerformTrackStripY +
                (row * (theme::UiTheme::Metrics::PerformTrackChipH + theme::UiTheme::Metrics::PerformTrackChipGap)),
            theme::UiTheme::Metrics::PerformTrackChipW,
            theme::UiTheme::Metrics::PerformTrackChipH);
  }
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
    _ringsDirty = true;
    _controlsDirty = true;
    _playDirty = true;
    _muteDirty = true;
    _statusDirty = true;
    _trackStripDirty = true;
    _bpmDirty = true;
  }

  if (_hasFrame) {
    if (_lastPlaying != snapshot.isPlaying) {
      _ringsDirty = true;
      _playDirty = true;
    }
    if (_lastBpm != snapshot.bpm) {
      _bpmDirty = true;
    }
    if (safeLastActiveTrack != safeActiveTrack) {
      _ringsDirty = true;
      _muteDirty = true;
      _statusDirty = true;
      _trackChipDirty[safeLastActiveTrack] = true;
      _trackChipDirty[safeActiveTrack] = true;
    }
    for (int i = 0; i < TRACK_COUNT; ++i) {
      if (_lastTrackMutes[i] != snapshot.trackMutes[i]) {
        _ringsDirty = true;
        if (i == safeActiveTrack || i == safeLastActiveTrack) {
          _muteDirty = true;
        }
        _trackChipDirty[i] = true;
      }
    }
  }

  if (_controlsDirty || _playDirty || _muteDirty || _statusDirty) {
    _controlsDirty = false;
  }

  if (_playDirty) {
    canvas.fillRect(_playButton.rect.x - 2,
                    _playButton.rect.y - 2,
                    _playButton.rect.w + 4,
                    _playButton.rect.h + 4,
                    theme::UiTheme::Colors::Bg);
    _playButton.pressed = _playPressed;
    _playButton.label = snapshot.isPlaying ? "STOP" : "PLAY";
    _playButton.draw(canvas);
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

  if (_statusDirty) {
    canvas.fillRect(_statusCard.rect.x,
                    _statusCard.rect.y - 2,
                    _statusCard.rect.w,
                    _statusCard.rect.h + 4,
                    theme::UiTheme::Colors::Bg);
    _statusCard.value = trackLabel(safeActiveTrack);
    _statusCard.draw(canvas);
    _statusDirty = false;
  }

  if (_ringsDirty) {
    canvas.fillRect(_ringsRect.x - 2,
                    _ringsRect.y - 2,
                    _ringsRect.w + 4,
                    _ringsRect.h + 4,
                    theme::UiTheme::Colors::Bg);
    _rings.draw(canvas, snapshot);
    _ringsDirty = false;
  }

  if (_bpmDirty) {
    canvas.fillRect(theme::UiTheme::Metrics::PerformBpmX,
                    theme::UiTheme::Metrics::PerformBpmY,
                    theme::UiTheme::Metrics::PerformBpmW,
                    theme::UiTheme::Metrics::PerformBpmH,
                    theme::UiTheme::Colors::Bg);
    canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
    canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
    canvas.setCursor(theme::UiTheme::Metrics::PerformBpmX, theme::UiTheme::Metrics::PerformBpmY);
    canvas.printf("BPM %d", snapshot.bpm);
    _bpmDirty = false;
  }

  if (_trackStripDirty) {
    const int numCols = theme::UiTheme::Metrics::PerformTrackStripColumns;
    const int numRows = (TRACK_COUNT + numCols - 1) / numCols;
    canvas.fillRect(theme::UiTheme::Metrics::PerformTrackStripX - 2,
                    theme::UiTheme::Metrics::PerformTrackStripY - 2,
                    (theme::UiTheme::Metrics::PerformTrackChipW * numCols) +
                        (theme::UiTheme::Metrics::PerformTrackChipGap * (numCols - 1)) + 4,
                    (theme::UiTheme::Metrics::PerformTrackChipH * numRows) +
                        (theme::UiTheme::Metrics::PerformTrackChipGap * (numRows - 1)) + 4,
                    theme::UiTheme::Colors::Bg);
    for (int i = 0; i < TRACK_COUNT; ++i) {
      _trackChips[i].active = (i == safeActiveTrack);
      _trackChips[i].selected = (i == safeActiveTrack);
      _trackChips[i].muted = snapshot.trackMutes[i];
      _trackChips[i].draw(canvas);
      _trackChipDirty[i] = false;
    }
    _trackStripDirty = false;
  } else {
    for (int i = 0; i < TRACK_COUNT; ++i) {
      if (!_trackChipDirty[i]) {
        continue;
      }
      _trackChips[i].active = (i == safeActiveTrack);
      _trackChips[i].selected = (i == safeActiveTrack);
      _trackChips[i].muted = snapshot.trackMutes[i];
      canvas.fillRect(_trackChips[i].rect.x - 2,
                      _trackChips[i].rect.y - 2,
                      _trackChips[i].rect.w + 4,
                      _trackChips[i].rect.h + 4,
                      theme::UiTheme::Colors::Bg);
      _trackChips[i].draw(canvas);
      _trackChipDirty[i] = false;
    }
  }

  _lastPlaying = snapshot.isPlaying;
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
  const bool mutePressedNow = tp.pressed && _muteButton.hitTest(tp.x, tp.y);
  if (playPressedNow != _playPressed) {
    _playPressed = playPressedNow;
    _playDirty = true;
  }
  if (mutePressedNow != _mutePressed) {
    _mutePressed = mutePressedNow;
    _muteDirty = true;
  }

  if (!tp.justPressed) {
    return playPressedNow || mutePressedNow;
  }

  if (_playButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::TOGGLE_PLAY, 0, 0);
    _playDirty = true;
    _rings.invalidateAll();
    _ringsDirty = true;
    return true;
  }

  if (_muteButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::TOGGLE_MUTE, 0, safeActiveTrack);
    _muteDirty = true;
    _trackChipDirty[safeActiveTrack] = true;
    _rings.invalidateTrack(safeActiveTrack);
    _ringsDirty = true;
    return true;
  }

  uint8_t ringTrack = 0;
  if (_rings.hitTestTrack(tp.x, tp.y, ringTrack)) {
    dispatchUiAction(UiActionType::SELECT_TRACK, 0, ringTrack);
    _muteDirty = true;
    _statusDirty = true;
    _trackChipDirty[safeActiveTrack] = true;
    _trackChipDirty[clampTrackIndex(ringTrack)] = true;
    _rings.invalidateAll();
    _ringsDirty = true;
    return true;
  }

  for (int i = 0; i < TRACK_COUNT; ++i) {
    if (_trackChips[i].hitTest(tp.x, tp.y)) {
      dispatchUiAction(UiActionType::SELECT_TRACK, 0, i);
      _muteDirty = true;
      _statusDirty = true;
      _trackChipDirty[safeActiveTrack] = true;
      _trackChipDirty[i] = true;
      _rings.invalidateAll();
      _ringsDirty = true;
      return true;
    }
  }

  return false;
}

void PerformScreen::invalidate() {
  _fullDirty = true;
  _ringsDirty = true;
  _controlsDirty = true;
  _playDirty = true;
  _muteDirty = true;
  _statusDirty = true;
  _trackStripDirty = true;
  _bpmDirty = true;
  for (int i = 0; i < TRACK_COUNT; ++i) {
    _trackChipDirty[i] = true;
  }
  _rings.invalidateAll();
}

} // namespace ui
