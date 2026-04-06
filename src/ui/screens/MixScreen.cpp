#include "MixScreen.h"

#include "../core/UiActions.h"
#include "../theme/UiTheme.h"

namespace ui {
namespace {
constexpr int kPanelTopY = theme::UiTheme::Metrics::TopBarH;
constexpr int kPanelH = theme::UiTheme::Metrics::ScreenH - theme::UiTheme::Metrics::TopBarH - theme::UiTheme::Metrics::BottomNavH;

void setRect(UiRect &rect, int16_t x, int16_t y, int16_t w, int16_t h) {
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;
}

int8_t toDisplayPercent(float normalized) {
  if (normalized <= 0.0f) {
    return 0;
  }
  if (normalized >= 1.0f) {
    return 100;
  }
  return static_cast<int8_t>(normalized * 100.0f);
}
} // namespace

MixScreen::MixScreen() {
  _headerCard.title = "MIX";
  _headerCard.value = "LEVELS";
  _headerCard.active = true;

  for (int i = 0; i < TRACK_COUNT; ++i) {
    _faders[i].track = static_cast<uint8_t>(i);
    _faders[i].hitRectExpandPx = 10;
    _faderDirty[i] = true;
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
  const bool repaintAll = _fullDirty || !_hasFrame;

  if (repaintAll) {
    canvas.fillRect(0, kPanelTopY, theme::UiTheme::Metrics::ScreenW, kPanelH, theme::UiTheme::Colors::Bg);
    _headerCard.draw(canvas);
    _masterDirty = true;
    for (int i = 0; i < TRACK_COUNT; ++i) {
      _faderDirty[i] = true;
    }
  }

  const uint8_t masterPercent = static_cast<uint8_t>(toDisplayPercent(snapshot.masterVolume));
  if (_lastRenderedMaster != masterPercent) {
    _masterDirty = true;
  }

  if (_masterDirty) {
    canvas.fillRect(140, 52, 132, 16, theme::UiTheme::Colors::Bg);
    canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
    canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
    canvas.setCursor(144, 56);
    canvas.printf("MASTER %u%%", static_cast<unsigned>(masterPercent));
    _lastRenderedMaster = masterPercent;
    _masterDirty = false;
  }

  if (_lastCapturedFader != _activeFader) {
    if (_lastCapturedFader >= 0) {
      _faderDirty[_lastCapturedFader] = true;
    }
    if (_activeFader >= 0) {
      _faderDirty[_activeFader] = true;
    }
    _lastCapturedFader = _activeFader;
  }

  for (int i = 0; i < TRACK_COUNT; ++i) {
    const uint8_t targetValue = (_activeFader == i) ? _faders[i].value : static_cast<uint8_t>(toDisplayPercent(snapshot.voiceGain[i]));
    if (_lastRenderedFaderValues[i] != targetValue) {
      _faderDirty[i] = true;
    }

    if (_activeFader != i) {
      _faders[i].value = targetValue;
    }

    _faders[i].captured = (_activeFader == i);
    if (_faderDirty[i]) {
      canvas.fillRect(_faders[i].visualRect.x - 2,
                      _faders[i].visualRect.y - 2,
                      _faders[i].visualRect.w + 4,
                      _faders[i].visualRect.h + 16,
                      theme::UiTheme::Colors::Bg);
      _faders[i].draw(canvas);
      _lastRenderedFaderValues[i] = _faders[i].value;
      _faderDirty[i] = false;
    }
  }

  _hasFrame = true;
  _fullDirty = false;
}

bool MixScreen::handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) {
  if (tp.justReleased) {
    if (_activeFader >= 0) {
      _faders[_activeFader].endCapture();
      _faderDirty[_activeFader] = true;
      _activeFader = -1;
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
        _faderDirty[i] = true;
        return true;
      }
    }
  }

  if (tp.pressed && _activeFader >= 0) {
    const uint8_t newValue = _faders[_activeFader].updateFromY(tp.y);
    dispatchUiAction(UiActionType::SET_VOICE_GAIN, _activeFader, newValue);
    _faderDirty[_activeFader] = true;
    return true;
  }

  (void)snapshot;
  return false;
}

void MixScreen::invalidate() {
  _fullDirty = true;
  _masterDirty = true;
  for (int i = 0; i < TRACK_COUNT; ++i) {
    _faderDirty[i] = true;
  }
}

} // namespace ui
