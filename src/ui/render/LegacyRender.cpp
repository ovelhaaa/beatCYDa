#include "LegacyRender.h"
#include "../core/UiLayout.h"
#include "../LGFX_CYD.h"
#include <math.h>
#include <string.h>

namespace {

static LGFX_Sprite s_ring(&tft);
static LGFX_Sprite s_panel(&tft);

constexpr uint16_t C_BG = 0x0863u;
constexpr uint16_t C_PANEL = 0x1947u;
constexpr uint16_t C_PANEL2 = 0x2104u;
constexpr uint16_t C_DIM = 0x4228u;
constexpr uint16_t C_TEXT = 0xBDF8u;
constexpr uint16_t C_ACCENT = 0xFEC9u;
constexpr uint16_t C_SELECT = 0x4F1Fu;
constexpr uint16_t C_EXPR = 0xFA74u;
constexpr uint16_t C_PLAY = 0x7FEEu;

uint16_t blend16(uint16_t fg, uint16_t bg, uint8_t a) {
  uint8_t r = static_cast<uint8_t>(((fg >> 11) * a + (bg >> 11) * (255u - a)) / 255u);
  uint8_t g = static_cast<uint8_t>((((fg >> 5) & 63u) * a + ((bg >> 5) & 63u) * (255u - a)) / 255u);
  uint8_t b = static_cast<uint8_t>(((fg & 31u) * a + (bg & 31u) * (255u - a)) / 255u);
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

void drawBtn(LGFX_Sprite *spr, const Rect *r, const char *label, uint16_t fill, uint16_t outline,
             uint16_t textCol) {
  if (spr) {
    spr->fillRoundRect(r->x, r->y, r->w, r->h, 3, fill);
    spr->drawRoundRect(r->x, r->y, r->w, r->h, 3, outline);
    spr->setTextColor(textCol, fill);
    spr->setTextDatum(MC_DATUM);
    spr->drawString(label, r->x + r->w / 2, r->y + r->h / 2 - 1);
    return;
  }

  tft.fillRoundRect(r->x, r->y, r->w, r->h, 3, fill);
  tft.drawRoundRect(r->x, r->y, r->w, r->h, 3, outline);
  tft.setTextColor(textCol, fill);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, r->x + r->w / 2, r->y + r->h / 2 - 1);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW
   ═══════════════════════════════════════════════════════════════════════════════ */
void drawTransport(const UiRuntime &ui) {
  char buf[8];
  tft.fillRect(0, 0, CYDConfig::ScreenWidth, 42, C_BG);

  bool pl = ui.snapshot.isPlaying;
  tft.fillRect(R_PLAY.x, R_PLAY.y, R_PLAY.w, R_PLAY.h, pl ? C_ACCENT : C_PLAY);
  tft.drawRect(R_PLAY.x, R_PLAY.y, R_PLAY.w, R_PLAY.h, C_DIM);
  if (pl) {
    tft.fillRect(R_PLAY.x + 20, R_PLAY.y + 7, 10, 16, C_BG);
    tft.fillRect(R_PLAY.x + 36, R_PLAY.y + 7, 10, 16, C_BG);
  } else {
    tft.fillTriangle(R_PLAY.x + 18, R_PLAY.y + 5, R_PLAY.x + 18, R_PLAY.y + 25, R_PLAY.x + 44,
                     R_PLAY.y + 15, C_BG);
  }

  drawBtn(nullptr, &R_SLOT_DEC, "<", C_PANEL, C_DIM, C_TEXT);
  snprintf(buf, sizeof(buf), "%d", ui.activeSlot + 1);
  drawBtn(nullptr, &R_SLOT_VAL, buf, C_PANEL2, C_DIM, C_TEXT);
  drawBtn(nullptr, &R_SLOT_INC, ">", C_PANEL, C_DIM, C_TEXT);

  drawBtn(nullptr, &R_SAVE, "S", C_PANEL2, C_DIM, C_TEXT);
  drawBtn(nullptr, &R_LOAD, "L", C_PANEL2, C_DIM, C_TEXT);

  drawBtn(nullptr, &R_BPM_DEC, "-", C_PANEL, C_DIM, C_TEXT);
  snprintf(buf, sizeof(buf), "%3d", ui.snapshot.bpm);
  drawBtn(nullptr, &R_BPM_VAL, buf, C_PANEL2, C_DIM, C_ACCENT);
  drawBtn(nullptr, &R_BPM_INC, "+", C_PANEL, C_DIM, C_TEXT);

  uint16_t barCol = (ui.statusUntilMs > millis()) ? C_ACCENT : C_DIM;
  tft.drawFastHLine(0, 41, CYDConfig::ScreenWidth, barCol);
}

void drawStatus(const UiRuntime &ui) {
  bool active = (ui.statusUntilMs > millis());
  const char *msg = active ? ui.status : ui.snapshot.isPlaying ? "PLAYING" : "READY";
  drawBtn(nullptr, &R_BPM_VAL, msg, C_PANEL2, active ? C_ACCENT : C_DIM,
          active ? static_cast<uint16_t>(C_PLAY) : static_cast<uint16_t>(C_TEXT));
}

void drawTracks(const UiRuntime &ui) {
  char lbl[2] = {0, 0};
  for (int i = 0; i < TRACK_COUNT; i++) {
    bool sel = (ui.snapshot.activeTrack == i);
    bool mut = ui.snapshot.trackMutes[i];
    uint16_t col = TRACK_COLORS[i];
    uint16_t fill = sel ? blend16(col, C_BG, 55) : (mut ? C_PANEL2 : C_PANEL);
    uint16_t brd = mut ? blend16(col, C_BG, 50) : col;
    uint16_t txt = sel ? C_BG : (mut ? blend16(col, C_BG, 60) : col);
    lbl[0] = TRACK_CHARS[i];
    drawBtn(nullptr, &R_TRACKS[i], lbl, fill, brd, txt);
    if (mut && !sel) {
      tft.drawLine(R_TRACKS[i].x + 2, R_TRACKS[i].y + 2, R_TRACKS[i].x + R_TRACKS[i].w - 3,
                   R_TRACKS[i].y + R_TRACKS[i].h - 3, C_DIM);
    }
  }
}

void drawRing(const UiRuntime &ui) {
  s_ring.fillSprite(C_BG);

  for (int t = 0; t < TRACK_COUNT; t++) {
    uint8_t a = (t == ui.snapshot.activeTrack) ? 55u : 18u;
    s_ring.drawCircle(RING_CX, RING_CY, RING_RADII[t], blend16(TRACK_COLORS[t], C_BG, a));
  }

  if (ui.snapshot.isPlaying) {
    float ang = ((ui.snapshot.currentStep % 16) / 16.0f) * 2.0f * static_cast<float>(M_PI) -
                static_cast<float>(M_PI / 2.0);
    s_ring.drawLine(RING_CX, RING_CY, RING_CX + static_cast<int>(66 * cosf(ang)),
                    RING_CY + static_cast<int>(66 * sinf(ang)), blend16(C_PLAY, C_BG, 18));
  }

  for (int t = 0; t < TRACK_COUNT; t++) {
    int len = ui.snapshot.patternLens[t];
    if (len <= 0)
      continue;
    bool sel = (t == ui.snapshot.activeTrack);
    bool mut = ui.snapshot.trackMutes[t];
    uint16_t col = TRACK_COLORS[t];
    uint8_t rad = RING_RADII[t];
    int dr = sel ? 3 : 2;

    for (int i = 0; i < len; i++) {
      float ang = (2.0f * static_cast<float>(M_PI) * i / len) - static_cast<float>(M_PI / 2.0);
      int16_t px = static_cast<int16_t>(RING_CX + rad * cosf(ang));
      int16_t py = static_cast<int16_t>(RING_CY + rad * sinf(ang));
      bool active = ui.snapshot.patterns[t][i] > 0;
      bool playhead = ui.snapshot.isPlaying && (i == ui.snapshot.currentStep % len);

      if (mut) {
        if (active)
          s_ring.drawCircle(px, py, dr - 1, blend16(col, C_BG, 25));
      } else if (playhead && active) {
        s_ring.fillCircle(px, py, dr + 1, TFT_WHITE);
      } else if (playhead) {
        s_ring.drawCircle(px, py, dr, col);
      } else if (active) {
        s_ring.fillCircle(px, py, dr, sel ? col : blend16(col, C_BG, 80));
      } else if (sel) {
        s_ring.drawCircle(px, py, 1, C_DIM);
      }
    }
  }

  int pr = ui.snapshot.isPlaying ? ((ui.snapshot.currentStep % 4 == 0) ? 5 : 3) : 3;
  s_ring.fillCircle(RING_CX, RING_CY, pr, ui.snapshot.isPlaying ? C_PLAY : C_PANEL2);
  s_ring.fillCircle(RING_CX, RING_CY, 1, ui.snapshot.isPlaying ? C_BG : C_DIM);

  s_ring.pushSprite(R_RING.x, R_RING.y);
}

void drawFooter(const UiRuntime &ui) {
  bool mut = ui.snapshot.trackMutes[ui.snapshot.activeTrack];
  bool voice = (ui.mode == UiMode::SOUND_EDIT);
  bool mix = (ui.mode == UiMode::MIXER);

  drawBtn(nullptr, &R_MUTE, mut ? "MUTED" : "MUTE", mut ? C_ACCENT : C_PANEL2, C_DIM, C_TEXT);
  drawBtn(nullptr, &R_VOICE, "VOICE", voice ? C_SELECT : C_PANEL2, C_DIM,
          voice ? static_cast<uint16_t>(C_BG) : static_cast<uint16_t>(C_TEXT));
  drawBtn(nullptr, &R_MIX, "MIX", mix ? C_EXPR : C_PANEL2, C_DIM, C_TEXT);
}

struct ParamMeta {
  char label[12];
  char value[12];
  float norm;
};

void paramMeta(const UiRuntime &ui, int row, ParamMeta *out) {
  out->label[0] = '\0';
  out->value[0] = '\0';
  out->norm = 0.0f;

  const int tr = ui.snapshot.activeTrack;
  const bool bass = (tr == VOICE_BASS);
  const VoiceParams &vp = ui.snapshot.voiceParams[tr];

  if (ui.mode == UiMode::SOUND_EDIT) {
    static const char *const PITCH_LBL[TRACK_COUNT] = {"Pitch", "Tune", "Freq", "Freq", "Pitch"};
    static const char *const DECAY_LBL[TRACK_COUNT] = {"Decay", "Snap", "Decay", "Ring", "Glide"};
    static const char *const TIMB_LBL[TRACK_COUNT] = {"Punch", "Tone", "Open", "Open", "Timbre"};
    static const char *const DRIVE_LBL[TRACK_COUNT] = {"Drive", "Crack", "Level", "Level", "Level"};
    float vals[4] = {vp.pitch, vp.decay, vp.timbre, vp.drive};
    if (!bass && row == 3 && tr != VOICE_KICK) {
      vals[3] = ui.snapshot.voiceGain[tr];
    }

    const char *lbls[4] = {PITCH_LBL[tr], DECAY_LBL[tr], TIMB_LBL[tr], DRIVE_LBL[tr]};
    if (!bass && row == 3 && tr != VOICE_KICK) {
      lbls[3] = "Level";
    }

    if (row < 4) {
      snprintf(out->label, sizeof(out->label), "%s", lbls[row]);
      snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(ui, row));
      out->norm = vals[row];
    }
    return;
  }

  if (bass) {
    static const char *const SCALE_NAMES[] = {"Chrom", "Major", "Minor", "Penta", "Blues"};
    static const char *const NOTE_NAMES[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A",
                                             "Bb", "B"};
    const BassGrooveParams &bp = ui.snapshot.bassParams;
    switch (row) {
    case 0:
      snprintf(out->label, sizeof(out->label), "Density");
      snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(ui, row));
      out->norm = bp.density;
      break;
    case 1:
      snprintf(out->label, sizeof(out->label), "Range");
      snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(ui, row));
      out->norm = static_cast<float>(bp.range - 1) / 11.0f;
      break;
    case 2: {
      snprintf(out->label, sizeof(out->label), "Scale");
      int sc = constrain(static_cast<int>(bp.scaleType), 0, 4);
      snprintf(out->value, sizeof(out->value), "%s", SCALE_NAMES[sc]);
      out->norm = static_cast<float>(sc) / 4.0f;
      break;
    }
    case 3:
      snprintf(out->label, sizeof(out->label), "Root");
      snprintf(out->value, sizeof(out->value), "%s%d", NOTE_NAMES[bp.rootNote % 12],
               bp.rootNote / 12 - 1);
      out->norm = static_cast<float>(bp.rootNote - 24) / 36.0f;
      break;
    default:
      break;
    }
    return;
  }

  int steps = ui.snapshot.trackSteps[tr];
  switch (row) {
  case 0:
    snprintf(out->label, sizeof(out->label), "Steps");
    snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(ui, row));
    out->norm = static_cast<float>(steps - 4) / 60.0f;
    break;
  case 1:
    snprintf(out->label, sizeof(out->label), "Hits");
    snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(ui, row));
    out->norm = steps > 0 ? static_cast<float>(ui.snapshot.trackHits[tr]) / steps : 0.0f;
    break;
  case 2:
    snprintf(out->label, sizeof(out->label), "Rot");
    snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(ui, row));
    out->norm = steps > 1 ? static_cast<float>(ui.snapshot.trackRotations[tr]) / (steps - 1) : 0.0f;
    break;
  case 3:
    snprintf(out->label, sizeof(out->label), "Vol");
    snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(ui, row));
    out->norm = ui.snapshot.voiceGain[tr];
    break;
  default:
    break;
  }
}

void drawParams(const UiRuntime &ui) {
  uint16_t tc = TRACK_COLORS[ui.snapshot.activeTrack];

  s_panel.fillSprite(C_BG);
  s_panel.setTextColor(tc, C_BG);
  s_panel.setTextDatum(MC_DATUM);
  s_panel.drawString(TRACK_LABELS[ui.snapshot.activeTrack], PANEL_W / 2, 8);
  s_panel.drawFastHLine(0, 15, PANEL_W, C_DIM);

  if (ui.mode == UiMode::MIXER) {
    int sh = PANEL_H - 22;
    int sw = (PANEL_W - 10) / TRACK_COUNT;
    for (int i = 0; i < TRACK_COUNT; i++) {
      int sx = 5 + i * sw, sy = 20;
      int filled = static_cast<int>(ui.snapshot.voiceGain[i] * sh);
      bool mut = ui.snapshot.trackMutes[i];
      s_panel.fillRect(sx, sy, sw - 2, sh, C_PANEL);
      s_panel.fillRect(sx, sy + sh - filled, sw - 2, filled,
                       mut ? blend16(TRACK_COLORS[i], C_BG, 40) : TRACK_COLORS[i]);
      s_panel.drawRect(sx, sy, sw - 2, sh, C_DIM);
      s_panel.setTextColor(TRACK_COLORS[i], C_BG);
      s_panel.setTextDatum(MC_DATUM);
      char lbl[2] = {TRACK_CHARS[i], '\0'};
      s_panel.drawString(lbl, sx + (sw - 2) / 2, sy + sh + 7);
    }
  } else {
    for (int row = 0; row < 4; row++) {
      ParamMeta m;
      paramMeta(ui, row, &m);
      if (m.label[0] == '\0')
        continue;

      const ParamRow *pr = &PARAM_ROWS[row];
      int bw = static_cast<int>(m.norm * static_cast<float>(pr->value.w - 4));
      s_panel.fillRect(pr->value.x + 2, pr->value.y - 5, pr->value.w - 4, 3, C_PANEL);
      if (bw > 0)
        s_panel.fillRect(pr->value.x + 2, pr->value.y - 5, bw, 3, tc);

      s_panel.setTextColor(C_DIM, C_BG);
      s_panel.setTextDatum(TL_DATUM);
      s_panel.drawString(m.label, pr->minus.x, pr->touch.y + 2);

      drawBtn(&s_panel, &pr->minus, "-", C_PANEL, C_DIM, C_TEXT);
      drawBtn(&s_panel, &pr->value, m.value, C_PANEL2, tc, C_TEXT);
      drawBtn(&s_panel, &pr->plus, "+", C_PANEL, C_DIM, C_TEXT);
    }
  }

  s_panel.pushSprite(PANEL_SX, PANEL_SY);
}

} // namespace

void initLegacyRender() {
  tft.fillScreen(C_BG);
  tft.setFont(&fonts::FreeMonoBold9pt7b);

  s_ring.createSprite(R_RING.w, R_RING.h);
  s_ring.setColorDepth(16);
  s_panel.createSprite(PANEL_W, PANEL_H);
  s_panel.setColorDepth(16);
}

void renderLegacyFrame(UiRuntime &ui, uint32_t now, uint32_t &lastFullMs, uint32_t &lastRingMs) {
  updateUiInvalidation(ui, now, lastFullMs);

  tft.startWrite();
  if (ui.invalidation.fullScreenDirty) {
    tft.fillScreen(C_BG);
    drawTransport(ui);
    drawTracks(ui);
    drawFooter(ui);
    drawParams(ui);
    lastFullMs = now;
    ui.forceRedraw = false;
  } else {
    if (ui.invalidation.topBarDirty) {
      drawTransport(ui);
    }

    if (ui.invalidation.toastDirty && !ui.invalidation.topBarDirty) {
      drawStatus(ui);
    }

    if (ui.invalidation.ringDirty || ui.invalidation.bottomNavDirty) {
      drawTracks(ui);
    }

    if (ui.invalidation.bottomNavDirty) {
      drawFooter(ui);
    }

    if (ui.invalidation.panelDirty) {
      drawParams(ui);
    }
  }
  tft.endWrite();

  if (ui.invalidation.ringDirty || ui.invalidation.fullScreenDirty) {
    if (now - lastRingMs >= 33u) {
      drawRing(ui);
      lastRingMs = now;
    }
  }
}
