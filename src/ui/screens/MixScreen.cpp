#include "MixScreen.h"

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
} // namespace

MixScreen::MixScreen() {
  _headerCard.title = "MIX";
  _headerCard.value = "LEVELS";
  _headerCard.active = true;

  for (int i = 0; i < TRACK_COUNT; ++i) {
    _faders[i].track = static_cast<uint8_t>(i);
    _faders[i].hitRectExpandPx = 10;
  }

  layout();
}

void MixScreen::layout() {
  setRect(_headerCard.rect, 12, 46, 120, 48);

  constexpr int faderY = 102;
  constexpr int faderW = 40;
  constexpr int faderH = 90;
  constexpr int gap = 20;
  constexpr int startX = 20;

  for (int i = 0; i < TRACK_COUNT; ++i) {
    setRect(_faders[i].visualRect, startX + i * (faderW + gap), faderY, faderW, faderH);
  }
}

void MixScreen::render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) {
  canvas.fillRect(0,
                  theme::UiTheme::Metrics::TopBarH,
                  theme::UiTheme::Metrics::ScreenW,
                  theme::UiTheme::Metrics::ScreenH -
                      theme::UiTheme::Metrics::TopBarH - theme::UiTheme::Metrics::BottomNavH,
                  theme::UiTheme::Colors::Bg);

  _headerCard.draw(canvas);

  canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
  canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
  canvas.setCursor(144, 56);
  canvas.printf("MASTER %d%%", static_cast<int>(snapshot.masterVolume * 100.0f));

  for (int i = 0; i < TRACK_COUNT; ++i) {
    if (_activeFader != i) {
      _faders[i].value = static_cast<uint8_t>(snapshot.voiceGain[i] * 100.0f);
    }
    _faders[i].captured = (_activeFader == i);
    _faders[i].draw(canvas);
  }

  _dirty = false;
}

bool MixScreen::handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) {
  if (tp.justReleased) {
    if (_activeFader >= 0) {
      _faders[_activeFader].endCapture();
      _activeFader = -1;
      _dirty = true;
      return true;
    }
    return false;
  }

  if (tp.justPressed) {
    for (int i = 0; i < TRACK_COUNT; ++i) {
      if (_faders[i].beginCapture(tp.x, tp.y)) {
        _activeFader = i;
        const uint8_t newValue = _faders[i].updateFromY(tp.y);
        dispatchUiAction(UiActionType::SET_VOICE_GAIN, i, newValue);
        _dirty = true;
        return true;
      }
    }
  }

  if (tp.pressed && _activeFader >= 0) {
    const uint8_t newValue = _faders[_activeFader].updateFromY(tp.y);
    dispatchUiAction(UiActionType::SET_VOICE_GAIN, _activeFader, newValue);
    _dirty = true;
    return true;
  }

  (void)snapshot;
  return false;
}

void MixScreen::invalidate() {
  _dirty = true;
}

} // namespace ui
