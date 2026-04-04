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

  for (int i = 0; i < TRACK_COUNT; ++i) {
    _trackChips[i].trackIndex = static_cast<uint8_t>(i);
    _trackChips[i].active = (i == 0);
    _trackChips[i].selected = (i == 0);
  }

  layout();
}

void PerformScreen::layout() {
  setRect(_playButton.rect, 12, 54, 88, 34);
  setRect(_muteButton.rect, 108, 54, 88, 34);
  setRect(_statusCard.rect, 204, 54, 104, 60);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    setRect(_trackChips[i].rect, 12 + (i * 60), 124, 56, 32);
  }
}

void PerformScreen::render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) {
  canvas.fillRect(0,
                  theme::UiTheme::Metrics::TopBarH,
                  theme::UiTheme::Metrics::ScreenW,
                  theme::UiTheme::Metrics::ScreenH -
                      theme::UiTheme::Metrics::TopBarH - theme::UiTheme::Metrics::BottomNavH,
                  theme::UiTheme::Colors::Bg);

  _playButton.label = snapshot.isPlaying ? "STOP" : "PLAY";
  _muteButton.label = snapshot.trackMutes[snapshot.activeTrack] ? "UNMUTE" : "MUTE";
  _muteButton.variant = snapshot.trackMutes[snapshot.activeTrack] ? UiButtonVariant::Danger : UiButtonVariant::Secondary;

  _statusCard.value = trackLabel(snapshot.activeTrack);

  _playButton.draw(canvas);
  _muteButton.draw(canvas);
  _statusCard.draw(canvas);

  canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
  canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
  canvas.setCursor(12, 96);
  canvas.printf("BPM %d", snapshot.bpm);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    _trackChips[i].active = (i == snapshot.activeTrack);
    _trackChips[i].selected = (i == snapshot.activeTrack);
    _trackChips[i].muted = snapshot.trackMutes[i];
    _trackChips[i].draw(canvas);
  }

  _dirty = false;
}

bool PerformScreen::handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) {
  if (!tp.justPressed) {
    return false;
  }

  if (_playButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::TOGGLE_PLAY, 0, 0);
    _dirty = true;
    return true;
  }

  if (_muteButton.hitTest(tp.x, tp.y)) {
    dispatchUiAction(UiActionType::TOGGLE_MUTE, 0, snapshot.activeTrack);
    _dirty = true;
    return true;
  }

  for (int i = 0; i < TRACK_COUNT; ++i) {
    if (_trackChips[i].hitTest(tp.x, tp.y)) {
      dispatchUiAction(UiActionType::SELECT_TRACK, 0, i);
      _dirty = true;
      return true;
    }
  }

  return false;
}

void PerformScreen::invalidate() {
  _dirty = true;
}

} // namespace ui
