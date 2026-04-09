#include "SoundScreen.h"

#include "../core/BassUiFormat.h"
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

void formatRootNote(char *buffer, size_t size, uint8_t note) {
  snprintf(buffer, size, "%s%d", bassfmt::noteName(note), bassfmt::noteOctave(note));
}

float regularSoundRowValue(const UiStateSnapshot &snapshot, int rowIndex) {
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

float bassPageRowValue(const UiStateSnapshot &snapshot, int page, int rowIndex) {
  const BassGrooveParams &bp = snapshot.bassParams;
  if (page == 0) {
    switch (rowIndex) {
    case 0:
      return (bp.rootNote - 24.0f) / 24.0f;
    case 1:
      return static_cast<float>(static_cast<uint8_t>(bp.scaleType)) / 3.0f;
    case 2:
      return static_cast<float>(static_cast<uint8_t>(bp.mode)) / 3.0f;
    case 3:
    default:
      return bp.density;
    }
  }
  if (page == 1) {
    switch (rowIndex) {
    case 0:
      return (bp.range - 1.0f) / 11.0f;
    case 1:
      return static_cast<float>(bp.motifIndex & 0x03) / 3.0f;
    case 2:
      return bp.swing;
    default:
      return bp.accentProb;
    }
  }

  switch (rowIndex) {
  case 0:
    return bp.ghostProb;
  case 1:
    return bp.phraseVariation;
  case 2:
    return bp.slideProb;
  default:
    return bp.density;
  }
}

void bassRowValueText(char *buffer, size_t size, const UiStateSnapshot &snapshot, int page,
                      int rowIndex) {
  const BassGrooveParams &bp = snapshot.bassParams;
  if (page == 0) {
    switch (rowIndex) {
    case 0:
      formatRootNote(buffer, size, bp.rootNote);
      return;
    case 1:
      snprintf(buffer, size, "%s", bassfmt::scaleShortName(bp.scaleType));
      return;
    case 2:
      snprintf(buffer, size, "%s", bassfmt::modeShortName(bp.mode));
      return;
    default:
      formatPercent(buffer, size, bp.density);
      return;
    }
  }

  if (page == 1) {
    switch (rowIndex) {
    case 0:
      snprintf(buffer, size, "%d", bp.range);
      return;
    case 1:
      snprintf(buffer, size, "M%u", static_cast<unsigned>(bp.motifIndex & 0x03));
      return;
    case 2:
      formatPercent(buffer, size, bp.swing);
      return;
    default:
      formatPercent(buffer, size, bp.accentProb);
      return;
    }
  }

  switch (rowIndex) {
  case 0:
    formatPercent(buffer, size, bp.ghostProb);
    return;
  case 1:
    formatPercent(buffer, size, bp.phraseVariation);
    return;
  case 2:
    formatPercent(buffer, size, bp.slideProb);
    return;
  default:
    formatPercent(buffer, size, bp.density);
    return;
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

bool SoundScreen::isBassTrack(const UiStateSnapshot &snapshot) const {
  return snapshot.activeTrack == VOICE_BASS;
}

void SoundScreen::applyRowLabels(const UiStateSnapshot &snapshot) {
  if (!isBassTrack(snapshot)) {
    _rows[0].label = "PITCH";
    _rows[1].label = "DECAY";
    _rows[2].label = "TIMBRE";
    _rows[3].label = "DRIVE";
    return;
  }

  if (_bassPage == 0) {
    _rows[0].label = "ROOT";
    _rows[1].label = "SCALE";
    _rows[2].label = "MODE";
    _rows[3].label = "DENSITY";
  } else if (_bassPage == 1) {
    _rows[0].label = "RANGE";
    _rows[1].label = "MOTIF";
    _rows[2].label = "SWING";
    _rows[3].label = "ACCENT";
  } else {
    _rows[0].label = "GHOST";
    _rows[1].label = "PHRASE";
    _rows[2].label = "SLIDE";
    _rows[3].label = "DENSITY";
  }
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
  if (trackChanged && snapshot.activeTrack != VOICE_BASS) {
    _bassPage = 0;
  }

  const bool pageChanged = forceFullRender || (_bassPage != _lastBassPage);
  applyRowLabels(snapshot);

  const bool identityDirty = forceFullRender || trackChanged || pageChanged;
  const bool chipsDirty = forceFullRender || trackChanged || hasMuteChanges(snapshot, _lastSnapshot);
  bool rowDirty[4] = {forceFullRender || trackChanged || pageChanged,
                      forceFullRender || trackChanged || pageChanged,
                      forceFullRender || trackChanged || pageChanged,
                      forceFullRender || trackChanged || pageChanged};

  for (int i = 0; i < 4; ++i) {
    if (rowDirty[i]) {
      continue;
    }

    const float currentValue = isBassTrack(snapshot) ? bassPageRowValue(snapshot, _bassPage, i)
                                                     : regularSoundRowValue(snapshot, i);
    const float previousValue = isBassTrack(_lastSnapshot) ? bassPageRowValue(_lastSnapshot, _lastBassPage, i)
                                                           : regularSoundRowValue(_lastSnapshot, i);
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

    if (isBassTrack(snapshot)) {
      _identityCard.value = (_bassPage == 0) ? "BASS A" : (_bassPage == 1) ? "BASS B" : "BASS C";
    } else {
      _identityCard.value = trackName(snapshot.activeTrack);
    }
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
    float rowValue = 0.0f;
    if (isBassTrack(snapshot)) {
      rowValue = bassPageRowValue(snapshot, _bassPage, i);
      bassRowValueText(value, sizeof(value), snapshot, _bassPage, i);
    } else {
      rowValue = regularSoundRowValue(snapshot, i);
      formatPercent(value, sizeof(value), rowValue);
    }

    _rows[i].focus = (_holdRow == i);
    _rows[i].minusPressed = (_holdRow == i && _holdDirection < 0);
    _rows[i].plusPressed = (_holdRow == i && _holdDirection > 0);
    _rows[i].valueText = value;
    _rows[i].barFill = static_cast<uint8_t>(constrain(rowValue, 0.0f, 1.0f) * 100.0f);
    _rows[i].draw(canvas);
  }

  _lastSnapshot = snapshot;
  _hasLastSnapshot = true;
  _lastHoldRow = _holdRow;
  _lastHoldDirection = _holdDirection;
  _lastBassPage = _bassPage;
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
  if (isBassTrack(snapshot)) {
    BassGrooveParams bp = snapshot.bassParams;
    int paramIdx = 0;
    int currentValue = 0;

    if (_bassPage == 0) {
      switch (rowIndex) {
      case 0:
        paramIdx = 3;
        currentValue = static_cast<int>(((bp.rootNote - 24.0f) / 24.0f) * 100.0f);
        break;
      case 1:
        paramIdx = 2;
        currentValue = static_cast<int>(static_cast<uint8_t>(bp.scaleType) * (100.0f / 3.0f));
        break;
      case 2:
        paramIdx = 4;
        currentValue = static_cast<int>(static_cast<uint8_t>(bp.mode) * (100.0f / 3.0f));
        break;
      case 3:
      default:
        paramIdx = 0;
        currentValue = static_cast<int>(bp.density * 100.0f);
        break;
      }
    } else if (_bassPage == 1) {
      switch (rowIndex) {
      case 0:
        paramIdx = 1;
        currentValue = static_cast<int>(((bp.range - 1.0f) / 11.0f) * 100.0f);
        break;
      case 1:
        paramIdx = 5;
        currentValue = static_cast<int>((bp.motifIndex & 0x03) * (100.0f / 3.0f));
        break;
      case 2:
        paramIdx = 6;
        currentValue = static_cast<int>(bp.swing * 100.0f);
        break;
      case 3:
      default:
        paramIdx = 7;
        currentValue = static_cast<int>(bp.accentProb * 100.0f);
        break;
      }
    } else {
      switch (rowIndex) {
      case 0:
        paramIdx = 8;
        currentValue = static_cast<int>(bp.ghostProb * 100.0f);
        break;
      case 1:
        paramIdx = 9;
        currentValue = static_cast<int>(bp.phraseVariation * 100.0f);
        break;
      case 2:
        paramIdx = 10;
        currentValue = static_cast<int>(bp.slideProb * 100.0f);
        break;
      case 3:
      default:
        paramIdx = 0;
        currentValue = static_cast<int>(bp.density * 100.0f);
        break;
      }
    }

    dispatchUiAction(UiActionType::SET_BASS_PARAM, paramIdx, currentValue + amount);
    return;
  }

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
  return true;
}

bool SoundScreen::handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) {
  bool consumed = false;

  if (tp.justReleased) {
    if (_holdRow >= 0) {
      consumed = true;
    }
    stopHold();
  }

  if (tp.justPressed) {
    if (isBassTrack(snapshot) && _identityCard.rect.contains(tp.x, tp.y)) {
      _bassPage = static_cast<uint8_t>((_bassPage + 1) % 3);
      _dirty = true;
      return true;
    }

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