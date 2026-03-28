#include "Display.h"

#include "LGFX_CYD.h"
#include "UiAction.h"
#include "../control/ControlManager.h"
#include "../control/InputManager.h"
#include "../storage/PatternStorage.h"

#include <SPI.h>
#include <math.h>
#include <string.h>

LGFX_CYD tft;

namespace {

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  bool contains(int16_t px, int16_t py) const {
    return px >= x && px < (x + w) && py >= y && py < (y + h);
  }
};

constexpr uint16_t C_BG = 0x0842;
constexpr uint16_t C_PANEL = 0x1084;
constexpr uint16_t C_PANEL_2 = 0x18E6;
constexpr uint16_t C_STROKE = 0x2927;
constexpr uint16_t C_DIVIDER = 0x2105;
constexpr uint16_t C_TEXT = 0xDEDB;
constexpr uint16_t C_TEXT_DIM = 0x8C71;
constexpr uint16_t C_ACCENT_SEQ = 0xFD20;
constexpr uint16_t C_ACCENT_STATUS = 0x2D7F;
constexpr uint16_t C_PLAY = 0x2E6A;
constexpr uint16_t C_STOP = 0xB0E4;
constexpr uint16_t C_MUTE = 0xB9A0;

static const uint16_t TRACK_COLORS[TRACK_COUNT] = {
    0xFC40, // Kick
    0xFC00, // Snare
    0x7D07, // Hat closed
    0x959F, // Hat open
    0x3D07  // Bass
};

static const char* const TRACK_NAMES[TRACK_COUNT] = {"KICK", "SNARE", "HAT C", "HAT O", "BASS"};

constexpr Rect R_HEADER = {0, 0, 320, 40};
constexpr Rect R_STATUS = {0, 40, 320, 16};
constexpr Rect R_TRACKS = {0, 56, 68, 148};
constexpr Rect R_CENTER = {68, 56, 148, 148};
constexpr Rect R_PANEL = {216, 56, 104, 148};
constexpr Rect R_TABS = {0, 204, 320, 36};

constexpr Rect BTN_PLAY = {6, 6, 52, 28};
constexpr Rect BPM_BOX = {66, 4, 92, 32};
constexpr Rect SLOT_BOX = {166, 6, 58, 28};
constexpr Rect BTN_SAVE = {232, 6, 38, 28};
constexpr Rect BTN_LOAD = {276, 6, 38, 28};

constexpr Rect TAB_SEQ = {4, 208, 102, 28};
constexpr Rect TAB_SOUND = {109, 208, 102, 28};
constexpr Rect TAB_MIX = {214, 208, 102, 28};

constexpr int kTrackButtonH = 24;
constexpr int kTrackButtonGap = 4;
constexpr int kLongPressMs = 380;

struct ParamCardRects {
  Rect outer;
  Rect minus;
  Rect value;
  Rect plus;
};

struct FaderRect {
  Rect slot;
  Rect track;
};

struct DirtyFlags {
  bool dirtyHeader = true;
  bool dirtyStatus = true;
  bool dirtyTracks = true;
  bool dirtyCenter = true;
  bool dirtyPanel = true;
  bool dirtyTabs = true;
  bool dirtyBpm = true;
  bool dirtySlot = true;
  bool dirtyTransport = true;
  bool dirtyActiveTrack = true;
  bool dirtyPattern = true;
  bool dirtyParams = true;
  bool dirtyMixer = true;

  void all() {
    dirtyHeader = dirtyStatus = dirtyTracks = dirtyCenter = dirtyPanel = dirtyTabs = true;
    dirtyBpm = dirtySlot = dirtyTransport = dirtyActiveTrack = true;
    dirtyPattern = dirtyParams = dirtyMixer = true;
  }
};

struct TouchHoldContext {
  bool pending = false;
  bool consumed = false;
  int trackIndex = -1;
};

struct UiRuntime {
  UiStateSnapshot snapshot;
  UiStateSnapshot lastSnapshot;
  uint8_t activeSlot = 0;
  uint8_t lastActiveSlot = 255;
  UiMode mode = UiMode::PATTERN_EDIT;
  UiMode lastMode = UiMode::SYSTEM;

  char status[40] = "READY";
  char lastStatus[40] = "";
  uint32_t statusUntilMs = 0;

  int activeHoldParam = -1;
  int holdDirection = 0;
  uint32_t holdNextTickMs = 0;
  int holdTicks = 0;

  DirtyFlags dirty;
  TouchHoldContext holdTouch;

  bool initialized = false;
} ui;

inline UiMode uiModeFromTab(int tab) {
  switch (tab) {
  case 0:
    return UiMode::PATTERN_EDIT;
  case 1:
    return UiMode::SOUND_EDIT;
  default:
    return UiMode::MIXER;
  }
}

inline int tabFromMode(UiMode mode) {
  if (mode == UiMode::SOUND_EDIT)
    return 1;
  if (mode == UiMode::MIXER)
    return 2;
  return 0;
}

template <typename T> static inline void setFontUiSmall(T& g) { g.setFont(&fonts::FreeSansBold12pt7b); }
template <typename T> static inline void setFontUiLarge(T& g) { g.setFont(&fonts::FreeSansBold18pt7b); }
template <typename T> static inline void setFontValueSmall(T& g) { g.setFont(&fonts::FreeMonoBold12pt7b); }
template <typename T> static inline void setFontValueLarge(T& g) { g.setFont(&fonts::FreeMonoBold18pt7b); }
template <typename T> static inline void setFontHeroNumber(T& g) { g.setFont(&fonts::FreeMonoBold24pt7b); }

static inline uint16_t blend16(uint16_t fg, uint16_t bg, uint8_t a) {
  uint8_t r = (uint8_t)(((fg >> 11) * a + (bg >> 11) * (255u - a)) / 255u);
  uint8_t g = (uint8_t)((((fg >> 5) & 63u) * a + ((bg >> 5) & 63u) * (255u - a)) / 255u);
  uint8_t b = (uint8_t)(((fg & 31u) * a + (bg & 31u) * (255u - a)) / 255u);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static void postUiAction(UiActionType type, int idx = 0, int val = 0) {
  IPCCommand cmd{};
  cmd.type = CommandType::UI_ACTION;
  cmd.voiceId = static_cast<uint8_t>(type);
  cmd.paramId = static_cast<uint8_t>(idx);
  cmd.value = static_cast<float>(val);
  CtrlMgr.sendCommand(cmd);
}

static void setStatus(const char* msg, uint32_t ms = 1200) {
  snprintf(ui.status, sizeof(ui.status), "%s", msg);
  ui.statusUntilMs = millis() + ms;
  ui.dirty.dirtyStatus = true;
}

static Rect trackRect(int index) {
  const int16_t x = 4;
  const int16_t yStart = R_TRACKS.y + 6;
  return {x, static_cast<int16_t>(yStart + index * (kTrackButtonH + kTrackButtonGap)),
          static_cast<int16_t>(R_TRACKS.w - 8), kTrackButtonH};
}

static Rect trackMuteRect(const Rect& tr) {
  return {static_cast<int16_t>(tr.x + tr.w - 14), tr.y, 14, tr.h};
}

static ParamCardRects cardRects(int row) {
  const int16_t x = R_PANEL.x + 4;
  const int16_t y = static_cast<int16_t>(R_PANEL.y + 4 + row * 36);
  const int16_t w = R_PANEL.w - 8;
  const int16_t h = 32;
  const Rect outer = {x, y, w, h};
  const Rect minus = {static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 12), 16, 16};
  const Rect plus = {static_cast<int16_t>(x + w - 20), static_cast<int16_t>(y + 12), 16, 16};
  const Rect value = {static_cast<int16_t>(minus.x + minus.w + 4), static_cast<int16_t>(y + 10),
                      static_cast<int16_t>(w - 48), 18};
  return {outer, minus, value, plus};
}

static FaderRect mixFaderRect(int index) {
  const Rect mixArea = {R_CENTER.x, R_CENTER.y, static_cast<int16_t>(R_CENTER.w + R_PANEL.w), R_CENTER.h};
  const int16_t pad = 8;
  const int16_t gap = 6;
  const int16_t faderW = static_cast<int16_t>((mixArea.w - (2 * pad) - (gap * 4)) / 5);
  const int16_t x = static_cast<int16_t>(mixArea.x + pad + index * (faderW + gap));
  const Rect slot = {x, static_cast<int16_t>(mixArea.y + 20), faderW, static_cast<int16_t>(mixArea.h - 30)};
  const Rect track = {static_cast<int16_t>(x + (faderW / 2) - 6), static_cast<int16_t>(slot.y + 4), 12,
                      static_cast<int16_t>(slot.h - 16)};
  return {slot, track};
}

static void drawPlusMinusSymbol(const Rect& r, bool plus) {
  const int16_t cx = r.x + r.w / 2;
  const int16_t cy = r.y + r.h / 2;
  tft.drawFastHLine(cx - 4, cy, 9, C_TEXT);
  if (plus) {
    tft.drawFastVLine(cx, cy - 4, 9, C_TEXT);
  }
}

static void drawHeaderButton(const Rect& r, const char* label, bool active, uint16_t accent) {
  const uint16_t fill = active ? accent : C_PANEL_2;
  const uint16_t text = active ? C_BG : C_TEXT;
  tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, fill);
  tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, C_STROKE);
  setFontUiSmall(tft);
  tft.setTextColor(text, fill);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
}

static void drawHeader() {
  tft.fillRect(R_HEADER.x, R_HEADER.y, R_HEADER.w, R_HEADER.h, C_BG);

  const bool playing = ui.snapshot.isPlaying;
  drawHeaderButton(BTN_PLAY, playing ? "STOP" : "PLAY", playing, playing ? C_STOP : C_PLAY);

  tft.fillRoundRect(BPM_BOX.x, BPM_BOX.y, BPM_BOX.w, BPM_BOX.h, 4, C_PANEL);
  tft.drawRoundRect(BPM_BOX.x, BPM_BOX.y, BPM_BOX.w, BPM_BOX.h, 4, C_STROKE);
  setFontUiSmall(tft);
  tft.setTextColor(C_TEXT_DIM, C_PANEL);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("BPM", BPM_BOX.x + 6, BPM_BOX.y + 4);
  setFontHeroNumber(tft);
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.setTextDatum(TR_DATUM);
  char bpmText[8];
  snprintf(bpmText, sizeof(bpmText), "%3d", ui.snapshot.bpm);
  tft.drawString(bpmText, BPM_BOX.x + BPM_BOX.w - 6, BPM_BOX.y + BPM_BOX.h - 4);

  tft.fillRoundRect(SLOT_BOX.x, SLOT_BOX.y, SLOT_BOX.w, SLOT_BOX.h, 4, C_PANEL_2);
  tft.drawRoundRect(SLOT_BOX.x, SLOT_BOX.y, SLOT_BOX.w, SLOT_BOX.h, 4, C_STROKE);
  setFontUiSmall(tft);
  tft.setTextColor(C_TEXT_DIM, C_PANEL_2);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("SLOT", SLOT_BOX.x + 4, SLOT_BOX.y + 4);
  setFontValueLarge(tft);
  tft.setTextColor(C_TEXT, C_PANEL_2);
  tft.setTextDatum(TR_DATUM);
  char slotText[8];
  snprintf(slotText, sizeof(slotText), "%02d", ui.activeSlot + 1);
  tft.drawString(slotText, SLOT_BOX.x + SLOT_BOX.w - 4, SLOT_BOX.y + SLOT_BOX.h - 5);

  drawHeaderButton(BTN_SAVE, "SAVE", false, C_ACCENT_SEQ);
  drawHeaderButton(BTN_LOAD, "LOAD", false, C_ACCENT_SEQ);

  tft.drawFastHLine(0, R_HEADER.y + R_HEADER.h - 1, R_HEADER.w, C_DIVIDER);
}

static void drawStatusStrip() {
  tft.fillRect(R_STATUS.x, R_STATUS.y, R_STATUS.w, R_STATUS.h, C_PANEL);
  const bool statusActive = ui.statusUntilMs > millis();
  setFontUiSmall(tft);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C_ACCENT_STATUS, C_PANEL);
  tft.drawString("STATUS", 6, R_STATUS.y + R_STATUS.h / 2);

  setFontValueSmall(tft);
  tft.setTextColor(statusActive ? C_TEXT : C_TEXT_DIM, C_PANEL);
  tft.drawString(statusActive ? ui.status : (ui.snapshot.isPlaying ? "PLAYING" : "READY"), 70,
                 R_STATUS.y + R_STATUS.h / 2);
}

static void drawTrackButton(int i) {
  const Rect tr = trackRect(i);
  const Rect mute = trackMuteRect(tr);
  const bool active = (ui.snapshot.activeTrack == i);
  const bool muted = ui.snapshot.trackMutes[i];
  const uint16_t trackColor = TRACK_COLORS[i];

  uint16_t fill = C_PANEL;
  uint16_t text = C_TEXT;
  if (active) {
    fill = blend16(trackColor, C_BG, 85);
    text = C_BG;
  } else if (muted) {
    fill = C_PANEL_2;
    text = C_TEXT_DIM;
  }

  tft.fillRoundRect(tr.x, tr.y, tr.w, tr.h, 4, fill);
  tft.drawRoundRect(tr.x, tr.y, tr.w, tr.h, 4, C_STROKE);

  tft.fillRect(mute.x, mute.y, mute.w, mute.h, muted ? C_MUTE : C_PANEL_2);
  tft.drawFastVLine(mute.x, mute.y, mute.h, C_STROKE);

  setFontUiLarge(tft);
  tft.setTextColor(text, fill);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(TRACK_NAMES[i], tr.x + 4, tr.y + tr.h / 2 + 1);

  setFontValueSmall(tft);
  tft.setTextColor(muted ? C_BG : C_TEXT_DIM, muted ? C_MUTE : C_PANEL_2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("M", mute.x + mute.w / 2, mute.y + mute.h / 2);
}

static void drawTracks() {
  tft.fillRect(R_TRACKS.x, R_TRACKS.y, R_TRACKS.w, R_TRACKS.h, C_BG);
  for (int i = 0; i < TRACK_COUNT; ++i) {
    drawTrackButton(i);
  }
}

static void drawSeqCenter() {
  tft.fillRect(R_CENTER.x, R_CENTER.y, R_CENTER.w, R_CENTER.h, C_BG);
  const int16_t cx = R_CENTER.x + R_CENTER.w / 2;
  const int16_t cy = R_CENTER.y + R_CENTER.h / 2;

  for (int t = 0; t < TRACK_COUNT; ++t) {
    const int16_t radius = 60 - t * 10;
    const uint16_t ringColor = (t == ui.snapshot.activeTrack) ? TRACK_COLORS[t] : blend16(TRACK_COLORS[t], C_BG, 40);
    tft.drawCircle(cx, cy, radius, ringColor);

    const int len = max(1, ui.snapshot.patternLens[t]);
    for (int s = 0; s < len; ++s) {
      const float a = ((2.0f * M_PI * s) / len) - 1.5707963f;
      const int16_t px = static_cast<int16_t>(cx + cosf(a) * radius);
      const int16_t py = static_cast<int16_t>(cy + sinf(a) * radius);
      const bool hit = ui.snapshot.patterns[t][s] > 0;
      const bool playhead = ui.snapshot.isPlaying && ((ui.snapshot.currentStep % len) == s);

      if (hit && playhead) {
        tft.fillCircle(px, py, 3, C_TEXT);
      } else if (hit) {
        tft.fillCircle(px, py, 2, ringColor);
      } else if (playhead && t == ui.snapshot.activeTrack) {
        tft.drawCircle(px, py, 2, C_ACCENT_SEQ);
      }
    }
  }

  setFontHeroNumber(tft);
  tft.setTextColor(C_TEXT, C_BG);
  tft.setTextDatum(MC_DATUM);
  char stepTxt[16];
  snprintf(stepTxt, sizeof(stepTxt), "%02d", (ui.snapshot.currentStep % 16) + 1);
  tft.drawString(stepTxt, cx, cy + 2);
  setFontUiSmall(tft);
  tft.setTextColor(C_TEXT_DIM, C_BG);
  tft.drawString("STEP", cx, cy - 26);
}

static void drawSoundCenter() {
  tft.fillRect(R_CENTER.x, R_CENTER.y, R_CENTER.w, R_CENTER.h, C_BG);
  setFontUiSmall(tft);
  tft.setTextColor(C_TEXT_DIM, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("SOUND PREVIEW", R_CENTER.x + 6, R_CENTER.y + 12);

  const int barCount = 8;
  const int barW = 14;
  const int gap = 3;
  const int x0 = R_CENTER.x + 10;
  const int baseY = R_CENTER.y + R_CENTER.h - 20;
  const uint16_t c = TRACK_COLORS[ui.snapshot.activeTrack];

  for (int i = 0; i < barCount; ++i) {
    const uint8_t patt = ui.snapshot.patterns[ui.snapshot.activeTrack][i] > 0 ? 1 : 0;
    const int h = patt ? 56 - (i % 3) * 10 : 24 - (i % 2) * 6;
    const int x = x0 + i * (barW + gap);
    tft.fillRoundRect(x, baseY - h, barW, h, 3, patt ? c : C_PANEL_2);
    tft.drawRoundRect(x, baseY - h, barW, h, 3, C_STROKE);
  }

  setFontValueLarge(tft);
  tft.setTextColor(C_TEXT, C_BG);
  tft.setTextDatum(BR_DATUM);
  char tr[8];
  snprintf(tr, sizeof(tr), "T%d", ui.snapshot.activeTrack + 1);
  tft.drawString(tr, R_CENTER.x + R_CENTER.w - 6, R_CENTER.y + R_CENTER.h - 6);
}

static void drawMixCenter() {
  const Rect mixArea = {R_CENTER.x, R_CENTER.y, static_cast<int16_t>(R_CENTER.w + R_PANEL.w), R_CENTER.h};
  tft.fillRect(mixArea.x, mixArea.y, mixArea.w, mixArea.h, C_BG);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    FaderRect fr = mixFaderRect(i);
    tft.fillRoundRect(fr.slot.x, fr.slot.y, fr.slot.w, fr.slot.h, 4, C_PANEL);
    tft.drawRoundRect(fr.slot.x, fr.slot.y, fr.slot.w, fr.slot.h, 4, C_STROKE);

    const float gain = constrain(ui.snapshot.voiceGain[i], 0.0f, 1.0f);
    const int16_t fillH = static_cast<int16_t>(gain * (fr.track.h - 6));
    const int16_t fillY = fr.track.y + fr.track.h - fillH;

    tft.fillRect(fr.track.x, fr.track.y, fr.track.w, fr.track.h, C_PANEL_2);
    tft.fillRect(fr.track.x, fillY, fr.track.w, fillH, ui.snapshot.trackMutes[i] ? C_MUTE : TRACK_COLORS[i]);
    tft.drawRect(fr.track.x, fr.track.y, fr.track.w, fr.track.h, C_STROKE);

    const int16_t knobY = constrain(fillY - 3, fr.track.y, fr.track.y + fr.track.h - 6);
    tft.fillRoundRect(fr.slot.x + 2, knobY, fr.slot.w - 4, 6, 2, C_TEXT);

    setFontUiSmall(tft);
    tft.setTextColor(C_TEXT, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(TRACK_NAMES[i], fr.slot.x + fr.slot.w / 2, fr.slot.y + fr.slot.h + 10);
  }
}

static int getParamDisplayValue(int paramIndex) {
  const int track = ui.snapshot.activeTrack;
  const bool isBass = track == VOICE_BASS;
  const VoiceParams& params = ui.snapshot.voiceParams[track];

  if (ui.mode == UiMode::SOUND_EDIT) {
    switch (paramIndex) {
    case 0:
      return static_cast<int>(params.pitch * 100.0f);
    case 1:
      return static_cast<int>(params.decay * 100.0f);
    case 2:
      return static_cast<int>(params.timbre * 100.0f);
    default:
      return static_cast<int>((isBass ? params.drive : ui.snapshot.voiceGain[track]) * 100.0f);
    }
  }

  if (isBass) {
    switch (paramIndex) {
    case 0:
      return static_cast<int>(ui.snapshot.bassParams.density * 100.0f);
    case 1:
      return ui.snapshot.bassParams.range;
    case 2:
      return static_cast<int>(ui.snapshot.bassParams.scaleType);
    default:
      return ui.snapshot.bassParams.rootNote;
    }
  }

  switch (paramIndex) {
  case 0:
    return ui.snapshot.trackSteps[track];
  case 1:
    return ui.snapshot.trackHits[track];
  case 2:
    return ui.snapshot.trackRotations[track];
  default:
    return static_cast<int>(ui.snapshot.voiceGain[track] * 100.0f);
  }
}

static void getParamLabel(int row, char* out, size_t n) {
  if (ui.mode == UiMode::SOUND_EDIT) {
    static const char* kSoundLabels[4] = {"PITCH", "DECAY", "TIMBRE", "LEVEL"};
    snprintf(out, n, "%s", kSoundLabels[row]);
    return;
  }

  if (ui.snapshot.activeTrack == VOICE_BASS) {
    static const char* kBassLabels[4] = {"DENS", "RANGE", "SCALE", "ROOT"};
    snprintf(out, n, "%s", kBassLabels[row]);
    return;
  }

  static const char* kSeqLabels[4] = {"STEPS", "HITS", "ROT", "VOL"};
  snprintf(out, n, "%s", kSeqLabels[row]);
}

static void drawParamPanel() {
  tft.fillRect(R_PANEL.x, R_PANEL.y, R_PANEL.w, R_PANEL.h, C_BG);
  if (ui.mode == UiMode::MIXER) {
    return;
  }

  for (int row = 0; row < 4; ++row) {
    ParamCardRects rc = cardRects(row);
    const bool focused = (row == 0);
    tft.fillRoundRect(rc.outer.x, rc.outer.y, rc.outer.w, rc.outer.h, 4, C_PANEL);
    tft.drawRoundRect(rc.outer.x, rc.outer.y, rc.outer.w, rc.outer.h, 4,
                      focused ? C_ACCENT_SEQ : C_STROKE);

    char lbl[10];
    getParamLabel(row, lbl, sizeof(lbl));
    setFontUiSmall(tft);
    tft.setTextColor(focused ? C_TEXT : C_TEXT_DIM, C_PANEL);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(lbl, rc.outer.x + 4, rc.outer.y + 3);

    tft.fillRoundRect(rc.minus.x, rc.minus.y, rc.minus.w, rc.minus.h, 3, C_PANEL_2);
    tft.fillRoundRect(rc.plus.x, rc.plus.y, rc.plus.w, rc.plus.h, 3, C_PANEL_2);
    tft.drawRoundRect(rc.minus.x, rc.minus.y, rc.minus.w, rc.minus.h, 3, C_STROKE);
    tft.drawRoundRect(rc.plus.x, rc.plus.y, rc.plus.w, rc.plus.h, 3, C_STROKE);

    drawPlusMinusSymbol(rc.minus, false);
    drawPlusMinusSymbol(rc.plus, true);

    tft.fillRoundRect(rc.value.x, rc.value.y, rc.value.w, rc.value.h, 3, C_PANEL_2);
    tft.drawRoundRect(rc.value.x, rc.value.y, rc.value.w, rc.value.h, 3, C_STROKE);

    setFontValueLarge(tft);
    tft.setTextColor(C_TEXT, C_PANEL_2);
    tft.setTextDatum(MC_DATUM);
    char val[12];
    snprintf(val, sizeof(val), "%d", getParamDisplayValue(row));
    tft.drawString(val, rc.value.x + rc.value.w / 2, rc.value.y + rc.value.h / 2 + 1);
  }
}

static void drawTabs() {
  tft.fillRect(R_TABS.x, R_TABS.y, R_TABS.w, R_TABS.h, C_BG);
  tft.drawFastHLine(R_TABS.x, R_TABS.y, R_TABS.w, C_DIVIDER);

  const int activeTab = tabFromMode(ui.mode);
  const Rect tabs[3] = {TAB_SEQ, TAB_SOUND, TAB_MIX};
  const char* names[3] = {"SEQ", "SOUND", "MIX"};

  for (int i = 0; i < 3; ++i) {
    const bool active = (i == activeTab);
    tft.fillRoundRect(tabs[i].x, tabs[i].y, tabs[i].w, tabs[i].h, 4, active ? C_ACCENT_SEQ : C_PANEL_2);
    tft.drawRoundRect(tabs[i].x, tabs[i].y, tabs[i].w, tabs[i].h, 4, C_STROKE);
    setFontUiLarge(tft);
    tft.setTextColor(active ? C_BG : C_TEXT, active ? C_ACCENT_SEQ : C_PANEL_2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(names[i], tabs[i].x + tabs[i].w / 2, tabs[i].y + tabs[i].h / 2 + 1);
  }
}

static void drawCenterByMode() {
  if (ui.mode == UiMode::MIXER) {
    drawMixCenter();
    return;
  }
  if (ui.mode == UiMode::SOUND_EDIT) {
    drawSoundCenter();
    return;
  }
  drawSeqCenter();
}

static void fireParamAction(int paramIndex, int amount) {
  const int track = ui.snapshot.activeTrack;
  const bool isBass = track == VOICE_BASS;
  const int newValue = getParamDisplayValue(paramIndex) + amount;

  if (ui.mode == UiMode::SOUND_EDIT) {
    if (isBass) {
      postUiAction(UiActionType::SET_BASS_PARAM, paramIndex, newValue);
    } else {
      postUiAction(UiActionType::SET_SOUND_PARAM, paramIndex, newValue);
    }
    return;
  }

  if (isBass) {
    postUiAction(UiActionType::SET_BASS_PARAM, paramIndex, newValue);
    return;
  }

  switch (paramIndex) {
  case 0:
    postUiAction(UiActionType::SET_STEPS, track, newValue);
    break;
  case 1:
    postUiAction(UiActionType::SET_HITS, track, newValue);
    break;
  case 2:
    postUiAction(UiActionType::SET_ROTATION, track, newValue);
    break;
  default:
    postUiAction(UiActionType::SET_SOUND_PARAM, 3, newValue);
    break;
  }
}

static uint32_t holdInterval(int ticks) {
  if (ticks < 6)
    return 260;
  if (ticks < 14)
    return 100;
  return 50;
}

static void startRepeat(int paramIndex, int direction) {
  ui.activeHoldParam = paramIndex;
  ui.holdDirection = direction;
  ui.holdTicks = 0;
  ui.holdNextTickMs = millis() + 420;
}

static void stopRepeat() {
  ui.activeHoldParam = -1;
  ui.holdDirection = 0;
  ui.holdTicks = 0;
}

static void tickRepeat() {
  if (ui.activeHoldParam < 0 || ui.holdDirection == 0) {
    return;
  }
  const uint32_t now = millis();
  if (now < ui.holdNextTickMs) {
    return;
  }

  ++ui.holdTicks;
  int amount = ui.holdDirection;
  if (ui.holdTicks > 14) {
    amount *= 4;
  } else if (ui.holdTicks > 6) {
    amount *= 2;
  }

  if (ui.activeHoldParam == 10) {
    postUiAction(UiActionType::NUDGE_BPM, 0, amount);
  } else {
    fireParamAction(ui.activeHoldParam, amount);
  }
  ui.holdNextTickMs = now + holdInterval(ui.holdTicks);
}

static void applyTabMode(int tabIndex) {
  UiMode newMode = uiModeFromTab(tabIndex);
  postUiAction(UiActionType::CHANGE_MODE, 0, static_cast<int>(newMode));
  ui.mode = newMode;
  ui.dirty.dirtyCenter = true;
  ui.dirty.dirtyTabs = true;
  ui.dirty.dirtyPanel = true;
}

static void handleLongPress(const TouchPoint& tp) {
  if (!tp.pressed || ui.holdTouch.consumed || !ui.holdTouch.pending) {
    return;
  }
  if ((millis() - tp.pressedAtMs) < kLongPressMs) {
    return;
  }
  if (ui.holdTouch.trackIndex >= 0 && ui.holdTouch.trackIndex < TRACK_COUNT) {
    postUiAction(UiActionType::TOGGLE_MUTE, 0, ui.holdTouch.trackIndex);
    setStatus("TRACK MUTE");
    ui.holdTouch.consumed = true;
  }
}

static bool handleHeaderTouch(int x, int y) {
  if (!R_HEADER.contains(x, y)) {
    return false;
  }

  if (BTN_PLAY.contains(x, y)) {
    postUiAction(UiActionType::TOGGLE_PLAY);
    return true;
  }

  if (BPM_BOX.contains(x, y)) {
    const int rel = x - BPM_BOX.x;
    if (rel < BPM_BOX.w / 2) {
      postUiAction(UiActionType::NUDGE_BPM, 0, -1);
      startRepeat(10, -1);
    } else {
      postUiAction(UiActionType::NUDGE_BPM, 0, +1);
      startRepeat(10, +1);
    }
    return true;
  }

  if (SLOT_BOX.contains(x, y)) {
    if (x < (SLOT_BOX.x + SLOT_BOX.w / 2)) {
      ui.activeSlot = (ui.activeSlot == 0) ? (CYDConfig::PatternSlots - 1) : (ui.activeSlot - 1);
    } else {
      ui.activeSlot = (ui.activeSlot + 1) % CYDConfig::PatternSlots;
    }
    ui.dirty.dirtySlot = true;
    ui.dirty.dirtyHeader = true;
    return true;
  }

  if (BTN_SAVE.contains(x, y)) {
    setStatus(PatternStore.saveSlot(ui.activeSlot) ? "SAVE OK" : "SAVE FAIL");
    return true;
  }

  if (BTN_LOAD.contains(x, y)) {
    setStatus(PatternStore.loadSlot(ui.activeSlot) ? "LOAD OK" : "LOAD FAIL");
    return true;
  }

  return true;
}

static bool handleTabTouch(int x, int y) {
  if (!R_TABS.contains(x, y)) {
    return false;
  }
  if (TAB_SEQ.contains(x, y)) {
    applyTabMode(0);
  } else if (TAB_SOUND.contains(x, y)) {
    applyTabMode(1);
  } else if (TAB_MIX.contains(x, y)) {
    applyTabMode(2);
  }
  return true;
}

static bool handleTrackTouch(int x, int y) {
  if (!R_TRACKS.contains(x, y)) {
    return false;
  }

  for (int i = 0; i < TRACK_COUNT; ++i) {
    const Rect tr = trackRect(i);
    const Rect muteR = trackMuteRect(tr);
    if (!tr.contains(x, y)) {
      continue;
    }

    if (muteR.contains(x, y)) {
      postUiAction(UiActionType::TOGGLE_MUTE, 0, i);
      return true;
    }

    postUiAction(UiActionType::SELECT_TRACK, 0, i);
    ui.holdTouch.pending = true;
    ui.holdTouch.consumed = false;
    ui.holdTouch.trackIndex = i;
    return true;
  }
  return true;
}

static bool handleParamTouch(int x, int y) {
  if (ui.mode == UiMode::MIXER || !R_PANEL.contains(x, y)) {
    return false;
  }

  for (int row = 0; row < 4; ++row) {
    ParamCardRects rc = cardRects(row);
    if (!rc.outer.contains(x, y)) {
      continue;
    }
    if (rc.minus.contains(x, y)) {
      fireParamAction(row, -1);
      startRepeat(row, -1);
      return true;
    }
    if (rc.plus.contains(x, y)) {
      fireParamAction(row, +1);
      startRepeat(row, +1);
      return true;
    }
    return true;
  }
  return true;
}

static bool handleMixTouch(int x, int y) {
  if (ui.mode != UiMode::MIXER) {
    return false;
  }
  const Rect mixArea = {R_CENTER.x, R_CENTER.y, static_cast<int16_t>(R_CENTER.w + R_PANEL.w), R_CENTER.h};
  if (!mixArea.contains(x, y)) {
    return false;
  }

  for (int i = 0; i < TRACK_COUNT; ++i) {
    FaderRect fr = mixFaderRect(i);
    if (!fr.slot.contains(x, y)) {
      continue;
    }
    float norm = 1.0f - static_cast<float>(y - fr.track.y) / static_cast<float>(fr.track.h);
    norm = constrain(norm, 0.0f, 1.0f);
    postUiAction(UiActionType::SET_VOICE_GAIN, i, static_cast<int>(norm * 100.0f));
    return true;
  }

  return true;
}

static void handleTouch(const TouchPoint& tp) {
  if (tp.justReleased) {
    stopRepeat();
    ui.holdTouch = TouchHoldContext{};
    return;
  }

  handleLongPress(tp);

  if (!tp.justPressed) {
    return;
  }

  stopRepeat();
  ui.holdTouch = TouchHoldContext{};

  const int tx = tp.x;
  const int ty = tp.y;
  if (handleHeaderTouch(tx, ty))
    return;
  if (handleTabTouch(tx, ty))
    return;
  if (handleTrackTouch(tx, ty))
    return;
  if (handleParamTouch(tx, ty))
    return;
  (void)handleMixTouch(tx, ty);
}

static void computeDirtyFlags() {
  if (!ui.initialized) {
    ui.dirty.all();
    return;
  }

  if (ui.snapshot.bpm != ui.lastSnapshot.bpm) {
    ui.dirty.dirtyBpm = true;
    ui.dirty.dirtyHeader = true;
  }

  if (ui.snapshot.isPlaying != ui.lastSnapshot.isPlaying) {
    ui.dirty.dirtyTransport = true;
    ui.dirty.dirtyHeader = true;
    ui.dirty.dirtyStatus = true;
  }

  if (ui.activeSlot != ui.lastActiveSlot) {
    ui.dirty.dirtySlot = true;
    ui.dirty.dirtyHeader = true;
  }

  if (strcmp(ui.status, ui.lastStatus) != 0) {
    ui.dirty.dirtyStatus = true;
  }

  if (ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack ||
      memcmp(ui.snapshot.trackMutes, ui.lastSnapshot.trackMutes, sizeof(ui.snapshot.trackMutes)) != 0) {
    ui.dirty.dirtyActiveTrack = true;
    ui.dirty.dirtyTracks = true;
    ui.dirty.dirtyCenter = true;
    ui.dirty.dirtyPanel = true;
  }

  if (ui.mode != ui.lastMode) {
    ui.dirty.dirtyCenter = true;
    ui.dirty.dirtyTabs = true;
    ui.dirty.dirtyPanel = true;
  }

  if (memcmp(ui.snapshot.patterns, ui.lastSnapshot.patterns, sizeof(ui.snapshot.patterns)) != 0 ||
      memcmp(ui.snapshot.patternLens, ui.lastSnapshot.patternLens, sizeof(ui.snapshot.patternLens)) != 0 ||
      ui.snapshot.currentStep != ui.lastSnapshot.currentStep) {
    ui.dirty.dirtyPattern = true;
    ui.dirty.dirtyCenter = true;
  }

  if (memcmp(ui.snapshot.trackSteps, ui.lastSnapshot.trackSteps, sizeof(ui.snapshot.trackSteps)) != 0 ||
      memcmp(ui.snapshot.trackHits, ui.lastSnapshot.trackHits, sizeof(ui.snapshot.trackHits)) != 0 ||
      memcmp(ui.snapshot.trackRotations, ui.lastSnapshot.trackRotations, sizeof(ui.snapshot.trackRotations)) != 0 ||
      memcmp(ui.snapshot.voiceParams, ui.lastSnapshot.voiceParams, sizeof(ui.snapshot.voiceParams)) != 0 ||
      memcmp(ui.snapshot.voiceGain, ui.lastSnapshot.voiceGain, sizeof(ui.snapshot.voiceGain)) != 0 ||
      memcmp(&ui.snapshot.bassParams, &ui.lastSnapshot.bassParams, sizeof(BassGrooveParams)) != 0) {
    ui.dirty.dirtyParams = true;
    ui.dirty.dirtyPanel = true;
    ui.dirty.dirtyMixer = true;
    if (ui.mode == UiMode::MIXER) {
      ui.dirty.dirtyCenter = true;
    }
  }
}

static void renderDirtyRegions() {
  if (ui.dirty.dirtyHeader || ui.dirty.dirtyBpm || ui.dirty.dirtySlot || ui.dirty.dirtyTransport) {
    drawHeader();
  }

  if (ui.dirty.dirtyStatus) {
    drawStatusStrip();
  }

  if (ui.dirty.dirtyTracks || ui.dirty.dirtyActiveTrack) {
    drawTracks();
  }

  if (ui.dirty.dirtyCenter || ui.dirty.dirtyPattern || ui.dirty.dirtyMixer) {
    drawCenterByMode();
  }

  if (ui.dirty.dirtyPanel || ui.dirty.dirtyParams) {
    drawParamPanel();
  }

  if (ui.dirty.dirtyTabs) {
    drawTabs();
  }

  ui.dirty = DirtyFlags{};
}

} // namespace

void UiStateSnapshot::capture() {
  bpm = engine.bpm.load(std::memory_order_relaxed);
  isPlaying = engine.isPlaying.load(std::memory_order_relaxed);
  currentStep = engine.currentStep.load(std::memory_order_relaxed);
  activeTrack = engine.uiActiveTrack.load(std::memory_order_relaxed);
  mode = engine.uiMode.load(std::memory_order_relaxed);
  bassParams = engine.bassGroove.getParams();
  masterVolume = voiceManager.getMasterVolume();

  for (int i = 0; i < TRACK_COUNT; ++i) {
    trackMutes[i] = engine.trackMutes[i].load(std::memory_order_relaxed);
    trackSteps[i] = engine.tracks[i].steps;
    trackHits[i] = engine.tracks[i].hits;
    trackRotations[i] = engine.tracks[i].rotationOffset;
    voiceParams[i] = voiceManager.getParams(static_cast<VoiceID>(i));
    voiceGain[i] = voiceManager.getVoiceGain(static_cast<VoiceID>(i));
  }

  if (engine.lockPattern()) {
    for (int i = 0; i < TRACK_COUNT; ++i) {
      patternLens[i] = min(64, static_cast<int>(engine.tracks[i].patternLen));
      memcpy(patterns[i], engine.tracks[i].pattern, patternLens[i]);
    }
    engine.unlockPattern();
  }
}

void displayTask(void* parameter) {
  (void)parameter;
  while (!engine.engineReady.load()) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  pinMode(CYDConfig::TftBacklight, OUTPUT);
  digitalWrite(CYDConfig::TftBacklight, HIGH);

  tft.init();
  tft.setRotation(CYDConfig::ScreenRotation);
  InputMgr.begin();

  tft.fillScreen(C_BG);
  ui.dirty.all();

  for (;;) {
    const uint32_t now = millis();

    ui.snapshot.capture();
    ui.mode = ui.snapshot.mode;

    if (ui.statusUntilMs && now >= ui.statusUntilMs) {
      ui.statusUntilMs = 0;
      ui.dirty.dirtyStatus = true;
    }

    InputMgr.update();
    handleTouch(InputMgr.state());
    tickRepeat();

    computeDirtyFlags();

    tft.startWrite();
    renderDirtyRegions();
    tft.endWrite();

    ui.lastSnapshot = ui.snapshot;
    ui.lastMode = ui.mode;
    ui.lastActiveSlot = ui.activeSlot;
    snprintf(ui.lastStatus, sizeof(ui.lastStatus), "%s", ui.status);
    ui.initialized = true;

    vTaskDelay(pdMS_TO_TICKS(16));
  }
}
