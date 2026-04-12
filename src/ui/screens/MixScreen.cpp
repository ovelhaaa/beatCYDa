#include "MixScreen.h"

#include "../core/UiActions.h"
#include "../theme/UiTheme.h"
#include <Arduino.h>

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

int16_t masterThumbXFromPercent(uint8_t percent) {
  const int16_t range = theme::UiTheme::Metrics::MixMasterTrackW - theme::UiTheme::Metrics::MixMasterThumbW;
  return theme::UiTheme::Metrics::MixMasterTrackX + static_cast<int16_t>((range * percent) / 100);
}

uint8_t masterPercentFromX(int16_t x) {
  const int16_t minX = theme::UiTheme::Metrics::MixMasterTrackX;
  const int16_t maxX = theme::UiTheme::Metrics::MixMasterTrackX +
                       theme::UiTheme::Metrics::MixMasterTrackW - theme::UiTheme::Metrics::MixMasterThumbW;
  const int16_t clamped = constrain(x - (theme::UiTheme::Metrics::MixMasterThumbW / 2), minX, maxX);
  const int16_t range = maxX - minX;
  if (range <= 0) {
    return 0;
  }
  return static_cast<uint8_t>(((clamped - minX) * 100) / range);
}

bool rectContainsExpanded(const UiRect &rect, int16_t x, int16_t y, int16_t expandX, int16_t expandY) {
  return x >= rect.x - expandX && x < rect.x + rect.w + expandX && y >= rect.y - expandY && y < rect.y + rect.h + expandY;
}
} // namespace

MixScreen::MixScreen() {
  _headerCard.title = "MIX";
  _headerCard.value = "LEVELS";
  _headerCard.active = true;

  for (int i = 0; i < TRACK_COUNT; ++i) {
    _faders[i].track = static_cast<uint8_t>(i);
    _faders[i].hitRectExpandPx = 8;
    _faderDirty[i] = true;
  }

  layout();
}

void MixScreen::layout() {
  setRect(_headerCard.rect,
          theme::UiTheme::Metrics::MixHeaderX,
          theme::UiTheme::Metrics::MixHeaderY,
          theme::UiTheme::Metrics::MixHeaderW,
          theme::UiTheme::Metrics::MixHeaderH);

  setRect(_masterTrackRect,
          theme::UiTheme::Metrics::MixMasterTrackX,
          theme::UiTheme::Metrics::MixMasterTrackY,
          theme::UiTheme::Metrics::MixMasterTrackW,
          theme::UiTheme::Metrics::MixMasterTrackH);
  setRect(_masterThumbRect,
          theme::UiTheme::Metrics::MixMasterTrackX,
          theme::UiTheme::Metrics::MixMasterTrackY,
          theme::UiTheme::Metrics::MixMasterThumbW,
          theme::UiTheme::Metrics::MixMasterThumbH);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    setRect(_faders[i].visualRect,
            theme::UiTheme::Metrics::MixFaderStartX + i * (theme::UiTheme::Metrics::MixFaderW + theme::UiTheme::Metrics::MixFaderGap),
            theme::UiTheme::Metrics::MixFaderY,
            theme::UiTheme::Metrics::MixFaderW,
            theme::UiTheme::Metrics::MixFaderH);
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

  const uint8_t masterPercent = _activeMaster ? _lastRenderedMaster : static_cast<uint8_t>(toDisplayPercent(snapshot.masterVolume));
  if (_lastRenderedMaster != masterPercent) {
    _masterDirty = true;
  }

  if (_masterDirty) {
    canvas.fillRect(theme::UiTheme::Metrics::MixMasterLabelX,
                    theme::UiTheme::Metrics::MixMasterLabelY,
                    theme::UiTheme::Metrics::MixMasterTrackW,
                    theme::UiTheme::Metrics::MixMasterTrackH + 20,
                    theme::UiTheme::Colors::Bg);

    canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
    canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
    canvas.setCursor(theme::UiTheme::Metrics::MixMasterLabelX, theme::UiTheme::Metrics::MixMasterLabelY + 8);
    canvas.printf("MASTER %u%%", static_cast<unsigned>(masterPercent));

    canvas.fillRoundRect(_masterTrackRect.x,
                         _masterTrackRect.y,
                         _masterTrackRect.w,
                         _masterTrackRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::SurfacePressed);
    canvas.drawRoundRect(_masterTrackRect.x,
                         _masterTrackRect.y,
                         _masterTrackRect.w,
                         _masterTrackRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::Outline);

    const int16_t fillW = (_masterTrackRect.w * masterPercent) / 100;
    canvas.fillRoundRect(_masterTrackRect.x,
                         _masterTrackRect.y,
                         fillW,
                         _masterTrackRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::AccentPressed);

    _masterThumbRect.x = masterThumbXFromPercent(masterPercent);
    _masterThumbRect.y = _masterTrackRect.y;
    const uint16_t thumbColor = _activeMaster ? theme::UiTheme::Colors::Accent : theme::UiTheme::Colors::TextPrimary;
    canvas.fillRoundRect(_masterThumbRect.x,
                         _masterThumbRect.y,
                         _masterThumbRect.w,
                         _masterThumbRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         thumbColor);

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
      canvas.fillRect(_faders[i].visualRect.x - theme::UiTheme::Metrics::MixFaderDirtyPadding,
                      _faders[i].visualRect.y - theme::UiTheme::Metrics::MixFaderDirtyPadding,
                      _faders[i].visualRect.w + theme::UiTheme::Metrics::MixFaderDirtyPadding * 2,
                      _faders[i].visualRect.h + theme::UiTheme::Metrics::MixFaderDirtyLabelExtraH,
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
    if (_activeMaster) {
      _activeMaster = false;
      _masterDirty = true;
      return true;
    }
    if (_activeFader >= 0) {
      _faders[_activeFader].endCapture();
      _faderDirty[_activeFader] = true;
      _activeFader = -1;
      return true;
    }
    return false;
  }

  if (tp.justPressed) {
    if (rectContainsExpanded(_masterTrackRect, tp.x, tp.y, 0, 8)) {
      const uint8_t newMaster = masterPercentFromX(tp.x);
      _lastRenderedMaster = newMaster;
      dispatchUiAction(UiActionType::SET_MASTER_GAIN, 0, newMaster);
      _activeMaster = true;
      _masterDirty = true;
      return true;
    }

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

  if (tp.pressed && _activeMaster) {
    const uint8_t newMaster = masterPercentFromX(tp.x);
    if (_lastRenderedMaster != newMaster) {
      _lastRenderedMaster = newMaster;
      dispatchUiAction(UiActionType::SET_MASTER_GAIN, 0, newMaster);
      _masterDirty = true;
    }
    return true;
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
