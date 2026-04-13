#include "SoundScreen.h"

#include "../core/BassUiFormat.h"
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

const char *bassTabName(uint8_t page) {
  if (page == 0) {
    return "Bass <A>";
  }
  if (page == 1) {
    return "Bass <B>";
  }
  return "Bass <C>";
}

const char *soundHeaderLabel(const UiStateSnapshot &snapshot, uint8_t bassPage) {
  if (snapshot.activeTrack == VOICE_BASS) {
    return bassTabName(bassPage);
  }
  return trackName(snapshot.activeTrack);
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
  applyLayoutMode(_layoutIsBass);
}

void SoundScreen::applyLayoutMode(bool bassLayout) {
  _layoutIsBass = bassLayout;
  setRect(_headerRect, 12, 46, 130, 24);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    setRect(_trackChips[i].rect, 150 + (i * 32), 48, 30, 20);
  }

  setRect(_soundTypeChipRect, 278, 48, 30, 20);

  const int bassTabsY = bassLayout ? 70 : 48;
  const int bassTabsH = bassLayout ? 16 : 20;
  setRect(_bassNavLeftRect, 210, bassTabsY, 20, bassTabsH);
  setRect(_bassNavCenterRect, 232, bassTabsY, 54, bassTabsH);
  setRect(_bassNavRightRect, 288, bassTabsY, 20, bassTabsH);

  constexpr int rowX = 12;
  constexpr int rowW = 296;
  constexpr int rowH = 24;

  for (int i = 0; i < 4; ++i) {
    const int y = 86 + (i * 25);
    setRect(_rows[i].rowRect, rowX, y, rowW, rowH);
  }
}

void SoundScreen::render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) {
  const bool forceFullRender = _dirty || !_hasLastSnapshot;
  const bool trackChanged = forceFullRender || snapshot.activeTrack != _lastSnapshot.activeTrack;
  const bool bassTrack = isBassTrack(snapshot);
  if (forceFullRender || (_layoutIsBass != bassTrack)) {
    applyLayoutMode(bassTrack);
  }
  if (trackChanged && snapshot.activeTrack != VOICE_BASS) {
    _bassPage = 0;
  }

  const bool pageChanged = forceFullRender || (_bassPage != _lastBassPage);
  applyRowLabels(snapshot);

  const bool identityDirty = forceFullRender || trackChanged || pageChanged;
  const bool chipsDirty = forceFullRender || trackChanged || hasMuteChanges(snapshot, _lastSnapshot) || pageChanged;
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

    rowDirty[i] = valueChanged;
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
    canvas.fillRect(_headerRect.x,
                    _headerRect.y,
                    _headerRect.w,
                    _headerRect.h,
                    theme::UiTheme::Colors::Bg);

    const uint16_t fill = theme::UiTheme::Colors::Surface;
    canvas.fillRoundRect(_headerRect.x,
                         _headerRect.y,
                         _headerRect.w,
                         _headerRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         fill);
    canvas.drawRoundRect(_headerRect.x,
                         _headerRect.y,
                         _headerRect.w,
                         _headerRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::Outline);
    canvas.setTextSize(theme::UiTheme::Typography::BodySize);
    canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, fill);
    canvas.setCursor(_headerRect.x + 7, _headerRect.y + 7);
    canvas.printf("SOUND / %s", soundHeaderLabel(snapshot, _bassPage));
  }

  if (chipsDirty) {
    canvas.fillRect(150, 46, 158, 42, theme::UiTheme::Colors::Bg);
    for (int i = 0; i < TRACK_COUNT; ++i) {
      _trackChips[i].active = (i == snapshot.activeTrack);
      _trackChips[i].selected = (i == snapshot.activeTrack);
      _trackChips[i].muted = snapshot.trackMutes[i];
      _trackChips[i].draw(canvas);
    }

    if (!bassTrack) {
      const uint16_t fill = theme::UiTheme::Colors::Surface;
      canvas.fillRoundRect(_soundTypeChipRect.x,
                           _soundTypeChipRect.y,
                           _soundTypeChipRect.w,
                           _soundTypeChipRect.h,
                           theme::UiTheme::Metrics::RadiusSm,
                           fill);
      canvas.drawRoundRect(_soundTypeChipRect.x,
                           _soundTypeChipRect.y,
                           _soundTypeChipRect.w,
                           _soundTypeChipRect.h,
                           theme::UiTheme::Metrics::RadiusSm,
                           theme::UiTheme::Colors::Outline);
      canvas.setTextSize(theme::UiTheme::Typography::BodySize);
      canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, fill);
      canvas.setCursor(_soundTypeChipRect.x + 5, _soundTypeChipRect.y + 7);
      canvas.print("TYPE");
    } else {
      const uint16_t arrowFill = theme::UiTheme::Colors::Surface;
      const uint16_t arrowText = theme::UiTheme::Colors::TextSecondary;
      canvas.fillRoundRect(_bassNavLeftRect.x,
                           _bassNavLeftRect.y,
                           _bassNavLeftRect.w,
                           _bassNavLeftRect.h,
                           theme::UiTheme::Metrics::RadiusSm,
                           arrowFill);
      canvas.drawRoundRect(_bassNavLeftRect.x,
                           _bassNavLeftRect.y,
                           _bassNavLeftRect.w,
                           _bassNavLeftRect.h,
                           theme::UiTheme::Metrics::RadiusSm,
                           theme::UiTheme::Colors::Outline);
      canvas.setTextSize(theme::UiTheme::Typography::BodySize);
      canvas.setTextColor(arrowText, arrowFill);
      canvas.setCursor(_bassNavLeftRect.x + 7, _bassNavLeftRect.y + 5);
      canvas.print("<");

      const uint16_t centerFill = theme::UiTheme::Colors::Accent;
      canvas.fillRoundRect(_bassNavCenterRect.x,
                           _bassNavCenterRect.y,
                           _bassNavCenterRect.w,
                           _bassNavCenterRect.h,
                           theme::UiTheme::Metrics::RadiusSm,
                           centerFill);
      canvas.drawRoundRect(_bassNavCenterRect.x,
                           _bassNavCenterRect.y,
                           _bassNavCenterRect.w,
                           _bassNavCenterRect.h,
                           theme::UiTheme::Metrics::RadiusSm,
                           theme::UiTheme::Colors::Accent);
      canvas.setTextColor(theme::UiTheme::Colors::TextOnAccent, centerFill);
      canvas.setCursor(_bassNavCenterRect.x + 8, _bassNavCenterRect.y + 5);
      canvas.print(static_cast<char>('A' + _bassPage));

      canvas.fillRoundRect(_bassNavRightRect.x,
                           _bassNavRightRect.y,
                           _bassNavRightRect.w,
                           _bassNavRightRect.h,
                           theme::UiTheme::Metrics::RadiusSm,
                           arrowFill);
      canvas.drawRoundRect(_bassNavRightRect.x,
                           _bassNavRightRect.y,
                           _bassNavRightRect.w,
                           _bassNavRightRect.h,
                           theme::UiTheme::Metrics::RadiusSm,
                           theme::UiTheme::Colors::Outline);
      canvas.setTextColor(arrowText, arrowFill);
      canvas.setCursor(_bassNavRightRect.x + 7, _bassNavRightRect.y + 5);
      canvas.print(">");
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

    _rows[i].focus = false;
    _rows[i].valueText = value;
    _rows[i].barFill = static_cast<uint8_t>(constrain(rowValue, 0.0f, 1.0f) * 100.0f);
    _rows[i].draw(canvas);
  }

  _lastSnapshot = snapshot;
  _hasLastSnapshot = true;
  _lastBassPage = _bassPage;
  _dirty = false;
}

void SoundScreen::dispatchRowValue(const UiStateSnapshot &snapshot, int rowIndex, int percent) {
    const int value = constrain(percent, 0, 100);
    if (isBassTrack(snapshot)) {
      int paramIdx = 0;

      if (_bassPage == 0) {
      switch (rowIndex) {
      case 0:
        paramIdx = 3;
        break;
        case 1:
          paramIdx = 2;
          break;
        case 2:
          paramIdx = 4;
        break;
      case 3:
      default:
        paramIdx = 0;
        break;
      }
      } else if (_bassPage == 1) {
        switch (rowIndex) {
        case 0:
          paramIdx = 1;
        break;
      case 1:
        paramIdx = 5;
        break;
      case 2:
        paramIdx = 6;
        break;
        case 3:
        default:
          paramIdx = 7;
          break;
        }
      } else {
        switch (rowIndex) {
        case 0:
          paramIdx = 8;
          break;
        case 1:
          paramIdx = 9;
          break;
        case 2:
          paramIdx = 10;
          break;
        case 3:
        default:
          paramIdx = 0;
          break;
        }
      }

    dispatchUiAction(UiActionType::SET_BASS_PARAM, paramIdx, value);
    return;
  }
  dispatchUiAction(UiActionType::SET_SOUND_PARAM, rowIndex, value);
}

void SoundScreen::cycleBassPage(int8_t direction) {
  if (direction == 0) {
    return;
  }

  constexpr int kBassPageCount = 3;
  int nextPage = static_cast<int>(_bassPage) + static_cast<int>(direction);
  while (nextPage < 0) {
    nextPage += kBassPageCount;
  }
  nextPage %= kBassPageCount;

  const uint8_t wrappedPage = static_cast<uint8_t>(nextPage);
  if (_bassPage != wrappedPage) {
    _bassPage = wrappedPage;
    _dirty = true;
  }
}

bool SoundScreen::handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) {
  if (tp.justPressed) {
    if (isBassTrack(snapshot)) {
      if (_bassNavLeftRect.contains(tp.x, tp.y)) {
        cycleBassPage(-1);
        return true;
      }
      if (_bassNavRightRect.contains(tp.x, tp.y)) {
        cycleBassPage(1);
        return true;
      }
    }

    for (int i = 0; i < TRACK_COUNT; ++i) {
      if (_trackChips[i].hitTest(tp.x, tp.y)) {
        dispatchUiAction(UiActionType::SELECT_TRACK, 0, i);
        return true;
      }
    }
  }

  if (tp.pressed && (tp.justPressed || tp.dragging)) {
    for (int i = 0; i < 4; ++i) {
      if (_rows[i].hitSlider(tp.x, tp.y)) {
        dispatchRowValue(snapshot, i, _rows[i].sliderPercentAt(tp.x));
        return true;
      }
    }
  }

  return false;
}

void SoundScreen::invalidate() {
  _dirty = true;
  _hasLastSnapshot = false;
}

} // namespace ui
