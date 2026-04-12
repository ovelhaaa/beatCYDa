#include "PatternScreen.h"

#include "../core/UiActions.h"
#include "../core/BassUiFormat.h"
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

bool isBassTrack(const UiStateSnapshot &snapshot) {
  return snapshot.activeTrack == VOICE_BASS;
}

bool bassParamChanged(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs, int rowIndex) {
  if (rowIndex == 0) {
    return lhs.bassParams.motifIndex != rhs.bassParams.motifIndex;
  }
  if (rowIndex == 1) {
    return lhs.bassParams.swing != rhs.bassParams.swing;
  }
  if (rowIndex == 2) {
    return lhs.bassParams.ghostProb != rhs.bassParams.ghostProb;
  }
  return lhs.bassParams.accentProb != rhs.bassParams.accentProb;
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
  _copyButton.label = "COPY";
  _copyButton.variant = UiButtonVariant::Secondary;
  _pasteButton.label = "PASTE";
  _pasteButton.variant = UiButtonVariant::Primary;
  _toolsButton.label = "TOOLS";
  _toolsButton.variant = UiButtonVariant::Secondary;

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
    const int y = 126 + (i * 18);
    setRect(_rows[i].rowRect, 12, y, 296, 16);
    setRect(_rows[i].minusRect, 220, y, 20, 16);
    setRect(_rows[i].plusRect, 244, y, 20, 16);
  }

  setRect(_toolsButton.rect,
          theme::UiTheme::Metrics::PatternToolsButtonX,
          theme::UiTheme::Metrics::PatternToolsButtonY,
          theme::UiTheme::Metrics::PatternToolsButtonW,
          theme::UiTheme::Metrics::PatternToolsButtonH);
  setRect(_toolsModalRect,
          theme::UiTheme::Metrics::PatternToolsModalX,
          theme::UiTheme::Metrics::PatternToolsModalY,
          theme::UiTheme::Metrics::PatternToolsModalW,
          theme::UiTheme::Metrics::PatternToolsModalH);
  setRect(_randomButton.rect,
          theme::UiTheme::Metrics::PatternToolsActionX1,
          theme::UiTheme::Metrics::PatternToolsActionY1,
          theme::UiTheme::Metrics::PatternToolsActionW,
          theme::UiTheme::Metrics::PatternToolsActionH);
  setRect(_clearButton.rect,
          theme::UiTheme::Metrics::PatternToolsActionX2,
          theme::UiTheme::Metrics::PatternToolsActionY1,
          theme::UiTheme::Metrics::PatternToolsActionW,
          theme::UiTheme::Metrics::PatternToolsActionH);
  setRect(_copyButton.rect,
          theme::UiTheme::Metrics::PatternToolsActionX1,
          theme::UiTheme::Metrics::PatternToolsActionY2,
          theme::UiTheme::Metrics::PatternToolsActionW,
          theme::UiTheme::Metrics::PatternToolsActionH);
  setRect(_pasteButton.rect,
          theme::UiTheme::Metrics::PatternToolsActionX2,
          theme::UiTheme::Metrics::PatternToolsActionY2,
          theme::UiTheme::Metrics::PatternToolsActionW,
          theme::UiTheme::Metrics::PatternToolsActionH);
}

void PatternScreen::render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) {
  const bool isBassTrackActive = snapshot.activeTrack == VOICE_BASS;
  const bool forceFullRender = _dirty || !_hasLastSnapshot;
  const bool trackChanged = forceFullRender || snapshot.activeTrack != _lastSnapshot.activeTrack;
  const bool bassContext = isBassTrack(snapshot);
  const bool bassContextChanged = forceFullRender || (isBassTrack(snapshot) != isBassTrack(_lastSnapshot));

  _rows[2].label = bassContext ? "MODE" : "ROTATE";
  _rows[3].label = bassContext ? "MOTIF" : "GAIN";

  const bool chipsDirty = forceFullRender || trackChanged || hasMuteChanges(snapshot, _lastSnapshot);
  const bool previewDirty =
      forceFullRender || trackChanged || snapshot.currentStep != _lastSnapshot.currentStep || hasPatternChanges(snapshot, _lastSnapshot);
  bool rowDirty[4] = {forceFullRender || trackChanged || bassContextChanged,
                      forceFullRender || trackChanged || bassContextChanged,
                      forceFullRender || trackChanged || bassContextChanged,
                      forceFullRender || trackChanged || bassContextChanged};
  const bool actionButtonsDirty = forceFullRender || _pasteButton.disabled != !_clipboard.hasData;

  for (int i = 0; i < 4; ++i) {
    if (rowDirty[i]) {
      continue;
    }

    bool valueChanged = false;
    if (isBassTrackActive) {
      valueChanged = bassParamChanged(snapshot, _lastSnapshot, i);
    } else if (i == 0) {
      valueChanged = snapshot.trackSteps[snapshot.activeTrack] != _lastSnapshot.trackSteps[_lastSnapshot.activeTrack];
    } else if (i == 1) {
      valueChanged = snapshot.trackHits[snapshot.activeTrack] != _lastSnapshot.trackHits[_lastSnapshot.activeTrack];
    } else if (i == 2) {
      valueChanged = bassContext ? (snapshot.bassParams.mode != _lastSnapshot.bassParams.mode)
                                 : (snapshot.trackRotations[snapshot.activeTrack] !=
                                    _lastSnapshot.trackRotations[_lastSnapshot.activeTrack]);
    } else {
      valueChanged = bassContext ? (snapshot.bassParams.motifIndex != _lastSnapshot.bassParams.motifIndex)
                                 : (snapshot.voiceGain[snapshot.activeTrack] !=
                                    _lastSnapshot.voiceGain[_lastSnapshot.activeTrack]);
    }

    const bool focusChanged = ((_holdRow == i) != (_lastHoldRow == i));
    const bool minusChanged = ((_holdRow == i && _holdDirection < 0) != (_lastHoldRow == i && _lastHoldDirection < 0));
    const bool plusChanged = ((_holdRow == i && _holdDirection > 0) != (_lastHoldRow == i && _lastHoldDirection > 0));
    rowDirty[i] = valueChanged || focusChanged || minusChanged || plusChanged;
  }

  if (forceFullRender) {
    canvas.fillRect(0,
                    theme::UiTheme::Metrics::ContentTop,
                    theme::UiTheme::Metrics::ScreenW,
                    theme::UiTheme::Metrics::ContentH,
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
    if (isBassTrackActive && i == 0) {
      _rows[i].label = "MOTIF";
      snprintf(valueBuffer, sizeof(valueBuffer), "%u", snapshot.bassParams.motifIndex);
    } else if (isBassTrackActive && i == 1) {
      _rows[i].label = "SWING";
      formatPercent(valueBuffer, sizeof(valueBuffer), static_cast<int>(snapshot.bassParams.swing * 100.0f));
    } else if (isBassTrackActive && i == 2) {
      _rows[i].label = "GHOST";
      formatPercent(valueBuffer, sizeof(valueBuffer), static_cast<int>(snapshot.bassParams.ghostProb * 100.0f));
    } else if (isBassTrackActive) {
      _rows[i].label = "ACCENT";
      formatPercent(valueBuffer, sizeof(valueBuffer), static_cast<int>(snapshot.bassParams.accentProb * 100.0f));
    } else if (i == 0) {
      _rows[i].label = "STEPS";
      snprintf(valueBuffer, sizeof(valueBuffer), "%d", snapshot.trackSteps[snapshot.activeTrack]);
    } else if (i == 1) {
      _rows[i].label = "HITS";
      snprintf(valueBuffer, sizeof(valueBuffer), "%d", snapshot.trackHits[snapshot.activeTrack]);
    } else if (i == 2) {
      _rows[i].label = "ROTATE";
      snprintf(valueBuffer, sizeof(valueBuffer), "%d", snapshot.trackRotations[snapshot.activeTrack]);
    } else {
      _rows[i].label = "GAIN";
      formatPercent(valueBuffer, sizeof(valueBuffer), static_cast<int>(snapshot.voiceGain[snapshot.activeTrack] * 100.0f));
    }

    _rows[i].focus = (_holdRow == i);
    _rows[i].minusPressed = (_holdRow == i && _holdDirection < 0);
    _rows[i].plusPressed = (_holdRow == i && _holdDirection > 0);
    _rows[i].valueText = valueBuffer;
    _rows[i].showBar = (isBassTrackActive && i > 0) || (!isBassTrackActive && i == 3);
    _rows[i].barFill = 0;
    if (isBassTrackActive && i == 1) {
      _rows[i].barFill = static_cast<uint8_t>(snapshot.bassParams.swing * 100.0f);
    } else if (isBassTrackActive && i == 2) {
      _rows[i].barFill = static_cast<uint8_t>(snapshot.bassParams.ghostProb * 100.0f);
    } else if (isBassTrackActive && i == 3) {
      _rows[i].barFill = static_cast<uint8_t>(snapshot.bassParams.accentProb * 100.0f);
    } else if (!isBassTrackActive && i == 3) {
      _rows[i].barFill = static_cast<uint8_t>(snapshot.voiceGain[snapshot.activeTrack] * 100.0f);
    }
    _rows[i].draw(canvas);
  }

  if (actionButtonsDirty) {
    _pasteButton.disabled = !_clipboard.hasData;
    _toolsButton.draw(canvas);
  }

  if (_toolsModalVisible) {
    canvas.fillRect(0,
                    theme::UiTheme::Metrics::ContentTop,
                    theme::UiTheme::Metrics::ScreenW,
                    theme::UiTheme::Metrics::ContentH,
                    theme::UiTheme::Colors::ModalScrim);
    canvas.fillRoundRect(_toolsModalRect.x,
                         _toolsModalRect.y,
                         _toolsModalRect.w,
                         _toolsModalRect.h,
                         theme::UiTheme::Metrics::RadiusMd,
                         theme::UiTheme::Colors::Surface);
    canvas.drawRoundRect(_toolsModalRect.x,
                         _toolsModalRect.y,
                         _toolsModalRect.w,
                         _toolsModalRect.h,
                         theme::UiTheme::Metrics::RadiusMd,
                         theme::UiTheme::Colors::Outline);
    canvas.setTextColor(theme::UiTheme::Colors::TextPrimary, theme::UiTheme::Colors::Surface);
    canvas.setTextSize(theme::UiTheme::Typography::BodySize);
    canvas.setCursor(_toolsModalRect.x + 8, _toolsModalRect.y + 12);
    canvas.print("TOOLS");
    _randomButton.draw(canvas);
    _clearButton.draw(canvas);
    _copyButton.draw(canvas);
    _pasteButton.draw(canvas);
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
  if (snapshot.activeTrack == VOICE_BASS) {
    if (rowIndex == 0) {
      dispatchUiAction(UiActionType::SET_BASS_PARAM, static_cast<int>(BassParamId::MOTIF_INDEX),
                       static_cast<int>(snapshot.bassParams.motifIndex) * 25 + amount);
      return;
    }
    if (rowIndex == 1) {
      dispatchUiAction(UiActionType::SET_BASS_PARAM, static_cast<int>(BassParamId::SWING),
                       static_cast<int>(snapshot.bassParams.swing * 100.0f) + amount);
      return;
    }
    if (rowIndex == 2) {
      dispatchUiAction(UiActionType::SET_BASS_PARAM, static_cast<int>(BassParamId::GHOST_PROB),
                       static_cast<int>(snapshot.bassParams.ghostProb * 100.0f) + amount);
      return;
    }
    dispatchUiAction(UiActionType::SET_BASS_PARAM, static_cast<int>(BassParamId::ACCENT_PROB),
                     static_cast<int>(snapshot.bassParams.accentProb * 100.0f) + amount);
    return;
  }

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

  dispatchUiAction(UiActionType::SET_VOICE_GAIN, snapshot.activeTrack,
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
    if (_toolsModalVisible) {
      if (_randomButton.hitTest(tp.x, tp.y)) {
        dispatchUiAction(UiActionType::RANDOMIZE_TRACK, snapshot.activeTrack, 0);
        _ringsPreview.invalidateAll();
        _toolsModalVisible = false;
        _dirty = true;
        return true;
      }
      if (_clearButton.hitTest(tp.x, tp.y)) {
        dispatchUiAction(UiActionType::SET_HITS, snapshot.activeTrack, 0);
        dispatchUiAction(UiActionType::SET_ROTATION, snapshot.activeTrack, 0);
        _ringsPreview.invalidateAll();
        _toolsModalVisible = false;
        _dirty = true;
        return true;
      }
      if (_copyButton.hitTest(tp.x, tp.y)) {
        copyActiveTrack(snapshot);
        _toolsModalVisible = false;
        _dirty = true;
        return true;
      }
      if (_pasteButton.hitTest(tp.x, tp.y) && _clipboard.hasData) {
        pasteToActiveTrack(snapshot);
        _ringsPreview.invalidateAll();
        _toolsModalVisible = false;
        _dirty = true;
        return true;
      }

      _toolsModalVisible = false;
      _dirty = true;
      return true;
    }

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

    if (_toolsButton.hitTest(tp.x, tp.y)) {
      _toolsModalVisible = true;
      _dirty = true;
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

void PatternScreen::copyActiveTrack(const UiStateSnapshot &snapshot) {
  const uint8_t track = snapshot.activeTrack;
  _clipboard.steps = snapshot.trackSteps[track];
  _clipboard.hits = snapshot.trackHits[track];
  _clipboard.rotation = snapshot.trackRotations[track];
  _clipboard.gainPercent = static_cast<uint8_t>(snapshot.voiceGain[track] * 100.0f);
  _clipboard.hasData = true;
}

void PatternScreen::pasteToActiveTrack(const UiStateSnapshot &snapshot) {
  if (!_clipboard.hasData) {
    return;
  }

  const uint8_t track = snapshot.activeTrack;
  dispatchUiAction(UiActionType::SET_STEPS, track, _clipboard.steps);
  dispatchUiAction(UiActionType::SET_HITS, track, _clipboard.hits);
  dispatchUiAction(UiActionType::SET_ROTATION, track, _clipboard.rotation);
  dispatchUiAction(UiActionType::SET_SOUND_PARAM, 3, _clipboard.gainPercent);
}

} // namespace ui
