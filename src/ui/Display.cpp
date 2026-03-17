#include "Display.h"

#include "../control/ControlManager.h"
#include "../storage/PatternStorage.h"
#include "LGFX_CYD.h"
#include <SPI.h>
#include "UiAction.h"
#include <math.h>

LGFX_CYD tft;

namespace {
// ── Color Palette: Visual Hierarchy ──────────────────────────────────────
// Background / structural
constexpr uint16_t ColorBg     = 0x0863;  // Deep navy-charcoal
constexpr uint16_t ColorPanel  = 0x1947;  // Dark slate-blue
constexpr uint16_t ColorPanel2 = 0x2104;  // Darker panel (inactive buttons)
constexpr uint16_t ColorDim    = 0x4228;  // Outline / subdued
// State colors — user visual hierarchy
constexpr uint16_t ColorText   = 0xBDF8;  // Normal state       #B8BDC7
constexpr uint16_t ColorAccent = 0xFEC9;  // Important/active   #FFD84D
constexpr uint16_t ColorSelect = 0x4F1F;  // Selected mode      #4DE3FF
constexpr uint16_t ColorExpr   = 0xFA74;  // Expressive         #FF4DA6
constexpr uint16_t ColorPlay   = 0x7FEE;  // Positive/running   #7CFF72
// Instrument colors
constexpr uint16_t ColorKick   = 0xCAA9;  // Terracotta
constexpr uint16_t ColorSnare  = 0xD568;  // Warm gold
constexpr uint16_t ColorHatC   = 0x4F1F;  // Steel teal = Selected #4DE3FF
constexpr uint16_t ColorHatO   = 0xFA74;  // Mauve = Expressive #FF4DA6
constexpr uint16_t ColorBass   = 0x5D6C;  // Muted sage
constexpr uint16_t TrackColors[TRACK_COUNT] = {ColorKick, ColorSnare, ColorHatC,
                                               ColorHatO, ColorBass};
constexpr float Pi = 3.14159265f;

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;

  bool contains(int16_t px, int16_t py) const {
    return px >= x && px < (x + w) && py >= y && py < (y + h);
  }
};

struct SliderWidget {
  Rect bounds;
  const char *label;
};

struct UiRuntime {
  UiStateSnapshot snapshot;
  UiStateSnapshot lastSnapshot;
  uint8_t activeSlot = 0;
  uint8_t lastActiveSlot = 255;
  
  // Hold-to-increment tracking
  int activeHoldAction = -1; 
  int activeHoldParam = -1;
  uint32_t holdNextTickMs = 0;
  uint32_t holdTickIntervalMs = 0;
  int holdTickCount = 0;

  String status = "CYD ready";
  String lastStatus = "";
  uint32_t statusUntilMs = 0;
  bool forceRedraw = true;
} ui;

// Play button left-aligned with track selector column (x=4)
Rect playButton{4, 8, 62, 30};
// Slot/preset control moved left (where BPM was), BPM moved right for accessibility
Rect slotMinus{80, 8, 20, 30};
Rect slotValue{104, 8, 28, 30};
Rect slotPlus{136, 8, 20, 30};
Rect saveButton{160, 8, 24, 14};
Rect loadButton{160, 24, 24, 14};
Rect bpmMinus{196, 8, 28, 30};
Rect bpmValue{228, 8, 52, 30};
Rect bpmPlus{284, 8, 28, 30};

// Left Margin Track Selector — h=34, gap=4, B bottom=234 aligns with mute bottom
// y: 48, 86, 124, 162, 200  (step=38=34+4)  last bottom=200+34=234 ✓
Rect trackButtons[TRACK_COUNT] = {
    {4, 48, 28, 34}, {4, 86, 28, 34}, {4, 124, 28, 34},
    {4, 162, 28, 34}, {4, 200, 28, 34}};

// Center Euclid — ring shifted down slightly
Rect ringArea{36, 52, 140, 140};
// Mute/Voice/Mix row — h=26 to match Vol param buttons, bottom=234
// x=36..192 (156px / 3 = 52 each). Gap left (right_B→left_MUTE) = 36-32 = 4px
// Gap right (right_MIX→left_Vol-) = 196-192 = 4px ← symmetric
Rect muteToggle {36,  208, 52, 26};
Rect voiceToggle{88,  208, 52, 26};
Rect mixToggle  {140, 208, 52, 26};

// Right Panel Parameter Components (3 main rows now)
struct ParamHBox {
  Rect minusBtn;
  Rect valueBox;
  Rect plusBtn;
  Rect touchArea; // The entire row area
};

// Param rows: redistribute so Volume bottom=234 aligns with MIX bottom (206+28)
// Gaps: 12,11,11 between blocks of 38 (label12+btn26)
// Row0: label@48 btn@60 bot@86 | Row1: label@98 btn@110 bot@136
// Row2: label@147 btn@159 bot@185 | Row3: label@196 btn@208 bot@234
ParamHBox paramBoxes[4] = {
  {{196, 60,  28, 26}, {228, 60,  52, 26}, {284, 60,  28, 26}, {180, 44, 132, 42}},
  {{196, 110, 28, 26}, {228, 110, 52, 26}, {284, 110, 28, 26}, {180, 94, 132, 42}},
  {{196, 159, 28, 26}, {228, 159, 52, 26}, {284, 159, 28, 26}, {180, 143, 132, 42}},
  {{196, 208, 28, 26}, {228, 208, 52, 26}, {284, 208, 28, 26}, {180, 192, 132, 42}},
};

Rect mixerSliders[TRACK_COUNT] = {
  {201, 54, 12, 126},
  {224, 54, 12, 126},
  {247, 54, 12, 126},
  {270, 54, 12, 126},
  {293, 54, 12, 126}
};

const char *trackLabels[TRACK_COUNT] = {"K", "S", "C", "O", "B"};

void postUiAction(UiActionType type, int index = 0, int value = 0) {
  IPCCommand cmd{};
  cmd.type = CommandType::UI_ACTION;
  cmd.voiceId = static_cast<uint8_t>(type);
  cmd.paramId = static_cast<uint8_t>(index);
  cmd.value = static_cast<float>(value);
  CtrlMgr.sendCommand(cmd);
}

void setStatus(const String &message, uint32_t durationMs = 1200) {
  ui.status = message;
  ui.statusUntilMs = millis() + durationMs;
}

float clamp01(float v) {
  if (v < 0.0f) {
    return 0.0f;
  }
  if (v > 1.0f) {
    return 1.0f;
  }
  return v;
}

float sliderRatio(const Rect &rect, uint16_t x) {
  return clamp01(static_cast<float>(x - rect.x) / static_cast<float>(rect.w));
}

uint16_t currentModeColor(UiMode mode, UiMode target) {
  return mode == target ? ColorAccent : ColorPanel2;
}

void drawRectButton(const Rect &rect, const String &label, uint16_t fill,
                    uint16_t outline = ColorDim, uint16_t text = ColorText,
                    uint8_t font = 0) {
  constexpr int16_t r = 3; // subtle corner radius
  tft.fillRoundRect(rect.x, rect.y, rect.w, rect.h, r, fill);
  tft.drawRoundRect(rect.x, rect.y, rect.w, rect.h, r, outline);
  tft.setTextColor(text, fill);
  tft.setTextDatum(MC_DATUM);
  if (font == 0) {
    // GFX free font: render 1px higher to avoid clipping in small buttons
    tft.drawString(label, rect.x + rect.w / 2, rect.y + rect.h / 2 - 1);
  } else {
    tft.drawString(label, rect.x + rect.w / 2, rect.y + rect.h / 2, font);
  }
}

void drawTransport() {
  tft.fillRect(0, 0, CYDConfig::ScreenWidth, 42, ColorBg);

  // Play button with shape instead of text
  tft.fillRect(playButton.x, playButton.y, playButton.w, playButton.h,
               ui.snapshot.isPlaying ? ColorAccent : ColorPlay);
  tft.drawRect(playButton.x, playButton.y, playButton.w, playButton.h,
               ColorDim);
  
  if (ui.snapshot.isPlaying) {
    // Stop Square
    tft.fillRect(playButton.x + 23, playButton.y + 7, 16, 16, ColorBg);
  } else {
    // Play Triangle
    tft.fillTriangle(playButton.x + 20, playButton.y + 6,
                     playButton.x + 20, playButton.y + 24,
                     playButton.x + 42, playButton.y + 15, ColorBg);
  }

  // Slot/preset first (moved left for quick access)
  drawRectButton(slotMinus, "<", ColorPanel);
  drawRectButton(slotValue, String(ui.activeSlot), ColorPanel2, ColorDim, ColorText, 0);
  drawRectButton(slotPlus, ">", ColorPanel);
  drawRectButton(saveButton, "S", ColorPanel2);
  drawRectButton(loadButton, "L", ColorPanel2);
  // BPM on the right — font 4 for digital retro look on value
  drawRectButton(bpmMinus, "-", ColorPanel);
  drawRectButton(bpmValue, String(ui.snapshot.bpm), ColorPanel2, ColorDim, ColorText, 0);
  drawRectButton(bpmPlus, "+", ColorPanel);
}

void drawTrackButtons() {
  for (int i = 0; i < TRACK_COUNT; ++i) {
    const uint16_t fill = (ui.snapshot.activeTrack == i) ? TrackColors[i] : ColorPanel;
    const uint16_t text = (ui.snapshot.activeTrack == i) ? ColorBg : TrackColors[i];
    drawRectButton(trackButtons[i], trackLabels[i], fill, TrackColors[i], text, 0);
  }
}

void drawRingView() {
  // Fill background only (no border rectangle around ring area)
  tft.fillRect(ringArea.x, ringArea.y, ringArea.w, ringArea.h, ColorBg);

  const int cx = ringArea.x + 70;
  const int cy = ringArea.y + 70;

  for (int track = 0; track < TRACK_COUNT; ++track) {
    if (track == VOICE_BASS) continue; // Bass does not use Euclidean patterns
    const int radius = 58 - (track * 10);
    const int len = ui.snapshot.patternLens[track];
    if (len <= 0) {
      continue;
    }

    for (int step = 0; step < len; ++step) {
      const float angle = ((2.0f * Pi) * step / len) - (Pi / 2.0f);
      const int px = cx + static_cast<int>(cosf(angle) * radius);
      const int py = cy + static_cast<int>(sinf(angle) * radius);
      const bool active = ui.snapshot.patterns[track][step] > 0;

      if (active) {
        const int dotRadius = (ui.snapshot.activeTrack == track) ? 3 : 2;
        tft.fillCircle(px, py, dotRadius, TrackColors[track]);
      } else if (ui.snapshot.activeTrack == track) {
        tft.drawCircle(px, py, 1, ColorDim);
      }
    }
  }

  const int pulseRadius = 10;
  tft.fillCircle(cx, cy, pulseRadius, ColorPanel2);
  const float playAngle =
      ((ui.snapshot.currentStep % 16) / 16.0f) * 2.0f * Pi - (Pi / 2.0f);
  const int headX = cx + static_cast<int>(cosf(playAngle) * 12.0f);
  const int headY = cy + static_cast<int>(sinf(playAngle) * 12.0f);
  tft.fillCircle(headX, headY, 3, ui.snapshot.isPlaying ? ColorPlay : ColorAccent);
  // EUCLID text removed
}

void drawBottomActions() {
  const bool isMuted = ui.snapshot.trackMutes[ui.snapshot.activeTrack];
  drawRectButton(muteToggle, isMuted ? "MUTED" : "MUTE",
                 isMuted ? ColorAccent : ColorPanel2,
                 ColorDim, ColorText, 2);

  const bool isVoiceMode = ui.snapshot.mode == UiMode::SOUND_EDIT;
  drawRectButton(voiceToggle, "VOICE",
                 isVoiceMode ? ColorSelect : ColorPanel2,
                 ColorDim, isVoiceMode ? ColorBg : ColorText, 2);

  const bool isMixMode = ui.snapshot.mode == UiMode::MIXER;
  drawRectButton(mixToggle, "MIX",
                 isMixMode ? ColorExpr : ColorPanel2,
                 ColorDim, ColorText, 2);
}

void sliderMeta(int index, String &label, int &displayValue, float &normalized,
                String &displayText) {
  displayText = ""; // empty = show displayValue as number
  const int track = ui.snapshot.activeTrack;
  const bool isBass = track == VOICE_BASS;
  const VoiceParams &params = ui.snapshot.voiceParams[track];

  if (ui.snapshot.mode == UiMode::SOUND_EDIT) {
    if (isBass) {
      switch (index) {
      case 0:
        label = "Pitch";
        normalized = params.pitch;
        displayValue = static_cast<int>(normalized * 100.0f);
        return;
      case 1:
        label = "Rel";
        normalized = params.decay;
        displayValue = static_cast<int>(normalized * 100.0f);
        return;
      case 2:
        label = "Bright";
        normalized = params.timbre;
        displayValue = static_cast<int>(normalized * 100.0f);
        return;
      default:
        label = "Drive";
        normalized = params.drive;
        displayValue = static_cast<int>(normalized * 100.0f);
        return;
      }
    }

    switch (index) {
    case 0:
      label = "Pitch";
      normalized = params.pitch;
      break;
    case 1:
      label = "Decay";
      normalized = params.decay;
      break;
    case 2:
      label = (track == VOICE_KICK) ? "Punch" : "Tone";
      normalized = params.timbre;
      break;
    default:
      label = (track == VOICE_KICK) ? "Drive" : "Level";
      normalized = (track == VOICE_KICK) ? params.drive : ui.snapshot.voiceGain[track];
      break;
    }
    displayValue = static_cast<int>(normalized * 100.0f);
    return;
  }

  if (isBass) {
    switch (index) {
    case 0:
      label = "Density";
      normalized = ui.snapshot.bassParams.density;
      displayValue = static_cast<int>(normalized * 100.0f);
      return;
    case 1:
      label = "Range";
      normalized = static_cast<float>(ui.snapshot.bassParams.range - 1) / 11.0f;
      displayValue = ui.snapshot.bassParams.range;
      return;
    case 2: {
      label = "Scale";
      normalized = static_cast<float>(static_cast<int>(ui.snapshot.bassParams.scaleType)) / 3.0f;
      displayValue = static_cast<int>(ui.snapshot.bassParams.scaleType);
      static const char *scaleNames[] = {"Min", "Maj", "Dor", "Phr"};
      displayText = scaleNames[constrain(displayValue, 0, 3)];
      return;
    }
    default: {
      label = "Root";
      normalized = static_cast<float>(ui.snapshot.bassParams.rootNote - 24) / 24.0f;
      displayValue = ui.snapshot.bassParams.rootNote;
      // Convert MIDI note to name: 24=C2, 25=C#2, ...
      static const char *noteNames[] = {"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
      int semitone = displayValue % 12;
      int octave   = (displayValue / 12) - 1;
      displayText  = String(noteNames[semitone]) + String(octave);
      return;
    }
    }
  }

  switch (index) {
  case 0:
    label = "Steps";
    normalized = static_cast<float>(ui.snapshot.trackSteps[track] - 1) / 63.0f;
    displayValue = ui.snapshot.trackSteps[track];
    return;
  case 1:
    label = "Hits";
    normalized = static_cast<float>(ui.snapshot.trackHits[track]) /
                 static_cast<float>(max(1, ui.snapshot.trackSteps[track]));
    displayValue = ui.snapshot.trackHits[track];
    return;
  case 2:
    label = "Rotate";
    normalized = static_cast<float>(ui.snapshot.trackRotations[track]) /
                 static_cast<float>(max(1, ui.snapshot.trackSteps[track] - 1));
    displayValue = ui.snapshot.trackRotations[track];
    return;
  default:
    label = "Vol";
    normalized = ui.snapshot.voiceGain[track];
    displayValue = static_cast<int>(normalized * 100.0f);
    return;
  }
}

void drawParameters() {
  // Clear right panel from x=196 (left of Vol-minus) — MIX right edge is now 192
  tft.fillRect(196, 44, 116, 191, ColorBg);

  for (int i = 0; i < 4; ++i) {
    String label;
    int displayValue = 0;
    float normalized = 0.0f;
    String displayText;
    sliderMeta(i, label, displayValue, normalized, displayText);

    const ParamHBox &box = paramBoxes[i];

    // Label — 12px above button
    tft.setTextColor(ColorText, ColorBg);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(label, box.minusBtn.x, box.minusBtn.y - 14);

    // Rotate row uses < > only for drum tracks in PATTERN_EDIT
    const bool isRotate = (i == 2 && ui.snapshot.activeTrack != VOICE_BASS
                           && ui.snapshot.mode != UiMode::SOUND_EDIT);
    bool minusActive = (ui.activeHoldParam == i && ui.activeHoldAction == -1);
    bool plusActive  = (ui.activeHoldParam == i && ui.activeHoldAction == 1);

    const String valStr = displayText.length() > 0 ? displayText : String(displayValue);
    drawRectButton(box.minusBtn, isRotate ? "<" : "-", minusActive ? ColorAccent : ColorPanel2);
    drawRectButton(box.valueBox, valStr, ColorBg, ColorDim, ColorText, 0);
    drawRectButton(box.plusBtn,  isRotate ? ">" : "+", plusActive  ? ColorAccent : ColorPanel2);
  }
}

void drawMixer() {
  tft.fillRect(196, 44, 116, 191, ColorBg);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    const Rect &box = mixerSliders[i];
    float gain = ui.snapshot.voiceGain[i];
    int fillH = static_cast<int>(gain * box.h);
    
    // Draw background track
    tft.fillRect(box.x, box.y, box.w, box.h - fillH, ColorPanel2);
    // Draw filled level
    tft.fillRect(box.x, box.y + box.h - fillH, box.w, fillH, TrackColors[i]);
    // Draw outline
    tft.drawRect(box.x, box.y, box.w, box.h, ColorDim);

    // Draw track label at the bottom
    tft.setTextColor(TrackColors[i], ColorBg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(trackLabels[i], box.x + box.w / 2, box.y + box.h + 12);
  }
}

// drawBottomActions covers this footer area now.

void drawScreen() {
  bool transportDirty = ui.forceRedraw || 
                        ui.snapshot.isPlaying != ui.lastSnapshot.isPlaying || 
                        ui.snapshot.bpm != ui.lastSnapshot.bpm || 
                        ui.activeSlot != ui.lastActiveSlot;
  if (transportDirty) drawTransport();

  bool ringBaseDirty = ui.forceRedraw || 
      ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack ||
      memcmp(ui.snapshot.patternLens, ui.lastSnapshot.patternLens, sizeof(ui.snapshot.patternLens)) != 0 ||
      memcmp(ui.snapshot.patterns, ui.lastSnapshot.patterns, sizeof(ui.snapshot.patterns)) != 0;

  if (ringBaseDirty) {
    drawRingView();
  } else if (ui.snapshot.currentStep != ui.lastSnapshot.currentStep || 
             ui.snapshot.isPlaying != ui.lastSnapshot.isPlaying) {
    const int cx = ringArea.x + 70;
    const int cy = ringArea.y + 70;
    
    // Erase old playhead (background is now ColorBg, not ColorPanel)
    const float oldPlayAngle =
        ((ui.lastSnapshot.currentStep % 16) / 16.0f) * 2.0f * Pi - (Pi / 2.0f);
    const int oldHeadX = cx + static_cast<int>(cosf(oldPlayAngle) * 12.0f);
    const int oldHeadY = cy + static_cast<int>(sinf(oldPlayAngle) * 12.0f);
    tft.fillCircle(oldHeadX, oldHeadY, 3, ColorBg);
    
    // Draw central pulse
    const int pulseRadius = 10;
    tft.fillCircle(cx, cy, pulseRadius, ColorPanel2);
    
    // Draw new playhead
    const float playAngle =
        ((ui.snapshot.currentStep % 16) / 16.0f) * 2.0f * Pi - (Pi / 2.0f);
    const int headX = cx + static_cast<int>(cosf(playAngle) * 12.0f);
    const int headY = cy + static_cast<int>(sinf(playAngle) * 12.0f);
    tft.fillCircle(headX, headY, 3, ui.snapshot.isPlaying ? ColorPlay : ColorAccent);
    // EUCLID text removed
  }

  bool bottomDirty = ui.forceRedraw || ui.snapshot.mode != ui.lastSnapshot.mode ||
                     ui.snapshot.trackMutes[ui.snapshot.activeTrack] != ui.lastSnapshot.trackMutes[ui.lastSnapshot.activeTrack] ||
                     ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack;
                     
  if (bottomDirty) drawBottomActions();

  bool tracksDirty = ui.forceRedraw || ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack;
  if (tracksDirty) drawTrackButtons();

  bool slidersDirty = ui.forceRedraw || 
      ui.snapshot.mode != ui.lastSnapshot.mode || 
      ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack ||
      ui.activeHoldParam != -1 || // Always redraw exactly when interacting
      memcmp(ui.snapshot.trackSteps, ui.lastSnapshot.trackSteps, sizeof(ui.snapshot.trackSteps)) != 0 ||
      memcmp(ui.snapshot.trackHits, ui.lastSnapshot.trackHits, sizeof(ui.snapshot.trackHits)) != 0 ||
      memcmp(ui.snapshot.trackRotations, ui.lastSnapshot.trackRotations, sizeof(ui.snapshot.trackRotations)) != 0 ||
      memcmp(ui.snapshot.trackMutes, ui.lastSnapshot.trackMutes, sizeof(ui.snapshot.trackMutes)) != 0 ||
      memcmp(ui.snapshot.voiceParams, ui.lastSnapshot.voiceParams, sizeof(ui.snapshot.voiceParams)) != 0 ||
      memcmp(ui.snapshot.voiceGain, ui.lastSnapshot.voiceGain, sizeof(ui.snapshot.voiceGain)) != 0 ||
      memcmp(&ui.snapshot.bassParams, &ui.lastSnapshot.bassParams, sizeof(BassGrooveParams)) != 0;

  if (slidersDirty) {
    if (ui.snapshot.mode == UiMode::MIXER) {
      drawMixer();
    } else {
      drawParameters();
    }
  }

  ui.lastSnapshot = ui.snapshot;
  ui.lastActiveSlot = ui.activeSlot;
  ui.activeHoldAction = 0;
  ui.forceRedraw = false;
}

void fireParamAction(int paramIndex, int amount) {
  const int track = ui.snapshot.activeTrack;
  const bool isBass = track == VOICE_BASS;

  String lbl;
  int displayValue;
  float norm;
  String dispText;
  sliderMeta(paramIndex, lbl, displayValue, norm, dispText);

  int newValue = displayValue + amount;

  if (ui.snapshot.mode == UiMode::SOUND_EDIT) {
    if (isBass) {
      postUiAction(UiActionType::SET_BASS_PARAM, paramIndex, newValue);
      return;
    }

    postUiAction(UiActionType::SET_SOUND_PARAM, paramIndex, newValue);
    return;
  }

  // PATTERN_EDIT Mode
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
    // Volume: adjust voice gain via SET_SOUND_PARAM index 3
    postUiAction(UiActionType::SET_SOUND_PARAM, 3, newValue);
    break;
  }
}

void handleTouch() {
  InputMgr.update();
  const TouchPoint &touch = InputMgr.state();
  const uint32_t now = millis();

  if (!touch.pressed && !touch.justReleased) {
    ui.activeHoldParam = -1;
    ui.activeHoldAction = 0;
    return;
  }

  if (touch.justPressed) {
    if (playButton.contains(touch.x, touch.y)) {
      postUiAction(UiActionType::TOGGLE_PLAY);
      return;
    }
    if (bpmMinus.contains(touch.x, touch.y)) {
      ui.activeHoldParam = 10;
      ui.activeHoldAction = -1;
      ui.holdTickIntervalMs = 400;
      ui.holdNextTickMs = now + ui.holdTickIntervalMs;
      ui.holdTickCount = 0;
      postUiAction(UiActionType::NUDGE_BPM, 0, -1);
      return;
    }
    if (bpmPlus.contains(touch.x, touch.y)) {
      ui.activeHoldParam = 10;
      ui.activeHoldAction = 1;
      ui.holdTickIntervalMs = 400;
      ui.holdNextTickMs = now + ui.holdTickIntervalMs;
      ui.holdTickCount = 0;
      postUiAction(UiActionType::NUDGE_BPM, 0, 1);
      return;
    }
    if (slotMinus.contains(touch.x, touch.y)) {
      ui.activeSlot = (ui.activeSlot == 0) ? (CYDConfig::PatternSlots - 1)
                                           : (ui.activeSlot - 1);
      return;
    }
    if (slotPlus.contains(touch.x, touch.y)) {
      ui.activeSlot = (ui.activeSlot + 1) % CYDConfig::PatternSlots;
      return;
    }
    if (saveButton.contains(touch.x, touch.y)) {
      setStatus(PatternStore.saveSlot(ui.activeSlot) ? "Pattern saved"
                                                     : "Save failed");
      return;
    }
    if (loadButton.contains(touch.x, touch.y)) {
      setStatus(PatternStore.loadSlot(ui.activeSlot) ? "Pattern loaded"
                                                     : "Load failed");
      return;
    }
    if (muteToggle.contains(touch.x, touch.y)) {
      postUiAction(UiActionType::TOGGLE_MUTE, ui.snapshot.activeTrack, ui.snapshot.activeTrack);
      return;
    }
    if (voiceToggle.contains(touch.x, touch.y)) {
      postUiAction(UiActionType::CHANGE_MODE, 0,
                   ui.snapshot.mode == UiMode::SOUND_EDIT
                     ? static_cast<int>(UiMode::PATTERN_EDIT)
                     : static_cast<int>(UiMode::SOUND_EDIT));
      return;
    }
    if (mixToggle.contains(touch.x, touch.y)) {
      postUiAction(UiActionType::CHANGE_MODE, 0,
                   ui.snapshot.mode == UiMode::MIXER
                     ? static_cast<int>(UiMode::PATTERN_EDIT)
                     : static_cast<int>(UiMode::MIXER));
      return;
    }
    for (int i = 0; i < TRACK_COUNT; ++i) {
      if (trackButtons[i].contains(touch.x, touch.y)) {
        postUiAction(UiActionType::SELECT_TRACK, 0, i);
        return;
      }
    }
    
    if (ui.snapshot.mode != UiMode::MIXER) {
      for (int i = 0; i < 4; ++i) {
        const ParamHBox &box = paramBoxes[i];
        if (box.minusBtn.contains(touch.x, touch.y)) {
          ui.activeHoldParam = i;
          ui.activeHoldAction = -1;
          ui.holdTickIntervalMs = 400; // First tick is slower
          ui.holdNextTickMs = now + ui.holdTickIntervalMs;
          ui.holdTickCount = 0;
          fireParamAction(i, -1);
          return;
        }
        if (box.plusBtn.contains(touch.x, touch.y)) {
          ui.activeHoldParam = i;
          ui.activeHoldAction = 1;
          ui.holdTickIntervalMs = 400;
          ui.holdNextTickMs = now + ui.holdTickIntervalMs;
          ui.holdTickCount = 0;
          fireParamAction(i, 1);
          return;
        }
      }
    }
  }

  // Handle Mixer dragging
  if (touch.pressed && ui.snapshot.mode == UiMode::MIXER) {
    for (int i = 0; i < TRACK_COUNT; ++i) {
      if (touch.x >= mixerSliders[i].x - 6 && touch.x <= mixerSliders[i].x + mixerSliders[i].w + 6 &&
          touch.y >= mixerSliders[i].y - 12 && touch.y <= mixerSliders[i].y + mixerSliders[i].h + 12) {
        float gain = 1.0f - static_cast<float>(touch.y - mixerSliders[i].y) / mixerSliders[i].h;
        gain = constrain(gain, 0.0f, 1.0f);
        postUiAction(UiActionType::SET_VOICE_GAIN, i, static_cast<int>(gain * 100.0f));
      }
    }
  }

  // Handle Hold-to-increment accelerating ticking
  if (touch.pressed && ui.activeHoldParam >= 0 && ui.activeHoldAction != 0) {
    if (now >= ui.holdNextTickMs) {
      ui.holdTickCount++;
      
      int amount = ui.activeHoldAction;
      if (ui.holdTickCount > 15) {
        amount *= 5;
      } else if (ui.holdTickCount > 6) {
        amount *= 2;
      }

      if (ui.activeHoldParam == 10) {
        postUiAction(UiActionType::NUDGE_BPM, 0, amount);
      } else {
        fireParamAction(ui.activeHoldParam, amount);
      }
      
      // Accelerate the ticking rate down to 40ms max speed
      if (ui.holdTickIntervalMs > 40) {
        ui.holdTickIntervalMs = (ui.holdTickIntervalMs * 8) / 10;
        if (ui.holdTickIntervalMs < 40) {
          ui.holdTickIntervalMs = 40;
        }
      }
      ui.holdNextTickMs = now + ui.holdTickIntervalMs;
    }
  }

  if (touch.justReleased) {
    ui.activeHoldParam = -1;
    ui.activeHoldAction = 0;
    ui.holdTickCount = 0;
  }
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

void displayTask(void *parameter) {
  while (!engine.engineReady.load()) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  pinMode(CYDConfig::TftBacklight, OUTPUT);
  digitalWrite(CYDConfig::TftBacklight, HIGH);

  tft.init();
  tft.setRotation(CYDConfig::ScreenRotation);
  
  // Initialize dedicated touch SPI *after* TFT to prevent TFT_eSPI
  // from redefining or conflicting with the standard SPI host.
  InputMgr.begin();
  tft.fillScreen(ColorBg);
  tft.setFont(&fonts::FreeMonoBold9pt7b); // Bold mono for legibility at small sizes

  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t frameTicks = pdMS_TO_TICKS(CYDConfig::UiFrameMs);

  for (;;) {
    ui.snapshot.capture();
    handleTouch();
    drawScreen();
    vTaskDelayUntil(&lastWake, frameTicks);
  }
}
