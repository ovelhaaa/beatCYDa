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
static LGFX_Sprite s_ring(&tft);   /* 50 × 50 — euclidean ring area  */
static LGFX_Sprite s_panel(&tft);  /* 258 × 174 — tab content panel */

/* ═══════════════════════════════════════════════════════════════════════════════
   COLOUR PALETTE  (RGB565)
   ═══════════════════════════════════════════════════════════════════════════════ */
#define RGB565(r, g, b) (uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

constexpr uint16_t C_BG       = RGB565(0x08, 0x0A, 0x0C);
constexpr uint16_t C_PANEL_BG = RGB565(0x06, 0x07, 0x08);
constexpr uint16_t C_DIV      = RGB565(0x1A, 0x1A, 0x1A);
constexpr uint16_t C_BORDER   = RGB565(0x2A, 0x2A, 0x2A);

constexpr uint16_t C_ACCENT   = RGB565(0xFF, 0x6A, 0x00);
constexpr uint16_t C_ACCENT_L = RGB565(0xFF, 0x8C, 0x40);

constexpr uint16_t C_TXT_ACT  = RGB565(0xFF, 0x8C, 0x40);
constexpr uint16_t C_TXT_SEC  = RGB565(0x55, 0x66, 0x77);
constexpr uint16_t C_TXT_DIM  = RGB565(0x2A, 0x33, 0x44);
constexpr uint16_t C_TXT_STAT = RGB565(0x44, 0x55, 0x66);

constexpr uint16_t C_FILL_BTN = RGB565(0x0A, 0x0B, 0x0D);
constexpr uint16_t C_FILL_TAB = RGB565(0x0D, 0x0E, 0x10);
constexpr uint16_t C_BORDER_DIM=RGB565(0x1E, 0x20, 0x22);


/* Legacy aliases for old UI code, to be removed later */
constexpr uint16_t C_PANEL    = C_PANEL_BG;
constexpr uint16_t C_PANEL2   = C_FILL_BTN;
constexpr uint16_t C_DIM      = C_TXT_DIM;
constexpr uint16_t C_TEXT     = C_TXT_SEC;
constexpr uint16_t C_SELECT   = C_ACCENT;
constexpr uint16_t C_EXPR     = C_ACCENT;
constexpr uint16_t C_PLAY     = C_TXT_STAT;

static const uint16_t TRACK_COLORS[TRACK_COUNT] = {
    RGB565(0xFF, 0x6A, 0x00),   /* K - Kick */
    RGB565(0x00, 0xCC, 0x66),   /* S - Snare */
    RGB565(0x00, 0xCC, 0xFF),   /* C - Clap */
    RGB565(0xCC, 0x44, 0xFF),   /* O - OpenHH */
    RGB565(0xFF, 0xCC, 0x00),   /* B - Bass */
};

static const char TRACK_CHARS[TRACK_COUNT] = {'K','S','C','O','B'};
static const char * const TRACK_LABELS[TRACK_COUNT] = {"KICK","SNRE","CLAP","HATO","BASS"};




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

/* ── Topbar (0,0 -> 320,30) ────────────────────────────────────────────────── */
static const Rect R_PLAY      = { 6,   5,  32,  20};
static const Rect R_BPM_DEC   = { 46,  8,  14,  14};
static const Rect R_BPM_VAL   = { 62,  8,  30,  14};
static const Rect R_BPM_INC   = { 94,  8,  14,  14};
static const Rect R_SLOT_DEC  = {80,   8,  20,  30};
static const Rect R_SLOT_VAL  = {104,  8,  28,  30};
static const Rect R_SLOT_INC  = {136,  8,  20,  30};
static const Rect R_SLOT_1    = {116,  6,  18,  18};
static const Rect R_SLOT_2    = {136,  6,  18,  18};
static const Rect R_SLOT_3    = {156,  6,  18,  18};
static const Rect R_SAVE      = {268,  6,  20,  18};
static const Rect R_LOAD      = {290,  6,  20,  18};

static const Rect R_BPM_GROUP = {43, 5, 68, 20};

/* ── Left Panel (0,30 -> 62,240) ───────────────────────────────────────────── */
static const Rect R_TRACKS[TRACK_COUNT] = {
    { 6,  92,  50, 18},
    { 6, 113,  50, 18},
    { 6, 134,  50, 18},
    { 6, 155,  50, 18},
    { 6, 176,  50, 18},
};

static const Rect R_RING = {6, 42, 50, 50};
#define RING_CX 25
#define RING_CY 25
static const uint8_t RING_RADII[4] = {23, 17, 11, 6}; // No bass ring

/* ── Tabs (62,30 -> 320,50) ────────────────────────────────────────────────── */
static const Rect R_TAB_SEQ = { 62, 30, 86, 20};
static const Rect R_TAB_DSP = {148, 30, 86, 20};
static const Rect R_TAB_MIX = {234, 30, 86, 20};

/* ── Footer (62,224 -> 320,240) ────────────────────────────────────────────── */
static const Rect R_FOOTER = {62, 224, 258, 16};
/* Bottom action row */
static const Rect R_MUTE  = { 36, 208, 52, 26};
static const Rect R_VOICE = { 88, 208, 52, 26};
static const Rect R_MIX   = {140, 208, 52, 26};



#define PANEL_SX 62
#define PANEL_SY 50
#define PANEL_W  258
#define PANEL_H  174



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
    strncpy(ui.status, msg, sizeof(ui.status) - 1);
    ui.status[sizeof(ui.status) - 1] = '\0';
    ui.statusUntilMs = millis() + ms;
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — TRANSPORT BAR  (y 0–42)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_transport(void) {
    char buf[8];
    tft.fillRect(0, 0, CYDConfig::ScreenWidth, 30, C_BG);

    /* Play / Stop */
    bool pl = ui.snapshot.isPlaying;
    tft.fillRoundRect(R_PLAY.x, R_PLAY.y, R_PLAY.w, R_PLAY.h, 2, pl ? blend16(C_ACCENT, C_BG, 24) : C_FILL_BTN);
    tft.drawRoundRect(R_PLAY.x, R_PLAY.y, R_PLAY.w, R_PLAY.h, 2, pl ? C_ACCENT : C_BORDER);

    tft.setTextColor(pl ? C_ACCENT_L : C_TXT_STAT, pl ? blend16(C_ACCENT, C_BG, 24) : C_FILL_BTN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(pl ? "X" : ">", R_PLAY.x + R_PLAY.w / 2, R_PLAY.y + R_PLAY.h / 2 - 1);

    /* BPM Group */
    tft.fillRoundRect(R_BPM_GROUP.x, R_BPM_GROUP.y, R_BPM_GROUP.w, R_BPM_GROUP.h, 2, blend16(C_ACCENT, C_BG, 16));
    tft.drawRoundRect(R_BPM_GROUP.x, R_BPM_GROUP.y, R_BPM_GROUP.w, R_BPM_GROUP.h, 2, blend16(C_ACCENT, C_BG, 34));

    tft.setTextColor(blend16(C_ACCENT_L, C_BG, 120), blend16(C_ACCENT, C_BG, 16));
    tft.drawString("-", R_BPM_DEC.x + R_BPM_DEC.w / 2, R_BPM_DEC.y + R_BPM_DEC.h / 2 - 1);
    tft.drawString("+", R_BPM_INC.x + R_BPM_INC.w / 2, R_BPM_INC.y + R_BPM_INC.h / 2 - 1);

    snprintf(buf, sizeof(buf), "%3d", ui.snapshot.bpm);
    tft.setTextColor(C_ACCENT_L, blend16(C_ACCENT, C_BG, 16));
    tft.drawString(buf, R_BPM_VAL.x + R_BPM_VAL.w / 2, R_BPM_VAL.y + R_BPM_VAL.h / 2 - 1);

    /* Slot Selector */
    for (int i = 0; i < 3; i++) {
        Rect r = (i == 0) ? R_SLOT_1 : (i == 1) ? R_SLOT_2 : R_SLOT_3;
        bool active = (ui.activeSlot == i);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 0, active ? blend16(C_ACCENT, C_BG, 32) : C_BG);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 0, active ? blend16(C_ACCENT, C_BG, 102) : C_BORDER_DIM);
        tft.setTextColor(active ? C_ACCENT_L : C_TXT_DIM, active ? blend16(C_ACCENT, C_BG, 32) : C_BG);
        snprintf(buf, sizeof(buf), "%d", i + 1);
        tft.drawString(buf, r.x + r.w / 2, r.y + r.h / 2 - 1);
    }

    /* Save / Load */
    tft.drawRect(R_SAVE.x, R_SAVE.y, R_SAVE.w, R_SAVE.h, C_BORDER);
    tft.setTextColor(C_TXT_STAT, C_BG);
    tft.drawString("SV", R_SAVE.x + R_SAVE.w / 2, R_SAVE.y + R_SAVE.h / 2 - 1);

    tft.drawRect(R_LOAD.x, R_LOAD.y, R_LOAD.w, R_LOAD.h, C_BORDER);
    tft.drawString("LD", R_LOAD.x + R_LOAD.w / 2, R_LOAD.y + R_LOAD.h / 2 - 1);
}
/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — STATUS TEXT (overlaid in BPM value box)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_status(void) {} // Deprecated, drawn in footer

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — TRACK SELECTOR (left margin)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_tracks(void) {
    tft.fillRect(0, 30, 62, 210, C_PANEL_BG);
    tft.drawFastVLine(61, 30, 210, C_DIV);

    tft.setTextColor(C_TXT_DIM, C_PANEL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("E U C L I D", 31, 37);

    char lbl[2] = {0, 0};
    for (int i = 0; i < TRACK_COUNT; i++) {
        bool sel = (ui.snapshot.activeTrack == i);
        bool mut = ui.snapshot.trackMutes[i];
        uint16_t col  = mut ? blend16(TRACK_COLORS[i], C_PANEL_BG, 60) : TRACK_COLORS[i];
        uint16_t dim_fill = blend16(C_BORDER_DIM, C_PANEL_BG, 60);

        tft.drawRect(R_TRACKS[i].x, R_TRACKS[i].y, R_TRACKS[i].w, R_TRACKS[i].h, sel ? blend16(col, C_PANEL_BG, 102) : C_DIV);

        if (sel) {
            tft.fillRect(R_TRACKS[i].x, R_TRACKS[i].y, 2, R_TRACKS[i].h, col);
        }

        tft.fillCircle(R_TRACKS[i].x + 12, R_TRACKS[i].y + R_TRACKS[i].h / 2, 2, col);

        lbl[0] = TRACK_CHARS[i];
        tft.setTextColor(sel ? col : C_TXT_DIM, C_PANEL_BG);
        tft.drawString(lbl, R_TRACKS[i].x + 28, R_TRACKS[i].y + R_TRACKS[i].h / 2);
    }
}
/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — EUCLIDEAN RING (via sprite — atomic push, zero flicker)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_ring(void) {
    s_ring.fillSprite(C_PANEL_BG);

    /* Guide circles */
    for (int t = 0; t < 4; t++) { // 4 rings for K, S, C, O
        s_ring.drawCircle(RING_CX, RING_CY, RING_RADII[t], C_DIV);
        s_ring.drawCircle(RING_CX, RING_CY, RING_RADII[t] - 1, C_DIV);
    }

    /* Playhead spoke */
    if (ui.snapshot.isPlaying) {
        float ang = ((ui.snapshot.currentStep % 16) / 16.0f) * 2.0f * (float)M_PI - (float)(M_PI / 2.0);
        int px = RING_CX + (int)(25 * cosf(ang));
        int py = RING_CY + (int)(25 * sinf(ang));
        for (int i = 0; i < 25; i+=2) { // dashed line
            int dx = RING_CX + (int)(i * cosf(ang));
            int dy = RING_CY + (int)(i * sinf(ang));
            s_ring.drawPixel(dx, dy, blend16(C_ACCENT, C_PANEL_BG, 96));
        }

        // extra dot on K ring
        int kx = RING_CX + (int)(RING_RADII[0] * cosf(ang));
        int ky = RING_CY + (int)(RING_RADII[0] * sinf(ang));
        s_ring.fillCircle(kx, ky, 2, C_BG);
        s_ring.drawCircle(kx, ky, 2, C_ACCENT_L);
    }

    /* Step dots */
    for (int t = 0; t < 4; t++) { // Only first 4 tracks have rings
        int len = ui.snapshot.patternLens[t];
        if (len <= 0) continue;
        bool sel = (t == ui.snapshot.activeTrack);
        bool mut = ui.snapshot.trackMutes[t];
        uint16_t col = TRACK_COLORS[t];
        uint8_t  rad = RING_RADII[t];

        for (int i = 0; i < len; i++) {
            float ang = (2.0f * (float)M_PI * i / len) - (float)(M_PI / 2.0);
            int16_t px = (int16_t)(RING_CX + rad * cosf(ang));
            int16_t py = (int16_t)(RING_CY + rad * sinf(ang));
            bool active   = ui.snapshot.patterns[t][i] > 0;

            if (mut) {
                if (active) s_ring.fillCircle(px, py, 1, blend16(col, C_PANEL_BG, 60));
            } else if (active) {
                s_ring.fillCircle(px, py, 2, col);
            }
        }
    }

    s_ring.pushSprite(R_RING.x, R_RING.y);
}
/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — BOTTOM ACTION ROW (Mute / Voice / Mix)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_tabs(void) {
    bool is_seq = (ui.mode == UiMode::PATTERN_EDIT);
    bool is_dsp = (ui.mode == UiMode::SOUND_EDIT);
    bool is_mix = (ui.mode == UiMode::MIXER);

    /* SEQ Tab */
    tft.fillRect(R_TAB_SEQ.x, R_TAB_SEQ.y, R_TAB_SEQ.w, R_TAB_SEQ.h, is_seq ? C_FILL_TAB : C_PANEL_BG);
    tft.drawFastVLine(R_TAB_SEQ.x + R_TAB_SEQ.w - 1, R_TAB_SEQ.y, R_TAB_SEQ.h, C_DIV);
    if (is_seq) tft.drawFastHLine(R_TAB_SEQ.x, R_TAB_SEQ.y + R_TAB_SEQ.h - 1, R_TAB_SEQ.w, C_ACCENT);
    tft.setTextColor(is_seq ? C_ACCENT_L : C_TXT_DIM, is_seq ? C_FILL_TAB : C_PANEL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("S E Q", R_TAB_SEQ.x + R_TAB_SEQ.w / 2, R_TAB_SEQ.y + R_TAB_SEQ.h / 2);

    /* DSP Tab */
    tft.fillRect(R_TAB_DSP.x, R_TAB_DSP.y, R_TAB_DSP.w, R_TAB_DSP.h, is_dsp ? C_FILL_TAB : C_PANEL_BG);
    tft.drawFastVLine(R_TAB_DSP.x + R_TAB_DSP.w - 1, R_TAB_DSP.y, R_TAB_DSP.h, C_DIV);
    if (is_dsp) tft.drawFastHLine(R_TAB_DSP.x, R_TAB_DSP.y + R_TAB_DSP.h - 1, R_TAB_DSP.w, C_ACCENT);
    tft.setTextColor(is_dsp ? C_ACCENT_L : C_TXT_DIM, is_dsp ? C_FILL_TAB : C_PANEL_BG);
    tft.drawString("D S P", R_TAB_DSP.x + R_TAB_DSP.w / 2, R_TAB_DSP.y + R_TAB_DSP.h / 2);

    /* MIX Tab */
    tft.fillRect(R_TAB_MIX.x, R_TAB_MIX.y, R_TAB_MIX.w, R_TAB_MIX.h, is_mix ? C_FILL_TAB : C_PANEL_BG);
    if (is_mix) tft.drawFastHLine(R_TAB_MIX.x, R_TAB_MIX.y + R_TAB_MIX.h - 1, R_TAB_MIX.w, C_ACCENT);
    tft.setTextColor(is_mix ? C_ACCENT_L : C_TXT_DIM, is_mix ? C_FILL_TAB : C_PANEL_BG);
    tft.drawString("M I X", R_TAB_MIX.x + R_TAB_MIX.w / 2, R_TAB_MIX.y + R_TAB_MIX.h / 2);
}

static void draw_footer(void) {
    tft.fillRect(R_FOOTER.x, R_FOOTER.y, R_FOOTER.w, R_FOOTER.h, C_PANEL_BG);
    tft.drawFastHLine(R_FOOTER.x, R_FOOTER.y, R_FOOTER.w, C_DIV);

    int tr = ui.snapshot.activeTrack;

    // Left Zone
    if (ui.statusUntilMs > millis()) {
        tft.setTextColor(C_TXT_STAT, C_PANEL_BG);
        tft.setTextDatum(ML_DATUM);
        tft.drawString(ui.status, R_FOOTER.x + 6, R_FOOTER.y + 8);
    } else {
        bool pl = ui.snapshot.isPlaying;
        uint16_t fill = pl ? blend16(C_ACCENT, C_PANEL_BG, 33) : C_PANEL_BG;
        tft.fillRoundRect(R_FOOTER.x + 6, R_FOOTER.y + 2, 60, 12, 2, fill);
        if (pl) tft.drawRoundRect(R_FOOTER.x + 6, R_FOOTER.y + 2, 60, 12, 2, blend16(C_ACCENT, C_PANEL_BG, 76));
        tft.setTextColor(pl ? blend16(C_ACCENT_L, C_PANEL_BG, 119) : C_TXT_STAT, fill);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(pl ? "X PLAYING" : "> STOP", R_FOOTER.x + 6 + 30, R_FOOTER.y + 8);
    }

    // Center Zone
    char center_buf[32];
    int steps = ui.snapshot.trackSteps[tr];
    int rot = ui.snapshot.trackRotations[tr];
    int hits = ui.snapshot.trackHits[tr];
    snprintf(center_buf, sizeof(center_buf), "%c - %d/%d - ROT %d", TRACK_CHARS[tr], hits, steps, rot);
    tft.setTextColor(C_TXT_STAT, C_PANEL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(center_buf, R_FOOTER.x + R_FOOTER.w / 2, R_FOOTER.y + 8);

    // Right Zone
    char right_buf[16];
    snprintf(right_buf, sizeof(right_buf), "step %d/%d", steps > 0 ? (ui.snapshot.currentStep % steps) + 1 : 1, steps);
    tft.setTextColor(C_TXT_DIM, C_PANEL_BG);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(right_buf, R_FOOTER.x + R_FOOTER.w - 6, R_FOOTER.y + 8);
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
            strncpy(out->label, lbls[row], sizeof(out->label) - 1);
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
                strncpy(out->label, "Density", sizeof(out->label) - 1);
                snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
                out->norm = bp.density; break;
            case 1:
                strncpy(out->label, "Range", sizeof(out->label) - 1);
                snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
                out->norm = (float)(bp.range - 1) / 11.0f; break;
            case 2: {
                strncpy(out->label, "Scale", sizeof(out->label) - 1);
                int sc = constrain((int)bp.scaleType, 0, 4);
                strncpy(out->value, SCALE_NAMES[sc], sizeof(out->value) - 1);
                out->norm = (float)sc / 4.0f; break;
            }
            case 3: {
                strncpy(out->label, "Root", sizeof(out->label) - 1);
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
            strncpy(out->label, "Steps", sizeof(out->label) - 1);
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = (float)(steps - 4) / 60.0f; break;
        case 1:
            strncpy(out->label, "Hits", sizeof(out->label) - 1);
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = steps > 0
                ? (float)ui.snapshot.trackHits[tr] / steps : 0.0f; break;
        case 2:
            strncpy(out->label, "Rot", sizeof(out->label) - 1);
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = steps > 1
                ? (float)ui.snapshot.trackRotations[tr] / (steps - 1) : 0.0f; break;
        case 3:
            strncpy(out->label, "Vol", sizeof(out->label) - 1);
            snprintf(out->value, sizeof(out->value), "%d", getParamDisplayValue(row));
            out->norm = ui.snapshot.voiceGain[tr]; break;
        default: break;
    }
}


/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — TAB CONTENT
   ═══════════════════════════════════════════════════════════════════════════════ */

static void draw_tab_seq(void) {
    s_panel.fillSprite(C_PANEL_BG);

    int tr = ui.snapshot.activeTrack;
    uint16_t tc = TRACK_COLORS[tr];

    // Grid 16 steps (2x8)
    int cell_w = 28;
    int cell_h = 14;
    int start_x = 6;
    int start_y = 7;

    for (int i = 0; i < 16; i++) {
        int row = i / 8;
        int col = i % 8;

        int x = start_x + col * (cell_w + 2);
        if (col >= 4) x += 3; // gap after 4 beats

        int y = start_y + row * (cell_h + 2);

        bool active = ui.snapshot.patterns[tr][i] > 0;
        bool playhead = ui.snapshot.isPlaying && (i == ui.snapshot.currentStep % 16);

        if (playhead) {
            s_panel.fillRect(x, y, cell_w, cell_h, blend16(tc, C_PANEL_BG, 80));
            s_panel.drawRect(x, y, cell_w, cell_h, C_ACCENT_L);
        } else if (active) {
            s_panel.fillRect(x, y, cell_w, cell_h, blend16(C_FILL_BTN, C_PANEL_BG, 128)); // base fill
            // emulated diagonal overlay
            for (int dx = 0; dx < cell_w; dx+=2) {
                s_panel.drawLine(x + dx, y, x, y + dx > y + cell_h ? y + cell_h : y + dx, blend16(tc, C_PANEL_BG, 68));
            }
            s_panel.drawRect(x, y, cell_w, cell_h, blend16(tc, C_PANEL_BG, 102));
        } else {
            s_panel.fillRoundRect(x, y, cell_w, cell_h, 2, C_FILL_TAB);
            s_panel.drawRoundRect(x, y, cell_w, cell_h, 2, C_BORDER_DIM);
        }
    }

    // Steppers HITS and ROT
    int hits = ui.snapshot.trackHits[tr];
    int rot = ui.snapshot.trackRotations[tr];

    int stepper_y = start_y + 2 * (cell_h + 2) + 6;

    // HITS
    s_panel.setTextColor(C_TXT_STAT, C_PANEL_BG);
    s_panel.setTextDatum(ML_DATUM);
    s_panel.drawString("HITS", start_x, stepper_y + 8);

    int step_btn_w = 14;
    int step_val_w = 30;
    int step_x = start_x + 28 + 6;

    s_panel.fillRoundRect(step_x, stepper_y, step_btn_w, step_btn_w, 2, C_FILL_TAB);
    s_panel.drawRoundRect(step_x, stepper_y, step_btn_w, step_btn_w, 2, C_BORDER_DIM);
    s_panel.setTextColor(C_TXT_SEC, C_FILL_TAB);
    s_panel.setTextDatum(MC_DATUM);
    s_panel.drawString("-", step_x + step_btn_w/2, stepper_y + step_btn_w/2);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", hits);
    s_panel.setTextColor(C_ACCENT_L, C_PANEL_BG);
    s_panel.drawString(buf, step_x + step_btn_w + step_val_w/2, stepper_y + step_btn_w/2);

    int plus_x = step_x + step_btn_w + step_val_w;
    s_panel.fillRoundRect(plus_x, stepper_y, step_btn_w, step_btn_w, 2, C_FILL_TAB);
    s_panel.drawRoundRect(plus_x, stepper_y, step_btn_w, step_btn_w, 2, C_BORDER_DIM);
    s_panel.setTextColor(C_TXT_SEC, C_FILL_TAB);
    s_panel.drawString("+", plus_x + step_btn_w/2, stepper_y + step_btn_w/2);

    // ROT
    stepper_y += step_btn_w + 6;
    s_panel.setTextColor(C_TXT_STAT, C_PANEL_BG);
    s_panel.setTextDatum(ML_DATUM);
    s_panel.drawString("ROT", start_x, stepper_y + 8);

    s_panel.fillRoundRect(step_x, stepper_y, step_btn_w, step_btn_w, 2, C_FILL_TAB);
    s_panel.drawRoundRect(step_x, stepper_y, step_btn_w, step_btn_w, 2, C_BORDER_DIM);
    s_panel.setTextColor(C_TXT_SEC, C_FILL_TAB);
    s_panel.setTextDatum(MC_DATUM);
    s_panel.drawString("-", step_x + step_btn_w/2, stepper_y + step_btn_w/2);

    snprintf(buf, sizeof(buf), "%d", rot);
    s_panel.setTextColor(C_ACCENT_L, C_PANEL_BG);
    s_panel.drawString(buf, step_x + step_btn_w + step_val_w/2, stepper_y + step_btn_w/2);

    s_panel.fillRoundRect(plus_x, stepper_y, step_btn_w, step_btn_w, 2, C_FILL_TAB);
    s_panel.drawRoundRect(plus_x, stepper_y, step_btn_w, step_btn_w, 2, C_BORDER_DIM);
    s_panel.setTextColor(C_TXT_SEC, C_FILL_TAB);
    s_panel.drawString("+", plus_x + step_btn_w/2, stepper_y + step_btn_w/2);

    s_panel.pushSprite(PANEL_SX, PANEL_SY);
}

static void draw_tab_dsp(void) {
    s_panel.fillSprite(C_PANEL_BG);
    int tr = ui.snapshot.activeTrack;
    bool bass = tr == VOICE_BASS;

    int padding = 6;
    int kw = (PANEL_W - 3*padding) / 2;
    int kh = (PANEL_H - 3*padding) / 2;

    for (int i=0; i<4; i++) {
        int row = i / 2;
        int col = i % 2;
        int x = padding + col * (kw + padding);
        int y = padding + row * (kh + padding);

        s_panel.fillRoundRect(x, y, kw, kh, 3, C_FILL_BTN);
        s_panel.drawRoundRect(x, y, kw, kh, 3, C_DIV);

        ParamMeta m;
        param_meta(i, &m);
        if (m.label[0] == '\0') continue;

        uint16_t arc_col = (i == 2) ? RGB565(0x00, 0xAA, 0xFF) : C_ACCENT;

        int cx = x + kw/2;
        int cy = y + kh/2;
        int r = 14;

        // Background arc
        s_panel.drawArc(cx, cy, r, r-2, 135, 405, C_DIV);

        // Foreground arc
        int end_ang = 135 + (int)(m.norm * 270);
        if (end_ang > 135) {
            s_panel.drawArc(cx, cy, r, r-2, 135, end_ang, arc_col);
        }

        s_panel.setTextColor(C_TXT_STAT, C_FILL_BTN);
        s_panel.setTextDatum(MC_DATUM);
        s_panel.drawString(m.label, cx, y + 8);

        s_panel.setTextColor(C_ACCENT_L, C_FILL_BTN);
        s_panel.drawString(m.value, cx, y + kh - 8);
    }

    s_panel.pushSprite(PANEL_SX, PANEL_SY);
}

static void draw_tab_mix(void) {
    s_panel.fillSprite(C_PANEL_BG);

    int padding_y = 5;
    int padding_x = 5;
    int bar_h = 18;
    int gap = 4;

    int total_w = PANEL_W - 2 * padding_x;
    int lbl_w = 14;
    int val_w = 24;
    int bar_w = total_w - lbl_w - val_w - 8;

    int y = padding_y;
    char buf[8];

    for (int i=0; i < TRACK_COUNT; i++) {
        s_panel.setTextColor(TRACK_COLORS[i], C_PANEL_BG);
        s_panel.setTextDatum(ML_DATUM);
        char lbl[2] = {TRACK_CHARS[i], '\0'};
        s_panel.drawString(lbl, padding_x, y + bar_h/2);

        int bx = padding_x + lbl_w + 4;
        s_panel.fillRoundRect(bx, y + 5, bar_w, 8, 2, C_FILL_TAB);
        s_panel.drawRoundRect(bx, y + 5, bar_w, 8, 2, C_BORDER_DIM);

        float gain = ui.snapshot.voiceGain[i];
        int fw = (int)(gain * bar_w);
        if (fw > 0) {
            uint16_t dim_col = blend16(TRACK_COLORS[i], C_PANEL_BG, 80);
            s_panel.fillRoundRect(bx, y + 5, fw, 8, 2, TRACK_COLORS[i]);
            s_panel.fillRect(bx + fw/2, y + 5, fw - fw/2, 8, blend16(dim_col, TRACK_COLORS[i], 128)); // Emulate gradient
        }

        s_panel.setTextColor(C_TXT_STAT, C_PANEL_BG);
        s_panel.setTextDatum(MR_DATUM);
        snprintf(buf, sizeof(buf), "%d", (int)(gain * 100));
        s_panel.drawString(buf, padding_x + total_w, y + bar_h/2);

        y += bar_h + gap;
    }

    y += gap; // Extra margin before MST
    s_panel.drawFastHLine(padding_x, y, total_w, C_DIV);
    y += gap + 1;

    s_panel.setTextColor(C_TXT_SEC, C_PANEL_BG);
    s_panel.setTextDatum(ML_DATUM);
    s_panel.drawString("M", padding_x, y + bar_h/2);

    int bx = padding_x + lbl_w + 4;
    s_panel.fillRoundRect(bx, y + 5, bar_w, 8, 2, C_FILL_TAB);
    s_panel.drawRoundRect(bx, y + 5, bar_w, 8, 2, C_BORDER_DIM);

    float mgain = ui.snapshot.masterVolume;
    int mfw = (int)(mgain * bar_w);
    if (mfw > 0) {
        s_panel.fillRoundRect(bx, y + 5, mfw, 8, 2, C_TXT_STAT);
    }

    s_panel.setTextColor(C_TXT_STAT, C_PANEL_BG);
    s_panel.setTextDatum(MR_DATUM);
    snprintf(buf, sizeof(buf), "%d", (int)(mgain * 100));
    s_panel.drawString(buf, padding_x + total_w, y + bar_h/2);

    s_panel.pushSprite(PANEL_SX, PANEL_SY);
}

static void draw_params(void) {
    if (ui.mode == UiMode::PATTERN_EDIT) draw_tab_seq();
    else if (ui.mode == UiMode::SOUND_EDIT) draw_tab_dsp();
    else if (ui.mode == UiMode::MIXER) draw_tab_mix();
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
    if (tp.justReleased) { hold_stop(); return; }
    if (!tp.justPressed) return;

    int tx = tp.x, ty = tp.y;

    /* ── Topbar (y < 30) ────────────────────────────────────────────────── */
    if (ty < 30) {
        if (R_PLAY.contains(tx, ty))  { postUiAction(UiActionType::TOGGLE_PLAY, 0, 0); return; }
        if (R_BPM_DEC.contains(tx, ty)) { postUiAction(UiActionType::NUDGE_BPM, 0, -1); hold_start(10, -1); return; }
        if (R_BPM_INC.contains(tx, ty)) { postUiAction(UiActionType::NUDGE_BPM, 0, +1); hold_start(10, +1); return; }

        if (R_SLOT_1.contains(tx, ty)) { ui.activeSlot = 0; return; }
        if (R_SLOT_2.contains(tx, ty)) { ui.activeSlot = 1; return; }
        if (R_SLOT_3.contains(tx, ty)) { ui.activeSlot = 2; return; }

        if (R_SAVE.contains(tx, ty))  { set_status(PatternStore.saveSlot(ui.activeSlot) ? "SAVED!" : "SAVE ERR"); return; }
        if (R_LOAD.contains(tx, ty))  { set_status(PatternStore.loadSlot(ui.activeSlot) ? "LOADED!" : "LOAD ERR"); return; }
        return;
    }

    /* ── Left Panel (x < 62, y > 30) ───────────────────────────────────────────── */
    if (tx < 62 && ty > 30) {
        // Track selector
        for (int i = 0; i < TRACK_COUNT; i++) {
            if (R_TRACKS[i].contains(tx, ty)) {
                postUiAction(UiActionType::SELECT_TRACK, 0, i); return;
            }
        }

        // Euclid ring area tap to enter pattern edit and focus
        if (R_RING.contains(tx, ty)) {
            postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::PATTERN_EDIT);
            ui.mode = UiMode::PATTERN_EDIT;
            return;
        }
        return;
    }

    /* ── Tabs (x >= 62, 30 <= y < 50) ────────────────────────────────────────────────── */
    if (tx >= 62 && ty >= 30 && ty < 50) {
        if (R_TAB_SEQ.contains(tx, ty)) { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::PATTERN_EDIT); ui.mode = UiMode::PATTERN_EDIT; return; }
        if (R_TAB_DSP.contains(tx, ty)) { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::SOUND_EDIT); ui.mode = UiMode::SOUND_EDIT; return; }
        if (R_TAB_MIX.contains(tx, ty)) { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::MIXER); ui.mode = UiMode::MIXER; return; }
        return;
    }

    /* ── Tab Content (x >= 62, 50 <= y < 224) ────────────────────────────────────────── */
    if (tx >= 62 && ty >= 50 && ty < 224) {
        int lx = tx - PANEL_SX;
        int ly = ty - PANEL_SY;

        if (ui.mode == UiMode::PATTERN_EDIT) {
            // Check grid clicks
            int cell_w = 28;
            int cell_h = 14;
            int start_x = 6;
            int start_y = 7;
            for (int i = 0; i < 16; i++) {
                int row = i / 8;
                int col = i % 8;
                int x = start_x + col * (cell_w + 2);
                if (col >= 4) x += 3;
                int y = start_y + row * (cell_h + 2);


            }

            // Check steppers
            int stepper_y = start_y + 2 * (cell_h + 2) + 6;
            int step_btn_w = 14;
            int step_val_w = 30;
            int step_x = start_x + 28 + 6;
            int plus_x = step_x + step_btn_w + step_val_w;

            // HITS stepper (row 1 param)
            if (ly >= stepper_y && ly < stepper_y + step_btn_w) {
                if (lx >= step_x && lx < step_x + step_btn_w) { param_delta(1, -1); return; }
                if (lx >= plus_x && lx < plus_x + step_btn_w) { param_delta(1, +1); return; }
            }

            // ROT stepper (row 2 param)
            stepper_y += step_btn_w + 6;
            if (ly >= stepper_y && ly < stepper_y + step_btn_w) {
                if (lx >= step_x && lx < step_x + step_btn_w) { param_delta(2, -1); return; }
                if (lx >= plus_x && lx < plus_x + step_btn_w) { param_delta(2, +1); return; }
            }
            return;
        }

        if (ui.mode == UiMode::SOUND_EDIT) {
            // Click quadrants for DSP
            int padding = 6;
            int kw = (PANEL_W - 3*padding) / 2;
            int kh = (PANEL_H - 3*padding) / 2;

            for (int i=0; i<4; i++) {
                int row = i / 2;
                int col = i % 2;
                int x = padding + col * (kw + padding);
                int y = padding + row * (kh + padding);

                if (lx >= x && lx < x + kw && ly >= y && ly < y + kh) {
                    // Tap left half = minus, right half = plus
                    if (lx < x + kw/2) { param_delta(i, -1); return; }
                    else { param_delta(i, +1); return; }
                }
            }
            return;
        }

        if (ui.mode == UiMode::MIXER) {
            // Vertical click on mixer bars
            int padding_y = 5;
            int padding_x = 5;
            int bar_h = 18;
            int gap = 4;
            int lbl_w = 14;
            int val_w = 24;
            int total_w = PANEL_W - 2 * padding_x;
            int bar_w = total_w - lbl_w - val_w - 8;

            int y = padding_y;

            for (int i=0; i < TRACK_COUNT; i++) {
                int bx = padding_x + lbl_w + 4;
                if (lx >= bx && lx < bx + bar_w && ly >= y && ly < y + bar_h) {
                    float norm = (float)(lx - bx) / bar_w;
                    postUiAction(UiActionType::SET_VOICE_GAIN, i, (int)(norm * 100));
                    return;
                }

                // Clicking label toggles mute
                if (lx >= padding_x && lx < padding_x + lbl_w && ly >= y && ly < y + bar_h) {
                    postUiAction(UiActionType::TOGGLE_MUTE, 0, i);
                    return;
                }

                y += bar_h + gap;
            }
            return;
        }
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
  tft.setTextSize(1); // Default 6x8 font

  /* ── Create sprites ────────────────────────────────────────────────────── */
  s_ring.createSprite(50, 50);
  s_ring.setColorDepth(16);
  s_panel.createSprite(258, 174);
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
          strncpy(ui.status, ui.snapshot.isPlaying ? "PLAYING" : "READY",
                  sizeof(ui.status) - 1);
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
          draw_tabs();
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

          if (any_change) { draw_tabs(); draw_footer(); draw_params(); }
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
      strncpy(ui.lastStatus, ui.status, sizeof(ui.lastStatus) - 1);

      vTaskDelay(pdMS_TO_TICKS(16));   /* ~60 fps budget */
  }
}
