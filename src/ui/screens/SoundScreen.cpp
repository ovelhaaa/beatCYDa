#include "SoundScreen.h"

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

void formatPercent(char *buffer, size_t size, float value) {
  snprintf(buffer, size, "%d%%", static_cast<int>(value * 100.0f));
}

bool hasMuteChanges(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  for (int i = 0; i < TRACK_COUNT; ++i) {
    if (lhs.trackMutes[i] != rhs.trackMutes[i]) {
      return true;
    }
  }
  return false;
}

float rowValueFromSnapshot(const UiStateSnapshot &snapshot, int rowIndex) {
  switch (rowIndex) {
  case 1:
    return snapshot.voiceParams[snapshot.activeTrack].decay;
  case 2:
    return snapshot.voiceParams[snapshot.activeTrack].timbre;
  case 3:
    return snapshot.voiceParams[snapshot.activeTrack].drive;
  default:
    return snapshot.voiceParams[snapshot.activeTrack].pitch;
  }
}

const char *trackName(int track) {
  switch (track) {
  case 0:
    return "KICK";
  case 1:
    return "SNARE";
  case 2:
    return "CLOSED HAT";
  case 3:
    return "OPEN HAT";
  default:
    return "BASS";
  }
}
} // namespace

SoundScreen::SoundScreen() {
  _identityCard.title = "SOUND";
  _identityCard.value = "KICK";
  _identityCard.active = true;

  _rows[0].label = "PITCH";
  _rows[1].label = "DECAY";
  _rows[2].label = "TIMBRE";
  _rows[3].label = "DRIVE";

  for (int i = 0; i < 4; ++i) {
    _rows[i].showBar = true;
  }

  for (int i = 0; i < TRACK_COUNT; ++i) {
    _trackChips[i].trackIndex = static_cast<uint8_t>(i);
  }

  layout();
}

void SoundScreen::layout() {
  setRect(_identityCard.rect, 12, 46, 130, 48);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    setRect(_trackChips[i].rect, 150 + (i * 32), 54, 30, 24);
  }

  for (int i = 0; i < 4; ++i) {
    const int y = 102 + (i * 24);
    setRect(_rows[i].rowRect, 12, y, 296, 22);
    setRect(_rows[i].minusRect, 208, y + 1, 24, 20);
    setRect(_rows[i].plusRect, 236, y + 1, 24, 20);
  }
}

void SoundScreen::render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) {
  const bool forceFullRender = _dirty || !_hasLastSnapshot;
  const bool trackChanged = forceFullRender || snapshot.activeTrack != _lastSnapshot.activeTrack;

  const bool identityDirty = forceFullRender || trackChanged;
  const bool chipsDirty = forceFullRender || trackChanged || hasMuteChanges(snapshot, _lastSnapshot);
  bool rowDirty[4] = {forceFullRender || trackChanged, forceFullRender || trackChanged,
                      forceFullRender || trackChanged, forceFullRender || trackChanged};

  for (int i = 0; i < 4; ++i) {
    if (rowDirty[i]) {
      continue;
    }

    const float currentValue = rowValueFromSnapshot(snapshot, i);
    const float previousValue = rowValueFromSnapshot(_lastSnapshot, i);
    const bool valueChanged = currentValue != previousValue;

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
  }

  if (identityDirty) {
    canvas.fillRect(_identityCard.rect.x,
                    _identityCard.rect.y,
                    _identityCard.rect.w,
                    _identityCard.rect.h,
                    theme::UiTheme::Colors::Bg);
    _identityCard.value = trackName(snapshot.activeTrack);
    _identityCard.draw(canvas);
  }

  if (chipsDirty) {
    canvas.fillRect(150, 54, 158, 24, theme::UiTheme::Colors::Bg);
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

    char value[16];
    const float rowValue = rowValueFromSnapshot(snapshot, i);
    formatPercent(value, sizeof(value), rowValue);

    _rows[i].focus = (_holdRow == i);
    _rows[i].minusPressed = (_holdRow == i && _holdDirection < 0);
    _rows[i].plusPressed = (_holdRow == i && _holdDirection > 0);
    _rows[i].valueText = value;
    _rows[i].barFill = static_cast<uint8_t>(rowValue * 100.0f);
    _rows[i].draw(canvas);
  }

  _lastSnapshot = snapshot;
  _hasLastSnapshot = true;
  _lastHoldRow = _holdRow;
  _lastHoldDirection = _holdDirection;
  _dirty = false;
}

void SoundScreen::startHold(int rowIndex, int direction) {
  _holdRow = rowIndex;
  _holdDirection = direction;
  _holdTickCount = 0;
  _holdNextTickMs = millis() + CYDConfig::HoldRepeatStartDelayMs;
}

void SoundScreen::stopHold() {
  _holdRow = -1;
  _holdDirection = 0;
  _holdTickCount = 0;
  _holdNextTickMs = 0;
}

void SoundScreen::dispatchRowDelta(const UiStateSnapshot &snapshot, int rowIndex, int amount) {
  const int currentValue = static_cast<int>(snapshot.voiceParams[snapshot.activeTrack].pitch * 100.0f);
  if (rowIndex == 1) {
    dispatchUiAction(UiActionType::SET_SOUND_PARAM, rowIndex,
                     static_cast<int>(snapshot.voiceParams[snapshot.activeTrack].decay * 100.0f) + amount);
    return;
  }
  if (rowIndex == 2) {
    dispatchUiAction(UiActionType::SET_SOUND_PARAM, rowIndex,
                     static_cast<int>(snapshot.voiceParams[snapshot.activeTrack].timbre * 100.0f) + amount);
    return;
  }
  if (rowIndex == 3) {
    dispatchUiAction(UiActionType::SET_SOUND_PARAM, rowIndex,
                     static_cast<int>(snapshot.voiceParams[snapshot.activeTrack].drive * 100.0f) + amount);
    return;
  }

  dispatchUiAction(UiActionType::SET_SOUND_PARAM, rowIndex, currentValue + amount);
}

bool SoundScreen::handleHoldTick(const TouchPoint &tp, const UiStateSnapshot &snapshot) {
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
  // Não forçar full redraw: o render incremental detecta mudanças por diff
  // de snapshot e estado de hold, reduzindo flicker durante repetição.
  return true;
}

bool SoundScreen::handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) {
  bool consumed = false;
  // Evita marcar _dirty durante interação normal para preservar redraw parcial.
  // O UiApp já agenda panelDirty quando o toque é consumido.

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
        return true;
      }
    }

    for (int i = 0; i < 4; ++i) {
      if (_rows[i].hitMinus(tp.x, tp.y)) {
        dispatchRowDelta(snapshot, i, -1);
        startHold(i, -1);
        return true;
      }

      if (_rows[i].hitPlus(tp.x, tp.y)) {
        dispatchRowDelta(snapshot, i, +1);
        startHold(i, +1);
        return true;
      }
    }
  }

  consumed = handleHoldTick(tp, snapshot) || consumed;
  return consumed;
}

void SoundScreen::invalidate() {
  _dirty = true;
  _hasLastSnapshot = false;
}

} // namespace ui
