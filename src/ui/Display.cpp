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

/* ── Sprites (ring + right panel) — eliminate flicker on animation ──────────── */
static LGFX_Sprite s_ring(&tft);   /* 140 × 140 — euclidean ring area  */
static LGFX_Sprite s_panel(&tft);  /* 132 × 168 — right parameter panel */

/* ═══════════════════════════════════════════════════════════════════════════════
   COLOUR PALETTE  (RGB565)
   ═══════════════════════════════════════════════════════════════════════════════ */
constexpr uint16_t C_BG       = 0x0863u;   /* deep navy-charcoal      */
constexpr uint16_t C_PANEL    = 0x1947u;   /* dark slate-blue         */
constexpr uint16_t C_PANEL2   = 0x2104u;   /* inactive button face    */
constexpr uint16_t C_DIM      = 0x4228u;   /* outline / subdued       */
constexpr uint16_t C_TEXT     = 0xBDF8u;   /* normal text             */
constexpr uint16_t C_ACCENT   = 0xFEC9u;   /* important / active      */
constexpr uint16_t C_SELECT   = 0x4F1Fu;   /* selected mode highlight */
constexpr uint16_t C_EXPR     = 0xFA74u;   /* expressive / mauve      */
constexpr uint16_t C_PLAY     = 0x7FEEu;   /* positive / running      */

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

/* Transport bar */
static const Rect R_PLAY      = { 4,   8,  62,  30};
static const Rect R_SLOT_DEC  = {80,   8,  20,  30};
static const Rect R_SLOT_VAL  = {104,  8,  28,  30};
static const Rect R_SLOT_INC  = {136,  8,  20,  30};
static const Rect R_SAVE      = {160,  8,  24,  14};
static const Rect R_LOAD      = {160, 24,  24,  14};
static const Rect R_BPM_DEC   = {196,  8,  28,  30};
static const Rect R_BPM_VAL   = {228,  8,  52,  30};
static const Rect R_BPM_INC   = {284,  8,  28,  30};

/* Track selector — 5 buttons, left margin */
static const Rect R_TRACKS[TRACK_COUNT] = {
    { 4,  48, 28, 34},
    { 4,  86, 28, 34},
    { 4, 124, 28, 34},
    { 4, 162, 28, 34},
    { 4, 200, 28, 34},
};

/* Ring area (sprite origin) */
static const Rect R_RING = {36, 52, 140, 140};
#define RING_CX 70   /* sprite-relative centre */
#define RING_CY 70
static const uint8_t RING_RADII[TRACK_COUNT] = {58, 48, 38, 28, 18};

/* Bottom action row */
static const Rect R_MUTE  = { 36, 208, 52, 26};
static const Rect R_VOICE = { 88, 208, 52, 26};
static const Rect R_MIX   = {140, 208, 52, 26};

/* Right panel (sprite origin x=188, y=44) */
#define PANEL_SX 188
#define PANEL_SY  44
#define PANEL_W  132
#define PANEL_H  168

/* 4 param rows inside the panel sprite */
struct ParamRow {
    Rect minus;
    Rect value;
    Rect plus;
    Rect touch;  /* full row, for hit-testing */
};

static const ParamRow PARAM_ROWS[4] = {
    {{ 8, 18, 28, 26}, {40, 18, 52, 26}, { 96, 18, 28, 26}, {0,  4, PANEL_W, 42}},
    {{ 8, 68, 28, 26}, {40, 68, 52, 26}, { 96, 68, 28, 26}, {0, 54, PANEL_W, 42}},
    {{ 8,118, 28, 26}, {40,118, 52, 26}, { 96,118, 28, 26}, {0,104, PANEL_W, 42}},
    {{ 8,142, 28, 26}, {40,142, 52, 26}, { 96,142, 28, 26}, {0,130, PANEL_W, 38}},
};

/* Mixer sliders (absolute coords) */
static const Rect R_SLIDERS[TRACK_COUNT] = {
    {201, 54, 12, 126},
    {224, 54, 12, 126},
    {247, 54, 12, 126},
    {270, 54, 12, 126},
    {293, 54, 12, 126},
};

/* ═══════════════════════════════════════════════════════════════════════════════
   RUNTIME STATE  (display-local — never touches audio engine directly)
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

  char status[32] = "CYD ready";
  char lastStatus[32] = "";
  uint32_t statusUntilMs = 0;
  bool forceRedraw = true;
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
        spr->fillRoundRect(r->x, r->y, r->w, r->h, 3, fill);
        spr->drawRoundRect(r->x, r->y, r->w, r->h, 3, outline);
        spr->setTextColor(text_col, fill);
        spr->setTextDatum(MC_DATUM);
        spr->drawString(label, r->x + r->w / 2, r->y + r->h / 2 - 1);
    } else {
        tft.fillRoundRect(r->x, r->y, r->w, r->h, 3, fill);
        tft.drawRoundRect(r->x, r->y, r->w, r->h, 3, outline);
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

static void set_status(const char *msg, uint32_t ms = 1400) {
    snprintf(ui.status, sizeof(ui.status), "%s", msg);
    ui.statusUntilMs = millis() + ms;
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — TRANSPORT BAR  (y 0–42)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_transport(void) {
    char buf[8];
    tft.fillRect(0, 0, CYDConfig::ScreenWidth, 42, C_BG);

    /* Play / Stop */
    bool pl = ui.snapshot.isPlaying;
    tft.fillRect(R_PLAY.x, R_PLAY.y, R_PLAY.w, R_PLAY.h, pl ? C_ACCENT : C_PLAY);
    tft.drawRect(R_PLAY.x, R_PLAY.y, R_PLAY.w, R_PLAY.h, C_DIM);
    if (pl) {
        tft.fillRect(R_PLAY.x + 20, R_PLAY.y + 7,  10, 16, C_BG);
        tft.fillRect(R_PLAY.x + 36, R_PLAY.y + 7,  10, 16, C_BG);
    } else {
        tft.fillTriangle(R_PLAY.x + 18, R_PLAY.y + 5,
                         R_PLAY.x + 18, R_PLAY.y + 25,
                         R_PLAY.x + 44, R_PLAY.y + 15, C_BG);
    }

    /* Pattern slot */
    draw_btn(NULL, &R_SLOT_DEC, "<", C_PANEL, C_DIM, C_TEXT);
    snprintf(buf, sizeof(buf), "%d", ui.activeSlot + 1);
    draw_btn(NULL, &R_SLOT_VAL, buf, C_PANEL2, C_DIM, C_TEXT);
    draw_btn(NULL, &R_SLOT_INC, ">", C_PANEL, C_DIM, C_TEXT);

    /* Save / Load */
    draw_btn(NULL, &R_SAVE, "S", C_PANEL2, C_DIM, C_TEXT);
    draw_btn(NULL, &R_LOAD, "L", C_PANEL2, C_DIM, C_TEXT);

    /* BPM */
    draw_btn(NULL, &R_BPM_DEC, "-", C_PANEL, C_DIM, C_TEXT);
    snprintf(buf, sizeof(buf), "%3d", ui.snapshot.bpm);
    draw_btn(NULL, &R_BPM_VAL, buf, C_PANEL2, C_DIM, C_ACCENT);
    draw_btn(NULL, &R_BPM_INC, "+", C_PANEL, C_DIM, C_TEXT);

    /* Status line (thin bar below transport) */
    uint16_t bar_col = (ui.statusUntilMs > millis()) ? C_ACCENT : C_DIM;
    tft.drawFastHLine(0, 41, CYDConfig::ScreenWidth, bar_col);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — STATUS TEXT (overlaid in BPM value box)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_status(void) {
    bool active = (ui.statusUntilMs > millis());
    const char *msg = active ? ui.status
                     : ui.snapshot.isPlaying
                         ? "PLAYING"
                         : "READY";
    draw_btn(NULL, &R_BPM_VAL, msg, C_PANEL2, active ? C_ACCENT : C_DIM,
             active ? (uint16_t)C_PLAY : (uint16_t)C_TEXT);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — TRACK SELECTOR (left margin)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_tracks(void) {
    char lbl[2] = {0, 0};
    for (int i = 0; i < TRACK_COUNT; i++) {
        bool sel = (ui.snapshot.activeTrack == i);
        bool mut = ui.snapshot.trackMutes[i];
        uint16_t col  = TRACK_COLORS[i];
        uint16_t fill = sel ? blend16(col, C_BG, 55) : (mut ? C_PANEL2 : C_PANEL);
        uint16_t brd  = mut ? blend16(col, C_BG, 50) : col;
        uint16_t txt  = sel ? C_BG : (mut ? blend16(col, C_BG, 60) : col);
        lbl[0] = TRACK_CHARS[i];
        draw_btn(NULL, &R_TRACKS[i], lbl, fill, brd, txt);
        if (mut && !sel) {
            tft.drawLine(R_TRACKS[i].x + 2, R_TRACKS[i].y + 2,
                         R_TRACKS[i].x + R_TRACKS[i].w - 3,
                         R_TRACKS[i].y + R_TRACKS[i].h - 3, C_DIM);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — EUCLIDEAN RING (via sprite — atomic push, zero flicker)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_ring(void) {
    s_ring.fillSprite(C_BG);

    /* Guide circles */
    for (int t = 0; t < TRACK_COUNT; t++) {
        uint8_t a = (t == ui.snapshot.activeTrack) ? 55u : 18u;
        s_ring.drawCircle(RING_CX, RING_CY, RING_RADII[t],
                          blend16(TRACK_COLORS[t], C_BG, a));
    }

    /* Playhead spoke */
    if (ui.snapshot.isPlaying) {
        float ang = ((ui.snapshot.currentStep % 16) / 16.0f) * 2.0f * (float)M_PI - (float)(M_PI / 2.0);
        s_ring.drawLine(RING_CX, RING_CY,
                        RING_CX + (int)(66 * cosf(ang)),
                        RING_CY + (int)(66 * sinf(ang)),
                        blend16(C_PLAY, C_BG, 18));
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
                if (active) s_ring.drawCircle(px, py, dr - 1,
                                blend16(col, C_BG, 25));
            } else if (playhead && active) {
                s_ring.fillCircle(px, py, dr + 1, TFT_WHITE);
            } else if (playhead) {
                s_ring.drawCircle(px, py, dr, col);
            } else if (active) {
                s_ring.fillCircle(px, py, dr,
                    sel ? col : blend16(col, C_BG, 80));
            } else if (sel) {
                s_ring.drawCircle(px, py, 1, C_DIM);
            }
        }
    }

    /* Centre pulse */
    int pr = ui.snapshot.isPlaying ? ((ui.snapshot.currentStep % 4 == 0) ? 5 : 3) : 3;
    s_ring.fillCircle(RING_CX, RING_CY, pr, ui.snapshot.isPlaying ? C_PLAY : C_PANEL2);
    s_ring.fillCircle(RING_CX, RING_CY, 1,  ui.snapshot.isPlaying ? C_BG   : C_DIM);

    s_ring.pushSprite(R_RING.x, R_RING.y);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — BOTTOM ACTION ROW (Mute / Voice / Mix)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_footer(void) {
    bool mut   = ui.snapshot.trackMutes[ui.snapshot.activeTrack];
    bool voice = (ui.mode == UiMode::SOUND_EDIT);
    bool mix   = (ui.mode == UiMode::MIXER);

    draw_btn(NULL, &R_MUTE,  mut   ? "MUTED" : "MUTE",
             mut   ? C_ACCENT : C_PANEL2, C_DIM, C_TEXT);
    draw_btn(NULL, &R_VOICE, "VOICE",
             voice ? C_SELECT : C_PANEL2, C_DIM, voice ? (uint16_t)C_BG : (uint16_t)C_TEXT);
    draw_btn(NULL, &R_MIX,   "MIX",
             mix   ? C_EXPR   : C_PANEL2, C_DIM, C_TEXT);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — PARAMETER PANEL (right side, via sprite)
   ═══════════════════════════════════════════════════════════════════════════════ */

/* Fill label + value strings for each param row */
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
        static const char * const PITCH_LBL[TRACK_COUNT] =
            {"Pitch","Tune","Freq","Freq","Pitch"};
        static const char * const DECAY_LBL[TRACK_COUNT] =
            {"Decay","Snap","Decay","Ring","Glide"};
        static const char * const TIMB_LBL[TRACK_COUNT] =
            {"Punch","Tone","Open","Open","Timbre"};
        static const char * const DRIVE_LBL[TRACK_COUNT] =
            {"Drive","Crack","Level","Level","Level"};
        float vals[4] = {vp.pitch, vp.decay, vp.timbre, vp.drive};
        if (!bass && row == 3 && tr != VOICE_KICK) {
            vals[3] = ui.snapshot.voiceGain[tr];
        }
        const char *lbls[4] = {PITCH_LBL[tr], DECAY_LBL[tr],
                                TIMB_LBL[tr], DRIVE_LBL[tr]};
        if (!bass && row == 3 && tr != VOICE_KICK) {
            lbls[3] = "Level";
        }
        if (row < 4) {
            snprintf(out->label, sizeof(out->label), "%s", lbls[row]);
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = vals[row];
        }
        return;
    }

    if (bass) {
        static const char * const SCALE_NAMES[] =
            {"Chrom","Major","Minor","Penta","Blues"};
        static const char * const NOTE_NAMES[] =
            {"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
        const BassGrooveParams &bp = ui.snapshot.bassParams;
        switch (row) {
            case 0:
                snprintf(out->label, sizeof(out->label), "Density");
                snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
                out->norm = bp.density; break;
            case 1:
                snprintf(out->label, sizeof(out->label), "Range");
                snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
                out->norm = (float)(bp.range - 1) / 11.0f; break;
            case 2: {
                snprintf(out->label, sizeof(out->label), "Scale");
                int sc = constrain((int)bp.scaleType, 0, 4);
                snprintf(out->value, sizeof(out->value), "%s", SCALE_NAMES[sc]);
                out->norm = (float)sc / 4.0f; break;
            }
            case 3: {
                snprintf(out->label, sizeof(out->label), "Root");
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
            snprintf(out->label, sizeof(out->label), "Steps");
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = (float)(steps - 4) / 60.0f; break;
        case 1:
            snprintf(out->label, sizeof(out->label), "Hits");
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = steps > 0
                ? (float)ui.snapshot.trackHits[tr] / steps : 0.0f; break;
        case 2:
            snprintf(out->label, sizeof(out->label), "Rot");
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = steps > 1
                ? (float)ui.snapshot.trackRotations[tr] / (steps - 1) : 0.0f; break;
        case 3:
            snprintf(out->label, sizeof(out->label), "Vol");
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = ui.snapshot.voiceGain[tr]; break;
        default: break;
    }
}

static void draw_params(void) {
    uint16_t tc = TRACK_COLORS[ui.snapshot.activeTrack];

    s_panel.fillSprite(C_BG);

    /* Track name header */
    s_panel.setTextColor(tc, C_BG);
    s_panel.setTextDatum(MC_DATUM);
    s_panel.drawString(TRACK_LABELS[ui.snapshot.activeTrack], PANEL_W / 2, 8);
    s_panel.drawFastHLine(0, 15, PANEL_W, C_DIM);

    if (ui.mode == UiMode::MIXER) {
        /* 5 vertical sliders */
        int sh = PANEL_H - 22;
        int sw = (PANEL_W - 10) / TRACK_COUNT;
        for (int i = 0; i < TRACK_COUNT; i++) {
            int sx = 5 + i * sw, sy = 20;
            int filled = (int)(ui.snapshot.voiceGain[i] * sh);
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
            param_meta(row, &m);
            if (m.label[0] == '\0') continue;

            const ParamRow *pr = &PARAM_ROWS[row];

            /* Mini progress bar */
            int bw = (int)(m.norm * (float)(pr->value.w - 4));
            s_panel.fillRect(pr->value.x + 2, pr->value.y - 5, pr->value.w - 4, 3, C_PANEL);
            if (bw > 0)
                s_panel.fillRect(pr->value.x + 2, pr->value.y - 5, bw, 3, tc);

            /* Label */
            s_panel.setTextColor(C_DIM, C_BG);
            s_panel.setTextDatum(TL_DATUM);
            s_panel.drawString(m.label, pr->minus.x, pr->touch.y + 2);

            /* − value + */
            draw_btn(&s_panel, &pr->minus, "-", C_PANEL,  C_DIM, C_TEXT);
            draw_btn(&s_panel, &pr->value, m.value, C_PANEL2, tc,    C_TEXT);
            draw_btn(&s_panel, &pr->plus,  "+", C_PANEL,  C_DIM, C_TEXT);
        }
    }

    s_panel.pushSprite(PANEL_SX, PANEL_SY);
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
    // Volume: adjust voice gain via SET_SOUND_PARAM index 3
    postUiAction(UiActionType::SET_SOUND_PARAM, 3, newValue);
    break;
  }
}

static void hold_tick(void) {
    if (ui.activeHoldParam < 0 || ui.activeHoldAction == 0) return;
    uint32_t now = millis();
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
    
    // Accelerate the ticking rate down to 40ms max speed
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
    static int activeFaderIndex = -1;
    constexpr int16_t sliderHitboxPadX = 8;
    constexpr int16_t sliderHitboxPadY = 10;
    const auto sliderHitRect = [&](int index) -> Rect {
        const Rect &r = R_SLIDERS[index];
        return {
            (int16_t)(r.x - sliderHitboxPadX),
            (int16_t)(r.y - sliderHitboxPadY),
            (int16_t)(r.w + (sliderHitboxPadX * 2)),
            (int16_t)(r.h + (sliderHitboxPadY * 2))
        };
    };
    const auto sliderGainFromY = [&](int index, int touchY) -> int {
        const Rect &r = R_SLIDERS[index];
        float norm = 1.0f - (float)(touchY - r.y) / r.h;
        norm = norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);
        return (int)(norm * 100.0f);
    };

    if (tp.justReleased) {
        hold_stop();
        activeFaderIndex = -1;
        return;
    }

    if (ui.mode == UiMode::MIXER && tp.pressed && activeFaderIndex >= 0) {
        postUiAction(UiActionType::SET_VOICE_GAIN, activeFaderIndex,
                     sliderGainFromY(activeFaderIndex, tp.y));
        return;
    }

    if (!tp.justPressed) return;

    int tx = tp.x, ty = tp.y;

    /* ── Transport (y < 42) ────────────────────────────────────────────────── */
    if (ty < 42) {
        if (R_PLAY.contains(tx, ty))  { postUiAction(UiActionType::TOGGLE_PLAY, 0, 0); return; }
        if (R_SLOT_DEC.contains(tx, ty)) { ui.activeSlot = (ui.activeSlot == 0) ? (CYDConfig::PatternSlots - 1) : (ui.activeSlot - 1); return; }
        if (R_SLOT_INC.contains(tx, ty)) { ui.activeSlot = (ui.activeSlot + 1) % CYDConfig::PatternSlots; return; }
        if (R_SAVE.contains(tx, ty))  {
            set_status(PatternStore.saveSlot(ui.activeSlot) ? "Saved!" : "Save Fail");
            return;
        }
        if (R_LOAD.contains(tx, ty))  {
            set_status(PatternStore.loadSlot(ui.activeSlot) ? "Loaded!" : "Load Fail");
            return;
        }
        if (R_BPM_DEC.contains(tx, ty)) {
            postUiAction(UiActionType::NUDGE_BPM, 0, -1);
            hold_start(10, -1); return;
        }
        if (R_BPM_INC.contains(tx, ty)) {
            postUiAction(UiActionType::NUDGE_BPM, 0, +1);
            hold_start(10, +1); return;
        }
        return;
    }

    /* ── Track selector (x < 36) ───────────────────────────────────────────── */
    if (tx < 36) {
        for (int i = 0; i < TRACK_COUNT; i++) {
            if (R_TRACKS[i].contains(tx, ty)) {
                postUiAction(UiActionType::SELECT_TRACK, 0, i); return;
            }
        }
        return;
    }

    /* ── Footer (y >= 208, x < 192) ────────────────────────────────────────── */
    if (ty >= 208 && tx < 192) {
        if (R_MUTE.contains(tx, ty)) { postUiAction(UiActionType::TOGGLE_MUTE, 0, ui.snapshot.activeTrack); return; }
        if (R_VOICE.contains(tx, ty)) { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::SOUND_EDIT);  ui.mode = UiMode::SOUND_EDIT;  return; }
        if (R_MIX.contains(tx, ty)) { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::MIXER);       ui.mode = UiMode::MIXER;        return; }
        return;
    }

    /* ── Ring Area (Tapping resets to pattern edit) ────────────────────────── */
    if (R_RING.contains(tx, ty) && ui.mode != UiMode::PATTERN_EDIT) {
        postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::PATTERN_EDIT);
        ui.mode = UiMode::PATTERN_EDIT;
        return;
    }

    /* ── Mixer sliders (x >= 192, mixer mode) ──────────────────────────────── */
    if (ui.mode == UiMode::MIXER && tx >= 192) {
        for (int i = 0; i < TRACK_COUNT; i++) {
            if (sliderHitRect(i).contains(tx, ty)) {
                activeFaderIndex = i;
                postUiAction(UiActionType::SET_VOICE_GAIN, i, sliderGainFromY(i, ty));
                return;
            }
        }
        activeFaderIndex = -1;
        return;
    }

    /* ── Param panel (x >= PANEL_SX, not mixer) ────────────────────────────── */
    if (tx >= PANEL_SX) {
        /* Convert to sprite-local coords */
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
  
  // Initialize dedicated touch SPI *after* TFT to prevent TFT_eSPI
  // from redefining or conflicting with the standard SPI host.
  InputMgr.begin();

  tft.fillScreen(C_BG);
  tft.setFont(&fonts::FreeMonoBold9pt7b); // Bold mono for legibility at small sizes

  /* ── Create sprites ────────────────────────────────────────────────────── */
  s_ring.createSprite(R_RING.w, R_RING.h);
  s_ring.setColorDepth(16);
  s_panel.createSprite(PANEL_W, PANEL_H);
  s_panel.setColorDepth(16);

  uint32_t last_full  = 0;
  uint32_t last_ring  = 0;

  for (;;) {
      uint32_t now = millis();

      /* Capture state snapshot */
      ui.snapshot.capture();

      /* Status timeout */
      if (ui.statusUntilMs && now >= ui.statusUntilMs) {
          ui.statusUntilMs = 0;
          snprintf(ui.status, sizeof(ui.status), "%s", ui.snapshot.isPlaying ? "PLAYING" : "READY");
          ui.forceRedraw = true; // force the full transport bar to redraw so the BPM value isn't overwritten
      }

      /* Mode sync from engine snapshot */
      ui.mode = ui.snapshot.mode;

      /* Touch */
      InputMgr.update();
      handleTouch(InputMgr.state());
      hold_tick();

      /* ── Decide what to redraw ─────────────────────────────────────────── */
      bool sliders_dirty = ui.activeHoldParam != -1 ||
          memcmp(ui.snapshot.trackSteps, ui.lastSnapshot.trackSteps, sizeof(ui.snapshot.trackSteps)) != 0 ||
          memcmp(ui.snapshot.trackHits, ui.lastSnapshot.trackHits, sizeof(ui.snapshot.trackHits)) != 0 ||
          memcmp(ui.snapshot.trackRotations, ui.lastSnapshot.trackRotations, sizeof(ui.snapshot.trackRotations)) != 0 ||
          memcmp(ui.snapshot.voiceParams, ui.lastSnapshot.voiceParams, sizeof(ui.snapshot.voiceParams)) != 0 ||
          memcmp(ui.snapshot.voiceGain, ui.lastSnapshot.voiceGain, sizeof(ui.snapshot.voiceGain)) != 0 ||
          memcmp(&ui.snapshot.bassParams, &ui.lastSnapshot.bassParams, sizeof(BassGrooveParams)) != 0;

      bool any_change = sliders_dirty ||
          (ui.snapshot.bpm         != ui.lastSnapshot.bpm)          ||
          (ui.snapshot.isPlaying   != ui.lastSnapshot.isPlaying)     ||
          (ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack)   ||
          (ui.mode                 != ui.lastMode)          ||
          (ui.activeSlot           != ui.lastActiveSlot)          ||
          (strcmp(ui.status, ui.lastStatus) != 0)       ||
          memcmp(ui.snapshot.trackMutes, ui.lastSnapshot.trackMutes,
                 sizeof(ui.snapshot.trackMutes)) != 0;

      bool pattern_change =
          memcmp(ui.snapshot.patterns, ui.lastSnapshot.patterns,    sizeof(ui.snapshot.patterns)) != 0  ||
          memcmp(ui.snapshot.trackSteps, ui.lastSnapshot.trackSteps,  sizeof(ui.snapshot.trackSteps)) != 0||
          ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack;

      bool step_change = (ui.snapshot.currentStep != ui.lastSnapshot.currentStep);

      /* Periodic full redraw to clear any artefacts */
      bool full = ui.forceRedraw || (now - last_full > 8000u);

      tft.startWrite();

      if (full) {
          tft.fillScreen(C_BG);
          draw_transport();
          draw_tracks();
          draw_footer();
          draw_params();
          last_full = now;
          ui.forceRedraw = false;
      } else if (any_change) {
          bool transport_dirty = (ui.snapshot.bpm != ui.lastSnapshot.bpm ||
              ui.snapshot.isPlaying != ui.lastSnapshot.isPlaying ||
              ui.activeSlot != ui.lastActiveSlot);

          if (transport_dirty) {
              draw_transport();
          } else if (strcmp(ui.status, ui.lastStatus) != 0) {
              // Redraw status only when status text actually changes
              draw_status();
          }

          if (ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack ||
              memcmp(ui.snapshot.trackMutes, ui.lastSnapshot.trackMutes,
                     sizeof(ui.snapshot.trackMutes)) != 0)
              draw_tracks();

          if (any_change) draw_footer();
          if (any_change) draw_params();
      }

      tft.endWrite();

      /* Ring: redraw every step tick or if pattern changed */
      if ((ui.snapshot.isPlaying && step_change) || pattern_change || full) {
          if (now - last_ring >= 33u) {   /* cap ~30 fps for ring */
              draw_ring();
              last_ring = now;
          }
      }

      /* Save snapshot */
      ui.lastSnapshot = ui.snapshot;
      ui.lastMode     = ui.mode;
      ui.lastActiveSlot = ui.activeSlot;
      snprintf(ui.lastStatus, sizeof(ui.lastStatus), "%s", ui.status);

      vTaskDelay(pdMS_TO_TICKS(16));   /* ~60 fps budget */
  }
}
