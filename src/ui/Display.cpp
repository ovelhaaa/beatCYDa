#include "Display.h"
#include "LGFX_CYD.h"
#include "UiAction.h"
#include "../control/ControlManager.h"
#include "../control/InputManager.h"
#include "../storage/PatternStorage.h"
#include <SPI.h>
#include <math.h>
#include <string.h>

/* ── Display object (declared extern in LGFX_CYD.h) ─────────────────────────── */
LGFX_CYD tft;

namespace {

/* ── Sprites ──────────── */
static LGFX_Sprite s_center(&tft);   /* 148 × 148 — center area  */
static LGFX_Sprite s_panel(&tft);  /* 104 × 148 — right parameter panel */

/* ═══════════════════════════════════════════════════════════════════════════════
   COLOUR PALETTE  (RGB565)
   ═══════════════════════════════════════════════════════════════════════════════ */
constexpr uint16_t C_BG       = 0x10A2u;   /* dark graphite      */
constexpr uint16_t C_PANEL    = 0x2124u;   /* 1 tone lighter     */
constexpr uint16_t C_PANEL2   = 0x31A6u;   /* slightly lighter   */
constexpr uint16_t C_DIM      = 0x4A49u;   /* borders / subdued  */
constexpr uint16_t C_TEXT     = 0xDF7Du;   /* off-white          */
constexpr uint16_t C_ACCENT   = 0xFC00u;   /* amber/orange       */
constexpr uint16_t C_SELECT   = 0x051Du;   /* cyan/blue          */
constexpr uint16_t C_PLAY     = 0x05E8u;   /* green              */
constexpr uint16_t C_STOP     = 0xF104u;   /* red                */

static const uint16_t TRACK_COLORS[TRACK_COUNT] = {
    0xCAA9u,   /* Kick  — terracotta */
    0xD568u,   /* Snare — warm gold  */
    0x4F1Fu,   /* HatC  — steel teal */
    0xFA74u,   /* HatO  — mauve      */
    0x5D6Cu,   /* Bass  — muted sage */
};

static const char TRACK_CHARS[TRACK_COUNT] = {'K','S','C','O','B'};
static const char * const TRACK_LABELS[TRACK_COUNT] = {"KICK","SNRE","HATC","HATO","BASS"};

/* ═══════════════════════════════════════════════════════════════════════════════
   LAYOUT  (pixel coords, landscape 320 × 240)
   ═══════════════════════════════════════════════════════════════════════════════ */

/* Generic rectangle with hit-test */
struct Rect {
    int16_t x, y, w, h;
    bool contains(int16_t px, int16_t py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

/* HEADER: y=0..39 */
static const Rect R_PLAY      = { 4,   4,  52,  32};
static const Rect R_BPM_DEC   = { 64,  4,  28,  32};
static const Rect R_BPM_VAL   = { 94,  4,  52,  32};
static const Rect R_BPM_INC   = {148,  4,  28,  32};
static const Rect R_SLOT_DEC  = {184,  4,  24,  32};
static const Rect R_SLOT_VAL  = {210,  4,  28,  32};
static const Rect R_SLOT_INC  = {240,  4,  24,  32};
static const Rect R_SAVE      = {272,  4,  44,  14};
static const Rect R_LOAD      = {272, 22,  44,  14};

/* STATUS: y=40..55 */
static const Rect R_STATUS    = { 0,  40, 320,  16};

/* TRACKS: x=0..67, y=56..203 */
static const Rect R_TRACKS[TRACK_COUNT] = {
    { 2,  56, 46, 28},
    { 2,  86, 46, 28},
    { 2, 116, 46, 28},
    { 2, 146, 46, 28},
    { 2, 176, 46, 28},
};
static const Rect R_MUTES[TRACK_COUNT] = {
    { 50,  56, 16, 28},
    { 50,  86, 16, 28},
    { 50, 116, 16, 28},
    { 50, 146, 16, 28},
    { 50, 176, 16, 28},
};

/* CENTER: x=68..215, y=56..203 */
static const Rect R_CENTER = {68, 56, 148, 148};
#define RING_CX 74
#define RING_CY 74
static const uint8_t RING_RADII[TRACK_COUNT] = {66, 54, 42, 30, 18};

/* PANEL: x=216..319, y=56..203 */
#define PANEL_SX 216
#define PANEL_SY  56
#define PANEL_W  104
#define PANEL_H  148

/* 4 param rows inside the panel sprite */
struct ParamRow {
    Rect touch;  /* full row, for hit-testing */
    Rect minus;
    Rect value;
    Rect plus;
};

static const ParamRow PARAM_ROWS[4] = {
    {{0,   0, PANEL_W, 36}, { 2, 14, 24, 22}, { 28, 14, 48, 22}, { 78, 14, 24, 22}},
    {{0,  37, PANEL_W, 36}, { 2, 51, 24, 22}, { 28, 51, 48, 22}, { 78, 51, 24, 22}},
    {{0,  74, PANEL_W, 36}, { 2, 88, 24, 22}, { 28, 88, 48, 22}, { 78, 88, 24, 22}},
    {{0, 111, PANEL_W, 36}, { 2,125, 24, 22}, { 28,125, 48, 22}, { 78,125, 24, 22}},
};

/* MIXER: x=68..319, y=56..203 */
static const Rect R_MIXER_SLIDERS[TRACK_COUNT] = {
    { 76, 68, 36, 110},
    {124, 68, 36, 110},
    {172, 68, 36, 110},
    {220, 68, 36, 110},
    {268, 68, 36, 110},
};

/* TABS: y=204..239 */
static const Rect R_TAB_SEQ   = {  0, 204, 106,  36};
static const Rect R_TAB_SOUND = {106, 204, 107,  36};
static const Rect R_TAB_MIX   = {213, 204, 107,  36};

/* ═══════════════════════════════════════════════════════════════════════════════
   RUNTIME STATE
   ═══════════════════════════════════════════════════════════════════════════════ */

struct UiRuntime {
  UiStateSnapshot snapshot;
  UiStateSnapshot lastSnapshot;
  uint8_t activeSlot = 0;
  uint8_t lastActiveSlot = 255;
  UiMode mode = UiMode::PATTERN_EDIT;
  UiMode lastMode = (UiMode)0xFF;
  
  // Hold-to-increment tracking
  int activeHoldAction = -1; 
  int activeHoldParam = -1;
  uint32_t holdNextTickMs = 0;
  int holdTickCount = 0;

  // Track button hold for mute
  int activeTrackHold = -1;
  uint32_t trackHoldStartMs = 0;

  char status[64] = "CYD ready";
  char lastStatus[64] = "";
  uint32_t statusUntilMs = 0;

  // Dirty regions
  bool dirtyHeader = true;
  bool dirtyStatus = true;
  bool dirtyTracks = true;
  bool dirtyTabs   = true;
  bool dirtyCenter = true;
  bool dirtyPanel  = true;
  bool forceFull   = true;
} ui;

/* ═══════════════════════════════════════════════════════════════════════════════
   HELPERS
   ═══════════════════════════════════════════════════════════════════════════════ */

/* Blend two RGB565 colours; alpha 0=bg .. 255=fg */
static uint16_t blend16(uint16_t fg, uint16_t bg, uint8_t a) {
    uint8_t r = (uint8_t)(((fg >> 11) * a + (bg >> 11) * (255u - a)) / 255u);
    uint8_t g = (uint8_t)((((fg >> 5) & 63u) * a + ((bg >> 5) & 63u) * (255u - a)) / 255u);
    uint8_t b = (uint8_t)(((fg & 31u) * a + (bg & 31u) * (255u - a)) / 255u);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static uint32_t hold_interval(uint8_t count) {
    if (count < 6)  return 300;
    if (count < 15) return 120;
    return 50;
}

/* Rounded-rect button — works on both tft and a sprite */
static void draw_btn(LGFX_Sprite *spr, const Rect *r,
                     const char *label, uint16_t fill,
                     uint16_t outline, uint16_t text_col) {
    if (spr) {
        spr->fillRoundRect(r->x, r->y, r->w, r->h, 2, fill);
        spr->drawRoundRect(r->x, r->y, r->w, r->h, 2, outline);
        spr->setTextColor(text_col, fill);
        spr->setTextDatum(MC_DATUM);
        spr->drawString(label, r->x + r->w / 2, r->y + r->h / 2 - 1);
    } else {
        tft.fillRoundRect(r->x, r->y, r->w, r->h, 2, fill);
        tft.drawRoundRect(r->x, r->y, r->w, r->h, 2, outline);
        tft.setTextColor(text_col, fill);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(label, r->x + r->w / 2, r->y + r->h / 2 - 1);
    }
}

static void postUiAction(UiActionType t, int idx=0, int val=0) {
    IPCCommand cmd{};
    cmd.type    = CommandType::UI_ACTION;
    cmd.voiceId = (uint8_t)t;
    cmd.paramId = (uint8_t)idx;
    cmd.value   = (float)val;
    CtrlMgr.sendCommand(cmd);
}

static void set_status(const char *msg, uint32_t ms = 2000) {
    snprintf(ui.status, sizeof(ui.status), "%s", msg);
    ui.statusUntilMs = millis() + ms;
    ui.dirtyStatus = true;
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — HEADER (y 0–39)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_header(void) {
    char buf[16];
    tft.fillRect(0, 0, CYDConfig::ScreenWidth, 40, C_BG);

    /* Play / Stop */
    bool pl = ui.snapshot.isPlaying;
    tft.fillRoundRect(R_PLAY.x, R_PLAY.y, R_PLAY.w, R_PLAY.h, 2, pl ? C_PLAY : C_STOP);
    tft.drawRoundRect(R_PLAY.x, R_PLAY.y, R_PLAY.w, R_PLAY.h, 2, C_DIM);
    if (pl) {
        tft.fillRect(R_PLAY.x + 16, R_PLAY.y + 8,  8, 16, C_TEXT);
        tft.fillRect(R_PLAY.x + 28, R_PLAY.y + 8,  8, 16, C_TEXT);
    } else {
        tft.fillTriangle(R_PLAY.x + 16, R_PLAY.y + 6,
                         R_PLAY.x + 16, R_PLAY.y + 26,
                         R_PLAY.x + 36, R_PLAY.y + 16, C_TEXT);
    }

    /* BPM */
    draw_btn(NULL, &R_BPM_DEC, "-", C_PANEL, C_DIM, C_TEXT);
    snprintf(buf, sizeof(buf), "%3d", ui.snapshot.bpm);
    draw_btn(NULL, &R_BPM_VAL, buf, C_PANEL2, C_DIM, C_ACCENT);
    draw_btn(NULL, &R_BPM_INC, "+", C_PANEL, C_DIM, C_TEXT);

    /* Pattern slot */
    draw_btn(NULL, &R_SLOT_DEC, "<", C_PANEL, C_DIM, C_TEXT);
    snprintf(buf, sizeof(buf), "%02d", ui.activeSlot + 1);
    draw_btn(NULL, &R_SLOT_VAL, buf, C_PANEL2, C_DIM, C_TEXT);
    draw_btn(NULL, &R_SLOT_INC, ">", C_PANEL, C_DIM, C_TEXT);

    /* Save / Load */
    draw_btn(NULL, &R_SAVE, "SAVE", C_PANEL2, C_DIM, C_TEXT);
    draw_btn(NULL, &R_LOAD, "LOAD", C_PANEL2, C_DIM, C_TEXT);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — STATUS (y 40–55)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_status(void) {
    tft.fillRect(R_STATUS.x, R_STATUS.y, R_STATUS.w, R_STATUS.h, C_PANEL);
    tft.drawFastHLine(R_STATUS.x, R_STATUS.y + R_STATUS.h - 1, R_STATUS.w, C_DIM);
    tft.drawFastHLine(R_STATUS.x, R_STATUS.y, R_STATUS.w, C_DIM);

    tft.setTextColor(C_TEXT, C_PANEL);
    tft.setTextDatum(MC_DATUM);

    char buf[64];
    if (ui.statusUntilMs > millis()) {
        snprintf(buf, sizeof(buf), "%s", ui.status);
        tft.setTextColor(C_ACCENT, C_PANEL);
    } else {
        const char* modeStr = "SEQ";
        if (ui.mode == UiMode::SOUND_EDIT) modeStr = "SOUND";
        else if (ui.mode == UiMode::MIXER) modeStr = "MIX";
        snprintf(buf, sizeof(buf), "TRK: %s | MODE: %s | S: %02d",
                 TRACK_LABELS[ui.snapshot.activeTrack], modeStr, ui.snapshot.currentStep + 1);
    }
    tft.drawString(buf, R_STATUS.x + R_STATUS.w / 2, R_STATUS.y + R_STATUS.h / 2 - 1);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — TRACKS (x 0–67, y 56–203)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_tracks(void) {
    tft.fillRect(0, 56, 68, 148, C_BG);
    for (int i = 0; i < TRACK_COUNT; i++) {
        bool sel = (ui.snapshot.activeTrack == i);
        bool mut = ui.snapshot.trackMutes[i];
        uint16_t col  = TRACK_COLORS[i];

        uint16_t fill = sel ? blend16(col, C_BG, 160) : C_PANEL;
        uint16_t brd  = sel ? C_ACCENT : C_DIM;
        uint16_t txt  = sel ? C_TEXT : col;

        if (mut) {
            fill = C_BG;
            txt = blend16(col, C_BG, 60);
        }

        draw_btn(NULL, &R_TRACKS[i], TRACK_LABELS[i], fill, brd, txt);

        // Mute indicator block
        uint16_t mFill = mut ? C_STOP : C_PANEL2;
        uint16_t mTxt = mut ? C_TEXT : C_DIM;
        draw_btn(NULL, &R_MUTES[i], "M", mFill, C_DIM, mTxt);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — TABS (y 204–239)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_tabs(void) {
    tft.fillRect(0, 204, 320, 36, C_BG);

    bool seq   = (ui.mode == UiMode::PATTERN_EDIT);
    bool sound = (ui.mode == UiMode::SOUND_EDIT);
    bool mix   = (ui.mode == UiMode::MIXER);

    draw_btn(NULL, &R_TAB_SEQ,   "SEQ",   seq   ? C_ACCENT : C_PANEL2, seq   ? C_ACCENT : C_DIM, seq   ? C_BG : C_TEXT);
    draw_btn(NULL, &R_TAB_SOUND, "SOUND", sound ? C_ACCENT : C_PANEL2, sound ? C_ACCENT : C_DIM, sound ? C_BG : C_TEXT);
    draw_btn(NULL, &R_TAB_MIX,   "MIX",   mix   ? C_ACCENT : C_PANEL2, mix   ? C_ACCENT : C_DIM, mix   ? C_BG : C_TEXT);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — PANEL (x 216–319, y 56–203)
   ═══════════════════════════════════════════════════════════════════════════════ */

struct ParamMeta { char label[12]; char value[12]; float norm; };

static int getParamDisplayValue(int paramIndex) {
    const int track = ui.snapshot.activeTrack;
    const bool isBass = track == VOICE_BASS;
    const VoiceParams &params = ui.snapshot.voiceParams[track];

    if (ui.mode == UiMode::SOUND_EDIT) {
        if (isBass) {
            switch (paramIndex) {
                case 0: return static_cast<int>(params.pitch * 100.0f);
                case 1: return static_cast<int>(params.decay * 100.0f);
                case 2: return static_cast<int>(params.timbre * 100.0f);
                default: return static_cast<int>(params.drive * 100.0f);
            }
        } else {
            switch (paramIndex) {
                case 0: return static_cast<int>(params.pitch * 100.0f);
                case 1: return static_cast<int>(params.decay * 100.0f);
                case 2: return static_cast<int>(params.timbre * 100.0f);
                default: return (track == VOICE_KICK) ? static_cast<int>(params.drive * 100.0f) : static_cast<int>(ui.snapshot.voiceGain[track] * 100.0f);
            }
        }
    } else {
        if (isBass) {
            switch (paramIndex) {
                case 0: return static_cast<int>(ui.snapshot.bassParams.density * 100.0f);
                case 1: return ui.snapshot.bassParams.range;
                case 2: return static_cast<int>(ui.snapshot.bassParams.scaleType);
                default: return ui.snapshot.bassParams.rootNote;
            }
        } else {
            switch (paramIndex) {
                case 0: return ui.snapshot.trackSteps[track];
                case 1: return ui.snapshot.trackHits[track];
                case 2: return ui.snapshot.trackRotations[track];
                default: return static_cast<int>(ui.snapshot.voiceGain[track] * 100.0f);
            }
        }
    }
}

static void param_meta(int row, ParamMeta *out) {
    out->label[0] = '\0';
    out->value[0] = '\0';
    out->norm = 0.0f;

    const int   tr    = ui.snapshot.activeTrack;
    const bool  bass  = (tr == VOICE_BASS);
    const VoiceParams &vp = ui.snapshot.voiceParams[tr];

    if (ui.mode == UiMode::SOUND_EDIT) {
        static const char * const PITCH_LBL[TRACK_COUNT] = {"PITCH","TUNE","FREQ","FREQ","PITCH"};
        static const char * const DECAY_LBL[TRACK_COUNT] = {"DECAY","SNAP","DECAY","RING","GLIDE"};
        static const char * const TIMB_LBL[TRACK_COUNT]  = {"PUNCH","TONE","OPEN","OPEN","TIMBR"};
        static const char * const DRIVE_LBL[TRACK_COUNT] = {"DRIVE","CRACK","LEVEL","LEVEL","DRIVE"};
        float vals[4] = {vp.pitch, vp.decay, vp.timbre, vp.drive};
        if (!bass && row == 3 && tr != VOICE_KICK) {
            vals[3] = ui.snapshot.voiceGain[tr];
        }
        const char *lbls[4] = {PITCH_LBL[tr], DECAY_LBL[tr], TIMB_LBL[tr], DRIVE_LBL[tr]};
        if (!bass && row == 3 && tr != VOICE_KICK) {
            lbls[3] = "LEVEL";
        }
        if (row < 4) {
            snprintf(out->label, sizeof(out->label), "%s", lbls[row]);
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = vals[row];
        }
        return;
    }

    if (bass) {
        static const char * const SCALE_NAMES[] = {"CHROM","MAJOR","MINOR","PENTA","BLUES"};
        static const char * const NOTE_NAMES[] = {"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
        const BassGrooveParams &bp = ui.snapshot.bassParams;
        switch (row) {
            case 0:
                snprintf(out->label, sizeof(out->label), "DENS");
                snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
                out->norm = bp.density; break;
            case 1:
                snprintf(out->label, sizeof(out->label), "RANGE");
                snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
                out->norm = (float)(bp.range - 1) / 11.0f; break;
            case 2: {
                snprintf(out->label, sizeof(out->label), "SCALE");
                int sc = constrain((int)bp.scaleType, 0, 4);
                snprintf(out->value, sizeof(out->value), "%s", SCALE_NAMES[sc]);
                out->norm = (float)sc / 4.0f; break;
            }
            case 3: {
                snprintf(out->label, sizeof(out->label), "ROOT");
                snprintf(out->value, sizeof(out->value), "%s%d",
                         NOTE_NAMES[bp.rootNote % 12], bp.rootNote / 12 - 1);
                out->norm = (float)(bp.rootNote - 24) / 36.0f; break;
            }
            default: break;
        }
        return;
    }

    /* Percussion — pattern params */
    int steps = ui.snapshot.trackSteps[tr];
    switch (row) {
        case 0:
            snprintf(out->label, sizeof(out->label), "STEPS");
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = (float)(steps - 4) / 60.0f; break;
        case 1:
            snprintf(out->label, sizeof(out->label), "HITS");
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = steps > 0
                ? (float)ui.snapshot.trackHits[tr] / steps : 0.0f; break;
        case 2:
            snprintf(out->label, sizeof(out->label), "SHIFT");
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = steps > 1
                ? (float)ui.snapshot.trackRotations[tr] / (steps - 1) : 0.0f; break;
        case 3:
            snprintf(out->label, sizeof(out->label), "LEVEL");
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = ui.snapshot.voiceGain[tr]; break;
        default: break;
    }
}

static void draw_panel(void) {
    s_panel.fillSprite(C_BG);
    uint16_t tc = TRACK_COLORS[ui.snapshot.activeTrack];

    for (int row = 0; row < 4; row++) {
        ParamMeta m;
        param_meta(row, &m);
        if (m.label[0] == '\0') continue;

        const ParamRow *pr = &PARAM_ROWS[row];

        // Card Background
        s_panel.fillRoundRect(pr->touch.x, pr->touch.y, pr->touch.w, pr->touch.h, 2, C_PANEL);
        s_panel.drawRoundRect(pr->touch.x, pr->touch.y, pr->touch.w, pr->touch.h, 2, C_DIM);

        // Label
        s_panel.setTextColor(C_TEXT, C_PANEL);
        s_panel.setTextDatum(MC_DATUM);
        s_panel.drawString(m.label, pr->touch.x + pr->touch.w / 2, pr->touch.y + 6);

        // Controls
        draw_btn(&s_panel, &pr->minus, "-", C_PANEL2, C_DIM, C_TEXT);
        draw_btn(&s_panel, &pr->value, m.value, C_BG, tc, C_TEXT);
        draw_btn(&s_panel, &pr->plus,  "+", C_PANEL2, C_DIM, C_TEXT);
    }

    s_panel.pushSprite(PANEL_SX, PANEL_SY);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — CENTER SEQ
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_center_seq(void) {
    s_center.fillSprite(C_BG);

    /* Guide circles */
    for (int t = 0; t < TRACK_COUNT; t++) {
        uint8_t a = (t == ui.snapshot.activeTrack) ? 80u : 20u;
        s_center.drawCircle(RING_CX, RING_CY, RING_RADII[t], blend16(TRACK_COLORS[t], C_BG, a));
    }

    /* Playhead spoke */
    if (ui.snapshot.isPlaying) {
        float ang = ((ui.snapshot.currentStep % 16) / 16.0f) * 2.0f * (float)M_PI - (float)(M_PI / 2.0);
        s_center.drawLine(RING_CX, RING_CY,
                        RING_CX + (int)(72 * cosf(ang)),
                        RING_CY + (int)(72 * sinf(ang)),
                        blend16(C_PLAY, C_BG, 40));
    }

    /* Step dots */
    for (int t = 0; t < TRACK_COUNT; t++) {
        int len = ui.snapshot.patternLens[t];
        if (len <= 0) continue;
        bool sel = (t == ui.snapshot.activeTrack);
        bool mut = ui.snapshot.trackMutes[t];
        uint16_t col = TRACK_COLORS[t];
        uint8_t  rad = RING_RADII[t];
        int      dr  = sel ? 3 : 2;

        for (int i = 0; i < len; i++) {
            float ang = (2.0f * (float)M_PI * i / len) - (float)(M_PI / 2.0);
            int16_t px = (int16_t)(RING_CX + rad * cosf(ang));
            int16_t py = (int16_t)(RING_CY + rad * sinf(ang));
            bool active   = ui.snapshot.patterns[t][i] > 0;
            bool playhead = ui.snapshot.isPlaying && (i == ui.snapshot.currentStep % len);

            if (mut) {
                if (active) s_center.drawCircle(px, py, dr - 1, blend16(col, C_BG, 40));
            } else if (playhead && active) {
                s_center.fillCircle(px, py, dr + 1, TFT_WHITE);
            } else if (playhead) {
                s_center.drawCircle(px, py, dr, col);
            } else if (active) {
                s_center.fillCircle(px, py, dr, sel ? col : blend16(col, C_BG, 120));
            } else if (sel) {
                s_center.drawCircle(px, py, 1, C_DIM);
            }
        }
    }

    /* Marker for step 1 */
    s_center.fillCircle(RING_CX, RING_CY - RING_RADII[0] - 4, 2, C_DIM);

    s_center.pushSprite(R_CENTER.x, R_CENTER.y);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — CENTER SOUND
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_center_sound(void) {
    s_center.fillSprite(C_BG);
    int tr = ui.snapshot.activeTrack;
    uint16_t tc = TRACK_COLORS[tr];

    // Track Name
    s_center.setTextColor(tc, C_BG);
    s_center.setTextDatum(MC_DATUM);
    s_center.setFont(&fonts::FreeSansBold12pt7b);
    s_center.drawString(TRACK_LABELS[tr], R_CENTER.w / 2, 24);
    s_center.setFont(&fonts::FreeMonoBold9pt7b);

    // Reactive "meter"
    int step = ui.snapshot.currentStep % ui.snapshot.patternLens[tr];
    bool isTriggering = ui.snapshot.isPlaying && ui.snapshot.patterns[tr][step] > 0;

    // Draw simple bars
    int numBars = 5;
    int barW = 12;
    int spacing = 18;
    int startX = (R_CENTER.w - (numBars * barW + (numBars - 1) * (spacing - barW))) / 2;
    int baseY = 110;

    for (int i = 0; i < numBars; i++) {
        int h = isTriggering ? 40 + (rand() % 40) : 10;
        if (i == 0 || i == 4) h = isTriggering ? 20 + (rand() % 20) : 6;

        uint16_t c = isTriggering ? tc : C_PANEL;
        s_center.fillRect(startX + i * spacing, baseY - h, barW, h, c);
    }

    if (tr == VOICE_BASS) {
        const BassGrooveParams &bp = ui.snapshot.bassParams;
        static const char * const SCALE_NAMES[] = {"CHROM","MAJOR","MINOR","PENTA","BLUES"};
        static const char * const NOTE_NAMES[] = {"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
        char buf[32];
        snprintf(buf, sizeof(buf), "%s%d %s", NOTE_NAMES[bp.rootNote % 12], bp.rootNote / 12 - 1, SCALE_NAMES[constrain((int)bp.scaleType, 0, 4)]);
        s_center.setTextColor(C_TEXT, C_BG);
        s_center.drawString(buf, R_CENTER.w / 2, 130);
    }

    s_center.pushSprite(R_CENTER.x, R_CENTER.y);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — CENTER MIX
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_center_mix(void) {
    // We don't use the s_center sprite here because it spans beyond R_CENTER width.
    // Instead we draw directly to TFT in the expanded area.
    tft.fillRect(R_CENTER.x, R_CENTER.y, 320 - R_CENTER.x, R_CENTER.h, C_BG);

    for (int i = 0; i < TRACK_COUNT; i++) {
        const Rect* r = &R_MIXER_SLIDERS[i];
        bool sel = (ui.snapshot.activeTrack == i);
        bool mut = ui.snapshot.trackMutes[i];
        uint16_t col = TRACK_COLORS[i];

        // Track Label
        tft.setTextColor(sel ? C_TEXT : col, C_BG);
        tft.setTextDatum(MC_DATUM);
        char lbl[2] = {TRACK_CHARS[i], '\0'};
        tft.drawString(lbl, r->x + r->w / 2, r->y - 10);

        // Fader Track
        tft.fillRoundRect(r->x, r->y, r->w, r->h, 2, C_PANEL);
        tft.drawRoundRect(r->x, r->y, r->w, r->h, 2, sel ? C_ACCENT : C_DIM);

        // Fader Fill
        int fillH = (int)(ui.snapshot.voiceGain[i] * (r->h - 4));
        uint16_t fillC = mut ? blend16(col, C_BG, 80) : col;
        tft.fillRoundRect(r->x + 2, r->y + r->h - 2 - fillH, r->w - 4, fillH, 2, fillC);

        // Fader Cap Line
        tft.drawFastHLine(r->x + 2, r->y + r->h - 2 - fillH, r->w - 4, C_TEXT);

        // Value
        char valBuf[8];
        snprintf(valBuf, sizeof(valBuf), "%d", (int)(ui.snapshot.voiceGain[i] * 100));
        tft.setTextColor(C_TEXT, C_BG);
        tft.drawString(valBuf, r->x + r->w / 2, r->y + r->h + 10);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
   HOLD-TO-ACCELERATE
   ═══════════════════════════════════════════════════════════════════════════════ */
static void hold_start(int paramIndex, int delta) {
    ui.activeHoldAction = delta; // delta is +1 or -1
    ui.activeHoldParam  = paramIndex;
    ui.holdTickCount  = 0;
    ui.holdNextTickMs   = millis() + 400;
}

static void hold_stop(void) {
    ui.activeHoldAction = 0;
    ui.activeHoldParam = -1;
    ui.holdTickCount  = 0;
}

static void fireParamAction(int paramIndex, int amount) {
  const int track = ui.snapshot.activeTrack;
  const bool isBass = track == VOICE_BASS;

  int displayValue = getParamDisplayValue(paramIndex);
  int newValue = displayValue + amount;

  if (ui.mode == UiMode::SOUND_EDIT) {
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
    postUiAction(UiActionType::SET_SOUND_PARAM, 3, newValue);
    break;
  }
}

static void hold_tick(void) {
    uint32_t now = millis();

    // Track mute hold logic
    if (ui.activeTrackHold >= 0 && ui.trackHoldStartMs > 0) {
        if (now - ui.trackHoldStartMs > 600) {
            postUiAction(UiActionType::TOGGLE_MUTE, 0, ui.activeTrackHold);
            ui.activeTrackHold = -1; // Fire once
            ui.trackHoldStartMs = 0;
        }
    }

    if (ui.activeHoldParam < 0 || ui.activeHoldAction == 0) return;
    if (now < ui.holdNextTickMs) return;
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
    
    ui.holdNextTickMs = now + hold_interval(ui.holdTickCount);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   TOUCH HANDLER
   ═══════════════════════════════════════════════════════════════════════════════ */
static void param_delta(int row, int delta) {
    fireParamAction(row, delta);
    hold_start(row, delta);
}

static void handleTouch(const TouchPoint &tp) {
    if (tp.justReleased) {
        hold_stop();
        ui.activeTrackHold = -1;
        ui.trackHoldStartMs = 0;
        return;
    }
    if (!tp.justPressed) return;

    int tx = tp.x, ty = tp.y;

    /* ── HEADER (y < 40) ───────────────────────────────────────────────────── */
    if (ty < 40) {
        if (R_PLAY.contains(tx, ty))     { postUiAction(UiActionType::TOGGLE_PLAY, 0, 0); return; }
        if (R_SLOT_DEC.contains(tx, ty)) { ui.activeSlot = (ui.activeSlot == 0) ? (CYDConfig::PatternSlots - 1) : (ui.activeSlot - 1); return; }
        if (R_SLOT_INC.contains(tx, ty)) { ui.activeSlot = (ui.activeSlot + 1) % CYDConfig::PatternSlots; return; }
        if (R_SAVE.contains(tx, ty))     {
            set_status(PatternStore.saveSlot(ui.activeSlot) ? "SAVED!" : "SAVE FAIL");
            return;
        }
        if (R_LOAD.contains(tx, ty))     {
            set_status(PatternStore.loadSlot(ui.activeSlot) ? "LOADED!" : "LOAD FAIL");
            return;
        }
        if (R_BPM_DEC.contains(tx, ty))  {
            postUiAction(UiActionType::NUDGE_BPM, 0, -1);
            hold_start(10, -1); return;
        }
        if (R_BPM_INC.contains(tx, ty))  {
            postUiAction(UiActionType::NUDGE_BPM, 0, +1);
            hold_start(10, +1); return;
        }
        return;
    }

    /* ── TRACKS (x < 68, 56 <= y < 204) ────────────────────────────────────── */
    if (tx < 68 && ty >= 56 && ty < 204) {
        for (int i = 0; i < TRACK_COUNT; i++) {
            if (R_MUTES[i].contains(tx, ty)) {
                postUiAction(UiActionType::TOGGLE_MUTE, 0, i); return;
            }
            if (R_TRACKS[i].contains(tx, ty)) {
                postUiAction(UiActionType::SELECT_TRACK, 0, i);
                ui.activeTrackHold = i;
                ui.trackHoldStartMs = millis();
                return;
            }
        }
        return;
    }

    /* ── TABS (y >= 204) ───────────────────────────────────────────────────── */
    if (ty >= 204) {
        if (R_TAB_SEQ.contains(tx, ty))   { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::PATTERN_EDIT); ui.mode = UiMode::PATTERN_EDIT; return; }
        if (R_TAB_SOUND.contains(tx, ty)) { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::SOUND_EDIT);   ui.mode = UiMode::SOUND_EDIT;   return; }
        if (R_TAB_MIX.contains(tx, ty))   { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::MIXER);        ui.mode = UiMode::MIXER;        return; }
        return;
    }

    /* ── MIXER SLIDERS (MIX mode, x >= 68) ─────────────────────────────────── */
    if (ui.mode == UiMode::MIXER && tx >= 68) {
        for (int i = 0; i < TRACK_COUNT; i++) {
            if (R_MIXER_SLIDERS[i].contains(tx, ty)) {
                float norm = 1.0f - (float)(ty - R_MIXER_SLIDERS[i].y) / R_MIXER_SLIDERS[i].h;
                norm = norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);
                postUiAction(UiActionType::SET_VOICE_GAIN, i, (int)(norm * 100));
                return;
            }
        }
        return;
    }

    /* ── PANEL (not MIX mode, x >= PANEL_SX) ───────────────────────────────── */
    if (ui.mode != UiMode::MIXER && tx >= PANEL_SX && ty >= 56 && ty < 204) {
        int lx = tx - PANEL_SX;
        int ly = ty - PANEL_SY;
        for (int row = 0; row < 4; row++) {
            const ParamRow *pr = &PARAM_ROWS[row];
            if (!pr->touch.contains(lx, ly)) continue;
            if (pr->minus.contains(lx, ly)) { param_delta(row, -1); return; }
            if (pr->plus.contains(lx, ly))  { param_delta(row, +1); return; }
            return;
        }
        return;
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
  InputMgr.begin();

  tft.fillScreen(C_BG);
  tft.setFont(&fonts::FreeMonoBold9pt7b);

  s_center.createSprite(R_CENTER.w, R_CENTER.h);
  s_center.setColorDepth(16);
  s_panel.createSprite(PANEL_W, PANEL_H);
  s_panel.setColorDepth(16);

  uint32_t last_full  = 0;
  uint32_t last_center = 0;

  for (;;) {
      uint32_t now = millis();

      ui.snapshot.capture();

      if (ui.statusUntilMs && now >= ui.statusUntilMs) {
          ui.statusUntilMs = 0;
          snprintf(ui.status, sizeof(ui.status), " ");
          ui.dirtyStatus = true;
      }

      ui.mode = ui.snapshot.mode;

      InputMgr.update();
      handleTouch(InputMgr.state());
      hold_tick();

      /* ── Decide what to redraw ─────────────────────────────────────────── */
      if (ui.snapshot.bpm != ui.lastSnapshot.bpm ||
          ui.snapshot.isPlaying != ui.lastSnapshot.isPlaying ||
          ui.activeSlot != ui.lastActiveSlot) {
          ui.dirtyHeader = true;
      }

      if (ui.mode != ui.lastMode) {
          ui.dirtyTabs = true;
          ui.dirtyCenter = true;
          ui.dirtyPanel = true;
      }

      if (ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack ||
          memcmp(ui.snapshot.trackMutes, ui.lastSnapshot.trackMutes, sizeof(ui.snapshot.trackMutes)) != 0) {
          ui.dirtyTracks = true;
          ui.dirtyCenter = true;
          ui.dirtyPanel = true;
      }

      bool sliders_dirty = ui.activeHoldParam != -1 ||
          memcmp(ui.snapshot.trackSteps, ui.lastSnapshot.trackSteps, sizeof(ui.snapshot.trackSteps)) != 0 ||
          memcmp(ui.snapshot.trackHits, ui.lastSnapshot.trackHits, sizeof(ui.snapshot.trackHits)) != 0 ||
          memcmp(ui.snapshot.trackRotations, ui.lastSnapshot.trackRotations, sizeof(ui.snapshot.trackRotations)) != 0 ||
          memcmp(ui.snapshot.voiceParams, ui.lastSnapshot.voiceParams, sizeof(ui.snapshot.voiceParams)) != 0 ||
          memcmp(ui.snapshot.voiceGain, ui.lastSnapshot.voiceGain, sizeof(ui.snapshot.voiceGain)) != 0 ||
          memcmp(&ui.snapshot.bassParams, &ui.lastSnapshot.bassParams, sizeof(BassGrooveParams)) != 0;

      if (sliders_dirty) {
          ui.dirtyPanel = true;
          if (ui.mode == UiMode::MIXER) ui.dirtyCenter = true;
      }

      bool step_change = (ui.snapshot.currentStep != ui.lastSnapshot.currentStep);
      bool pattern_change = memcmp(ui.snapshot.patterns, ui.lastSnapshot.patterns, sizeof(ui.snapshot.patterns)) != 0 ||
                            memcmp(ui.snapshot.trackSteps, ui.lastSnapshot.trackSteps, sizeof(ui.snapshot.trackSteps)) != 0;

      if (ui.mode == UiMode::PATTERN_EDIT && (step_change || pattern_change)) {
          ui.dirtyCenter = true;
      }

      if (ui.mode == UiMode::SOUND_EDIT && step_change) {
          ui.dirtyCenter = true;
      }

      bool full = ui.forceFull || (now - last_full > 8000u);

      tft.startWrite();

      if (full) {
          tft.fillScreen(C_BG);
          ui.dirtyHeader = true;
          ui.dirtyStatus = true;
          ui.dirtyTracks = true;
          ui.dirtyTabs = true;
          ui.dirtyCenter = true;
          ui.dirtyPanel = true;
          last_full = now;
          ui.forceFull = false;
      }

      if (ui.dirtyHeader) { draw_header(); ui.dirtyHeader = false; }
      if (ui.dirtyStatus) { draw_status(); ui.dirtyStatus = false; }
      if (ui.dirtyTracks) { draw_tracks(); ui.dirtyTracks = false; }
      if (ui.dirtyTabs)   { draw_tabs(); ui.dirtyTabs = false; }

      if (ui.mode != UiMode::MIXER && ui.dirtyPanel) {
          draw_panel();
          ui.dirtyPanel = false;
      } else if (ui.mode == UiMode::MIXER && ui.dirtyPanel) {
          tft.fillRect(PANEL_SX, PANEL_SY, PANEL_W, PANEL_H, C_BG);
          ui.dirtyPanel = false;
      }

      tft.endWrite();

      if (ui.dirtyCenter) {
          if (now - last_center >= 33u) {
              if (ui.mode == UiMode::PATTERN_EDIT) {
                  draw_center_seq();
              } else if (ui.mode == UiMode::SOUND_EDIT) {
                  draw_center_sound();
              } else if (ui.mode == UiMode::MIXER) {
                  draw_center_mix();
              }
              last_center = now;
              ui.dirtyCenter = false;
          }
      }

      ui.lastSnapshot = ui.snapshot;
      ui.lastMode     = ui.mode;
      ui.lastActiveSlot = ui.activeSlot;
      snprintf(ui.lastStatus, sizeof(ui.lastStatus), "%s", ui.status);

      vTaskDelay(pdMS_TO_TICKS(16));
  }
}
