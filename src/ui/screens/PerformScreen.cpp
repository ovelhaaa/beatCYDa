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
  setRect(_playButton.rect, 204, 46, 104, 34);
  setRect(_muteButton.rect, 204, 86, 104, 34);
  setRect(_statusCard.rect, 204, 126, 104, 62);

  setRect(_ringsRect, 12, 42, 176, 142);
  _rings.setRect(_ringsRect);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    setRect(_trackChips[i].rect, 12 + (i * 60), 196, 56, 32);
  }
}

void PerformScreen::render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) {
  const bool repaintAll = _fullDirty || !_hasFrame;

  if (repaintAll) {
    canvas.fillRect(0,
                    theme::UiTheme::Metrics::TopBarH,
                    theme::UiTheme::Metrics::ScreenW,
                    theme::UiTheme::Metrics::ScreenH -
                        theme::UiTheme::Metrics::TopBarH - theme::UiTheme::Metrics::BottomNavH,
                    theme::UiTheme::Colors::Bg);
    _ringsDirty = true;
    _controlsDirty = true;
    _trackStripDirty = true;
    _bpmDirty = true;
  }

  if (_hasFrame) {
    if (_lastPlaying != snapshot.isPlaying) {
      _ringsDirty = true;
      _controlsDirty = true;
    }
    if (_lastBpm != snapshot.bpm) {
      _bpmDirty = true;
    }
    if (_lastActiveTrack != snapshot.activeTrack) {
      _ringsDirty = true;
      _controlsDirty = true;
      _trackStripDirty = true;
    }
    for (int i = 0; i < TRACK_COUNT; ++i) {
      if (_lastTrackMutes[i] != snapshot.trackMutes[i]) {
        _ringsDirty = true;
        if (i == snapshot.activeTrack || i == _lastActiveTrack) {
          _controlsDirty = true;
        }
        _trackStripDirty = true;
      }
    }
  }

  if (_controlsDirty) {
    canvas.fillRect(_playButton.rect.x,
                    _playButton.rect.y,
                    _playButton.rect.w,
                    _statusCard.rect.y + _statusCard.rect.h - _playButton.rect.y,
                    theme::UiTheme::Colors::Bg);
    _playButton.label = snapshot.isPlaying ? "STOP" : "PLAY";
    _muteButton.label = snapshot.trackMutes[snapshot.activeTrack] ? "UNMUTE" : "MUTE";
    _muteButton.variant = snapshot.trackMutes[snapshot.activeTrack] ? UiButtonVariant::Danger : UiButtonVariant::Secondary;
    _statusCard.value = trackLabel(snapshot.activeTrack);

    _playButton.draw(canvas);
    _muteButton.draw(canvas);
    _statusCard.draw(canvas);
    _controlsDirty = false;
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
    canvas.fillRect(206, 196, 104, 16, theme::UiTheme::Colors::Bg);
    canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
    canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
    canvas.setCursor(206, 196);
    canvas.printf("BPM %d", snapshot.bpm);
    _bpmDirty = false;
  }

  if (_trackStripDirty) {
    canvas.fillRect(10, 194, theme::UiTheme::Metrics::ScreenW - 20, 36, theme::UiTheme::Colors::Bg);
    for (int i = 0; i < TRACK_COUNT; ++i) {
      _trackChips[i].active = (i == snapshot.activeTrack);
      _trackChips[i].selected = (i == snapshot.activeTrack);
      _trackChips[i].muted = snapshot.trackMutes[i];
      _trackChips[i].draw(canvas);
    }
    _trackStripDirty = false;
  } else {
    for (int i = 0; i < TRACK_COUNT; ++i) {
      _trackChips[i].active = (i == snapshot.activeTrack);
      _trackChips[i].selected = (i == snapshot.activeTrack);
      _trackChips[i].muted = snapshot.trackMutes[i];
    }
  }

  _lastPlaying = snapshot.isPlaying;
  _lastBpm = snapshot.bpm;
  _lastActiveTrack = snapshot.activeTrack;
  for (int i = 0; i < TRACK_COUNT; ++i) {
    _lastTrackMutes[i] = snapshot.trackMutes[i];
  }

  _hasFrame = true;
  _fullDirty = false;
}

bool PerformScreen::handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) {
  if (!tp.justPressed) {
    return false;
  }

  if (_playButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::TOGGLE_PLAY, 0, 0);
    _controlsDirty = true;
    _rings.invalidateAll();
    _ringsDirty = true;
    return true;
  }

  if (_muteButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::TOGGLE_MUTE, 0, snapshot.activeTrack);
    _controlsDirty = true;
    _trackStripDirty = true;
    _rings.invalidateTrack(static_cast<uint8_t>(snapshot.activeTrack));
    _ringsDirty = true;
    return true;
  }

  uint8_t ringTrack = 0;
  if (_rings.hitTestTrack(tp.x, tp.y, ringTrack)) {
    dispatchUiAction(UiActionType::SELECT_TRACK, 0, ringTrack);
    _controlsDirty = true;
    _trackStripDirty = true;
    _rings.invalidateAll();
    _ringsDirty = true;
    return true;
  }

  for (int i = 0; i < TRACK_COUNT; ++i) {
    if (_trackChips[i].hitTest(tp.x, tp.y)) {
      dispatchUiAction(UiActionType::SELECT_TRACK, 0, i);
      _controlsDirty = true;
      _trackStripDirty = true;
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
  _trackStripDirty = true;
  _bpmDirty = true;
  _rings.invalidateAll();
}

} // namespace ui
