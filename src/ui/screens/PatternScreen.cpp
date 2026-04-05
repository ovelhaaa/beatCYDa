#include "PatternScreen.h"

#include "../core/UiActions.h"
#include "../theme/UiTheme.h"
#include "../../CYD_Config.h"

namespace ui {
namespace {
void setRect(UiRect &rect, int16_t x, int16_t y, int16_t w, int16_t h) {
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;
}

void formatPercent(char *buffer, size_t size, int value) {
  snprintf(buffer, size, "%d%%", value);
}
} // namespace

PatternScreen::PatternScreen() {
  _headerCard.title = "PATTERN";
  _headerCard.value = "EDIT";
  _headerCard.active = true;

  _rows[0].label = "STEPS";
  _rows[1].label = "HITS";
  _rows[2].label = "ROTATE";
  _rows[3].label = "GAIN";

  _rows[3].showBar = true;

  _randomButton.label = "RANDOM";
  _randomButton.variant = UiButtonVariant::Secondary;
  _randomButton.disabled = false;
  _clearButton.label = "CLEAR";
  _clearButton.variant = UiButtonVariant::Danger;

  _ringsPreview.setCompact(true);
  _ringsPreview.setInteractive(false);
  _ringsPreview.setSingleTrack(true);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    _trackChips[i].trackIndex = static_cast<uint8_t>(i);
  }

  layout();
}

void PatternScreen::layout() {
  setRect(_headerCard.rect, 12, 46, 104, 48);

  UiRect previewRect{};
  setRect(previewRect, 124, 40, 84, 84);
  _ringsPreview.setRect(previewRect);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    setRect(_trackChips[i].rect, 12 + (i * 60), 96, 56, 26);
  }

  for (int i = 0; i < 4; ++i) {
    const int y = 128 + (i * 18);
    setRect(_rows[i].rowRect, 12, y, 296, 16);
    setRect(_rows[i].minusRect, 220, y, 20, 16);
    setRect(_rows[i].plusRect, 244, y, 20, 16);
  }

  setRect(_randomButton.rect, 12, 200, 140, 28);
  setRect(_clearButton.rect, 168, 200, 140, 28);
}

void PatternScreen::render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) {
  canvas.fillRect(0,
                  theme::UiTheme::Metrics::TopBarH,
                  theme::UiTheme::Metrics::ScreenW,
                  theme::UiTheme::Metrics::ScreenH -
                      theme::UiTheme::Metrics::TopBarH - theme::UiTheme::Metrics::BottomNavH,
                  theme::UiTheme::Colors::Bg);

  _headerCard.active = true;
  _headerCard.draw(canvas);
  _ringsPreview.draw(canvas, snapshot);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    _trackChips[i].active = (i == snapshot.activeTrack);
    _trackChips[i].selected = (i == snapshot.activeTrack);
    _trackChips[i].muted = snapshot.trackMutes[i];
    _trackChips[i].draw(canvas);
  }

  char valueBuffers[4][16];
  snprintf(valueBuffers[0], sizeof(valueBuffers[0]), "%d", snapshot.trackSteps[snapshot.activeTrack]);
  snprintf(valueBuffers[1], sizeof(valueBuffers[1]), "%d", snapshot.trackHits[snapshot.activeTrack]);
  snprintf(valueBuffers[2], sizeof(valueBuffers[2]), "%d", snapshot.trackRotations[snapshot.activeTrack]);
  formatPercent(valueBuffers[3], sizeof(valueBuffers[3]), static_cast<int>(snapshot.voiceGain[snapshot.activeTrack] * 100.0f));

  const uint8_t gainFill = static_cast<uint8_t>(snapshot.voiceGain[snapshot.activeTrack] * 100.0f);

  for (int i = 0; i < 4; ++i) {
    _rows[i].focus = (_holdRow == i);
    _rows[i].minusPressed = (_holdRow == i && _holdDirection < 0);
    _rows[i].plusPressed = (_holdRow == i && _holdDirection > 0);
    _rows[i].valueText = valueBuffers[i];
    _rows[i].barFill = (i == 3) ? gainFill : 0;
    _rows[i].draw(canvas);
  }

  _randomButton.draw(canvas);
  _clearButton.draw(canvas);
  _dirty = false;
}

bool PatternScreen::handleHoldTick(const TouchPoint &tp, const UiStateSnapshot &snapshot) {
  if (_holdRow < 0 || !tp.pressed) {
    stopHold();
    return false;
  }

  const bool stillHolding = (_holdDirection < 0) ? _rows[_holdRow].hitMinus(tp.x, tp.y) : _rows[_holdRow].hitPlus(tp.x, tp.y);
  if (!stillHolding) {
    stopHold();
    return false;
  }

  const uint32_t now = millis();
  if (now < _holdNextTickMs) {
    return false;
  }

  int multiplier = 1;
  uint16_t interval = CYDConfig::HoldRepeatIntervalStartMs;
  if (_holdTickCount >= CYDConfig::HoldRepeatStage2Threshold) {
    multiplier = CYDConfig::HoldRepeatStage2Multiplier;
    interval = CYDConfig::HoldRepeatIntervalStage2Ms;
  } else if (_holdTickCount >= CYDConfig::HoldRepeatStage1Threshold) {
    multiplier = CYDConfig::HoldRepeatStage1Multiplier;
    interval = CYDConfig::HoldRepeatIntervalStage1Ms;
  }

  dispatchRowDelta(snapshot, _holdRow, _holdDirection * multiplier);
  _holdTickCount++;
  _holdNextTickMs = now + interval;
  _dirty = true;
  _ringsPreview.invalidateAll();
  return true;
}

void PatternScreen::dispatchRowDelta(const UiStateSnapshot &snapshot, int rowIndex, int amount) {
  if (rowIndex == 0) {
    dispatchUiAction(UiActionType::SET_STEPS, snapshot.activeTrack, snapshot.trackSteps[snapshot.activeTrack] + amount);
    return;
  }

  if (rowIndex == 1) {
    dispatchUiAction(UiActionType::SET_HITS, snapshot.activeTrack, snapshot.trackHits[snapshot.activeTrack] + amount);
    return;
  }

  if (rowIndex == 2) {
    dispatchUiAction(UiActionType::SET_ROTATION, snapshot.activeTrack,
                     snapshot.trackRotations[snapshot.activeTrack] + amount);
    return;
  }

  dispatchUiAction(UiActionType::SET_SOUND_PARAM, 3,
                   static_cast<int>(snapshot.voiceGain[snapshot.activeTrack] * 100.0f) + amount);
}

void PatternScreen::startHold(int rowIndex, int direction) {
  _holdRow = rowIndex;
  _holdDirection = direction;
  _holdTickCount = 0;
  _holdNextTickMs = millis() + CYDConfig::HoldRepeatStartDelayMs;
}

void PatternScreen::stopHold() {
  _holdRow = -1;
  _holdDirection = 0;
  _holdTickCount = 0;
  _holdNextTickMs = 0;
}

bool PatternScreen::handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) {
  bool consumed = false;

  if (tp.justReleased) {
    stopHold();
  }

  if (tp.justPressed) {
    for (int i = 0; i < TRACK_COUNT; ++i) {
      if (_trackChips[i].hitTest(tp.x, tp.y)) {
        dispatchUiAction(UiActionType::SELECT_TRACK, 0, i);
        _dirty = true;
        _ringsPreview.invalidateAll();
        return true;
      }
    }

    for (int i = 0; i < 4; ++i) {
      if (_rows[i].hitMinus(tp.x, tp.y)) {
        dispatchRowDelta(snapshot, i, -1);
        startHold(i, -1);
        _dirty = true;
        _ringsPreview.invalidateAll();
        return true;
      }

      if (_rows[i].hitPlus(tp.x, tp.y)) {
        dispatchRowDelta(snapshot, i, +1);
        startHold(i, +1);
        _dirty = true;
        _ringsPreview.invalidateAll();
        return true;
      }
    }

    if (_randomButton.hitTest(tp.x, tp.y)) {
      dispatchUiAction(UiActionType::RANDOMIZE_TRACK, snapshot.activeTrack, 0);
      _dirty = true;
      _ringsPreview.invalidateAll();
      return true;
    }

    if (_clearButton.hitTest(tp.x, tp.y)) {
      dispatchUiAction(UiActionType::SET_HITS, snapshot.activeTrack, 0);
      dispatchUiAction(UiActionType::SET_ROTATION, snapshot.activeTrack, 0);
      _dirty = true;
      _ringsPreview.invalidateAll();
      return true;
    }
  }

  consumed = handleHoldTick(tp, snapshot) || consumed;
  return consumed;
}

void PatternScreen::invalidate() {
  _dirty = true;
  _ringsPreview.invalidateAll();
}

} // namespace ui
