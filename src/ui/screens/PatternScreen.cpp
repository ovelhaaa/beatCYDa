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

bool hasMuteChanges(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  for (int i = 0; i < TRACK_COUNT; ++i) {
    if (lhs.trackMutes[i] != rhs.trackMutes[i]) {
      return true;
    }
  }
  return false;
}

bool hasPatternChanges(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  for (int track = 0; track < TRACK_COUNT; ++track) {
    if (lhs.patternLens[track] != rhs.patternLens[track]) {
      return true;
    }
    for (int step = 0; step < 64; ++step) {
      if (lhs.patterns[track][step] != rhs.patterns[track][step]) {
        return true;
      }
    }
  }
  return false;
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
  const bool forceFullRender = _dirty || !_hasLastSnapshot;
  const bool trackChanged = forceFullRender || snapshot.activeTrack != _lastSnapshot.activeTrack;

  const bool chipsDirty = forceFullRender || trackChanged || hasMuteChanges(snapshot, _lastSnapshot);
  const bool previewDirty =
      forceFullRender || trackChanged || snapshot.currentStep != _lastSnapshot.currentStep || hasPatternChanges(snapshot, _lastSnapshot);
  bool rowDirty[4] = {forceFullRender || trackChanged, forceFullRender || trackChanged,
                      forceFullRender || trackChanged, forceFullRender || trackChanged};
  const bool actionButtonsDirty = forceFullRender;

  for (int i = 0; i < 4; ++i) {
    if (rowDirty[i]) {
      continue;
    }

    bool valueChanged = false;
    if (i == 0) {
      valueChanged = snapshot.trackSteps[snapshot.activeTrack] != _lastSnapshot.trackSteps[_lastSnapshot.activeTrack];
    } else if (i == 1) {
      valueChanged = snapshot.trackHits[snapshot.activeTrack] != _lastSnapshot.trackHits[_lastSnapshot.activeTrack];
    } else if (i == 2) {
      valueChanged = snapshot.trackRotations[snapshot.activeTrack] != _lastSnapshot.trackRotations[_lastSnapshot.activeTrack];
    } else {
      valueChanged = snapshot.voiceGain[snapshot.activeTrack] != _lastSnapshot.voiceGain[_lastSnapshot.activeTrack];
    }

    const bool focusChanged = ((_holdRow == i) != (_lastHoldRow == i));
    const bool minusChanged = ((_holdRow == i && _holdDirection < 0) != (_lastHoldRow == i && _lastHoldDirection < 0));
    const bool plusChanged = ((_holdRow == i && _holdDirection > 0) != (_lastHoldRow == i && _lastHoldDirection > 0));
    rowDirty[i] = valueChanged || focusChanged || minusChanged || plusChanged;
  }

  if (forceFullRender) {
    canvas.fillRect(0,
                    theme::UiTheme::Metrics::TopBarH,
                    theme::UiTheme::Metrics::ScreenW,
                    theme::UiTheme::Metrics::ScreenH -
                        theme::UiTheme::Metrics::TopBarH - theme::UiTheme::Metrics::BottomNavH,
                    theme::UiTheme::Colors::Bg);
    _headerCard.active = true;
    _headerCard.draw(canvas);
  }

  if (previewDirty) {
    canvas.fillRect(124, 40, 84, 84, theme::UiTheme::Colors::Bg);
    _ringsPreview.draw(canvas, snapshot);
  }

  if (chipsDirty) {
    canvas.fillRect(12, 96, 296, 26, theme::UiTheme::Colors::Bg);
    for (int i = 0; i < TRACK_COUNT; ++i) {
      _trackChips[i].active = (i == snapshot.activeTrack);
      _trackChips[i].selected = (i == snapshot.activeTrack);
      _trackChips[i].muted = snapshot.trackMutes[i];
      _trackChips[i].draw(canvas);
    }
  }

  for (int i = 0; i < 4; ++i) {
    if (!rowDirty[i]) {
      continue;
    }

    canvas.fillRect(_rows[i].rowRect.x, _rows[i].rowRect.y, _rows[i].rowRect.w, _rows[i].rowRect.h, theme::UiTheme::Colors::Bg);

    char valueBuffer[16];
    if (i == 0) {
      snprintf(valueBuffer, sizeof(valueBuffer), "%d", snapshot.trackSteps[snapshot.activeTrack]);
    } else if (i == 1) {
      snprintf(valueBuffer, sizeof(valueBuffer), "%d", snapshot.trackHits[snapshot.activeTrack]);
    } else if (i == 2) {
      snprintf(valueBuffer, sizeof(valueBuffer), "%d", snapshot.trackRotations[snapshot.activeTrack]);
    } else {
      formatPercent(valueBuffer, sizeof(valueBuffer), static_cast<int>(snapshot.voiceGain[snapshot.activeTrack] * 100.0f));
    }

    _rows[i].focus = (_holdRow == i);
    _rows[i].minusPressed = (_holdRow == i && _holdDirection < 0);
    _rows[i].plusPressed = (_holdRow == i && _holdDirection > 0);
    _rows[i].valueText = valueBuffer;
    _rows[i].barFill = (i == 3) ? static_cast<uint8_t>(snapshot.voiceGain[snapshot.activeTrack] * 100.0f) : 0;
    _rows[i].draw(canvas);
  }

  if (actionButtonsDirty) {
    _randomButton.draw(canvas);
    _clearButton.draw(canvas);
  }

  _lastSnapshot = snapshot;
  _hasLastSnapshot = true;
  _lastHoldRow = _holdRow;
  _lastHoldDirection = _holdDirection;
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
  // Não forçar _dirty aqui: o render incremental usa diff de snapshot/hold state
  // e evita forceFullRender (flicker) a cada tick de hold.
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
  // Importante: não marcar _dirty durante interação normal.
  // O UiApp já agenda panelDirty quando há toque consumido e o render incremental
  // decide as sub-regiões por comparação de estado.

  if (tp.justReleased) {
    if (_holdRow >= 0) {
      consumed = true;
    }
    stopHold();
  }

  if (tp.justPressed) {
    for (int i = 0; i < TRACK_COUNT; ++i) {
      if (_trackChips[i].hitTest(tp.x, tp.y)) {
        dispatchUiAction(UiActionType::SELECT_TRACK, 0, i);
        _ringsPreview.invalidateAll();
        return true;
      }
    }

    for (int i = 0; i < 4; ++i) {
      if (_rows[i].hitMinus(tp.x, tp.y)) {
        dispatchRowDelta(snapshot, i, -1);
        startHold(i, -1);
        _ringsPreview.invalidateAll();
        return true;
      }

      if (_rows[i].hitPlus(tp.x, tp.y)) {
        dispatchRowDelta(snapshot, i, +1);
        startHold(i, +1);
        _ringsPreview.invalidateAll();
        return true;
      }
    }

    if (_randomButton.hitTest(tp.x, tp.y)) {
      dispatchUiAction(UiActionType::RANDOMIZE_TRACK, snapshot.activeTrack, 0);
      _ringsPreview.invalidateAll();
      return true;
    }

    if (_clearButton.hitTest(tp.x, tp.y)) {
      dispatchUiAction(UiActionType::SET_HITS, snapshot.activeTrack, 0);
      dispatchUiAction(UiActionType::SET_ROTATION, snapshot.activeTrack, 0);
      _ringsPreview.invalidateAll();
      return true;
    }
  }

  consumed = handleHoldTick(tp, snapshot) || consumed;
  return consumed;
}

void PatternScreen::invalidate() {
  _dirty = true;
  _hasLastSnapshot = false;
  _ringsPreview.invalidateAll();
}

} // namespace ui
