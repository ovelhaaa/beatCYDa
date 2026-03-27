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
constexpr uint16_t C_BG            = 0x10A2u; // grafite escuro
constexpr uint16_t C_PANEL         = 0x2124u; // painel escuro
constexpr uint16_t C_PANEL_2       = 0x31A6u; // painel secundario/botoes
constexpr uint16_t C_STROKE        = 0x4228u; // borda
constexpr uint16_t C_DIVIDER       = 0x31A6u; // separador
constexpr uint16_t C_TEXT          = 0xFFFFu; // off-white
constexpr uint16_t C_TEXT_DIM      = 0x7BEFu; // texto secundario
constexpr uint16_t C_ACCENT_SEQ    = 0xFD20u; // ambar/laranja
constexpr uint16_t C_ACCENT_STATUS = 0x07FFu; // ciano
constexpr uint16_t C_PLAY          = 0x07E0u; // verde
constexpr uint16_t C_STOP          = 0xF800u; // vermelho suave
constexpr uint16_t C_MUTE          = 0xB800u; // vermelho discreto

static const uint16_t TRACK_COLORS[TRACK_COUNT] = {
    0xFA00u, // KICK (Orange-ish)
    0x066Cu, // SNRE (Green-ish)
    0x067Fu, // HATC (Cyan-ish)
    0xC21Fu, // HATO (Purple-ish)
    0xFE40u  // BASS (Yellow-ish)
};

static const char TRACK_CHARS[TRACK_COUNT] = {'K','S','C','O','B'};

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

/* Base Rects */
static const Rect R_HEADER = {0, 0, 320, 40};
static const Rect R_STATUS = {0, 40, 320, 16};
static const Rect R_TRACKS_AREA = {0, 56, 68, 148};
static const Rect R_CENTER = {68, 56, 148, 148};
static const Rect R_PANEL  = {216, 56, 104, 148};
static const Rect R_TABS   = {0, 204, 320, 36};

/* Center area (sprite origin) */
static const Rect R_RING = {R_CENTER.x, R_CENTER.y, R_CENTER.w, R_CENTER.h};
#define RING_CX (R_CENTER.w / 2)   /* sprite-relative centre */
#define RING_CY (R_CENTER.h / 2)
static const uint8_t RING_RADII = 55; // simplified to a single ring

/* Header Sub-areas */
static const Rect R_PLAY    = {6, 6, 52, 28};
static const Rect R_BPM_BOX = {66, 4, 92, 32};
static const Rect R_SLOT_BOX= {166, 6, 58, 28};
static const Rect R_SAVE    = {232, 6, 38, 28};
static const Rect R_LOAD    = {276, 6, 38, 28};

/* Track Selector (Vertical, in TRACKS area) */
static const Rect R_TRACKS[TRACK_COUNT] = {
    {4,  60, 60, 24},
    {4,  88, 60, 24},
    {4, 116, 60, 24},
    {4, 144, 60, 24},
    {4, 172, 60, 24},
};

/* Tab Selector (Bottom) */
static const Rect R_TAB_SEQ   = {4,   208, 100, 28};
static const Rect R_TAB_SOUND = {110, 208, 100, 28};
static const Rect R_TAB_MIX   = {216, 208, 100, 28};

/* Param Panel rows (inside PANEL sprite) */
#define PANEL_SX R_PANEL.x
#define PANEL_SY R_PANEL.y
#define PANEL_W  R_PANEL.w
#define PANEL_H  R_PANEL.h

struct ParamRow {
    Rect minus;
    Rect value;
    Rect plus;
    Rect touch;
};

// Y spacing inside 104x148 panel
static const ParamRow PARAM_ROWS[4] = {
    {{ 4, 20, 24, 24}, { 30, 20, 44, 24}, { 76, 20, 24, 24}, {0,  4, PANEL_W, 40}},
    {{ 4, 56, 24, 24}, { 30, 56, 44, 24}, { 76, 56, 24, 24}, {0, 44, PANEL_W, 36}},
    {{ 4, 92, 24, 24}, { 30, 92, 44, 24}, { 76, 92, 24, 24}, {0, 80, PANEL_W, 36}},
    {{ 4,124, 24, 24}, { 30,124, 44, 24}, { 76,124, 24, 24}, {0,116, PANEL_W, 32}},
};

/* Mixer Faders (In combined CENTER+PANEL area, 68,56 to 320,204 -> w=252) */
static const Rect R_SLIDERS[TRACK_COUNT] = {
    { 80,  70, 36, 120},
    {130,  70, 36, 120},
    {180,  70, 36, 120},
    {230,  70, 36, 120},
    {280,  70, 36, 120},
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

  // Touch state for long press
  uint32_t touchPressedMs = 0;
  int16_t touchPressedX = -1;
  int16_t touchPressedY = -1;
  bool touchLongTriggered = false;

  char status[32] = "CYD ready";
  char lastStatus[32] = "";
  uint32_t statusUntilMs = 0;
  bool forceRedraw = true;

  // Dirty Regions
  bool dirtyHeader = true;
  bool dirtyStatus = true;
  bool dirtyTracks = true;
  bool dirtyCenter = true;
  bool dirtyPanel  = true;
  bool dirtyTabs   = true;
} ui;


/* ═══════════════════════════════════════════════════════════════════════════════
   FONTS
   ═══════════════════════════════════════════════════════════════════════════════ */
static void setFontUiSmall(LGFX_Sprite *spr = nullptr) {
    if (spr) spr->setFont(&fonts::FreeSansBold12pt7b);
    else tft.setFont(&fonts::FreeSansBold12pt7b);
}

static void setFontUiLarge(LGFX_Sprite *spr = nullptr) {
    if (spr) spr->setFont(&fonts::FreeSansBold18pt7b);
    else tft.setFont(&fonts::FreeSansBold18pt7b);
}

static void setFontValueSmall(LGFX_Sprite *spr = nullptr) {
    if (spr) spr->setFont(&fonts::FreeMonoBold12pt7b);
    else tft.setFont(&fonts::FreeMonoBold12pt7b);
}

static void setFontValueLarge(LGFX_Sprite *spr = nullptr) {
    if (spr) spr->setFont(&fonts::FreeMonoBold18pt7b);
    else tft.setFont(&fonts::FreeMonoBold18pt7b);
}

static void setFontHeroNumber(LGFX_Sprite *spr = nullptr) {
    if (spr) spr->setFont(&fonts::FreeMonoBold24pt7b);
    else tft.setFont(&fonts::FreeMonoBold24pt7b);
}

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
        if (outline != fill) spr->drawRoundRect(r->x, r->y, r->w, r->h, 3, outline);
        spr->setTextColor(text_col, fill);
        spr->setTextDatum(MC_DATUM);
        spr->drawString(label, r->x + r->w / 2, r->y + r->h / 2 + 3); // Adjust for specific font vertical centering
    } else {
        tft.fillRoundRect(r->x, r->y, r->w, r->h, 3, fill);
        if (outline != fill) tft.drawRoundRect(r->x, r->y, r->w, r->h, 3, outline);
        tft.setTextColor(text_col, fill);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(label, r->x + r->w / 2, r->y + r->h / 2 + 3);
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
   DRAW — HEADER (y 0–39)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_header(void) {
    tft.fillRect(R_HEADER.x, R_HEADER.y, R_HEADER.w, R_HEADER.h, C_BG);

    /* Play / Stop */
    bool pl = ui.snapshot.isPlaying;
    setFontUiLarge(); // PLAY/STOP = FreeSansBold18pt7b
    draw_btn(NULL, &R_PLAY, pl ? "II" : ">",
             pl ? C_PLAY : C_PANEL_2,
             C_STROKE,
             C_TEXT);

    /* BPM */
    setFontValueLarge(); // Use large value font for BPM
    char buf[8];
    snprintf(buf, sizeof(buf), "%3d", ui.snapshot.bpm);
    draw_btn(NULL, &R_BPM_BOX, buf, C_PANEL_2, C_STROKE, C_TEXT);
    // Draw small BPM label
    setFontUiSmall();
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_TEXT_DIM, C_PANEL_2);
    tft.drawString("BPM", R_BPM_BOX.x + 24, R_BPM_BOX.y + R_BPM_BOX.h - 6);

    /* Slot */
    setFontValueLarge();
    snprintf(buf, sizeof(buf), "%02d", ui.activeSlot + 1);
    draw_btn(NULL, &R_SLOT_BOX, buf, C_PANEL_2, C_STROKE, C_TEXT);
    setFontUiSmall();
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_TEXT_DIM, C_PANEL_2);
    tft.drawString("SL", R_SLOT_BOX.x + 16, R_SLOT_BOX.y + R_SLOT_BOX.h - 6);


    /* Save / Load */
    setFontUiSmall();
    draw_btn(NULL, &R_SAVE, "S", C_PANEL_2, C_STROKE, C_TEXT);
    draw_btn(NULL, &R_LOAD, "L", C_PANEL_2, C_STROKE, C_TEXT);

    tft.drawFastHLine(0, 39, 320, C_DIVIDER);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — STATUS BAR (y 40–55)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_status(void) {
    tft.fillRect(R_STATUS.x, R_STATUS.y, R_STATUS.w, R_STATUS.h, C_BG);
    tft.drawFastHLine(0, 55, 320, C_DIVIDER);

    bool active = (ui.statusUntilMs > millis());
    const char *msg = active ? ui.status
                     : ui.snapshot.isPlaying
                         ? "PLAYING"
                         : "READY";

    setFontUiSmall();
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(active ? C_ACCENT_STATUS : C_TEXT_DIM, C_BG);
    tft.drawString(msg, R_STATUS.x + 8, R_STATUS.y + R_STATUS.h / 2 + 3);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — TRACK SELECTOR
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_tracks(void) {
    tft.fillRect(R_TRACKS_AREA.x, R_TRACKS_AREA.y, R_TRACKS_AREA.w, R_TRACKS_AREA.h, C_BG);
    char lbl[2] = {0, 0};

    setFontUiLarge();

    for (int i = 0; i < TRACK_COUNT; i++) {
        bool sel = (ui.snapshot.activeTrack == i);
        bool mut = ui.snapshot.trackMutes[i];
        uint16_t col  = TRACK_COLORS[i];

        uint16_t fill = sel ? col : C_PANEL;
        uint16_t brd  = sel ? col : C_STROKE;
        uint16_t txt  = sel ? C_BG : (mut ? C_TEXT_DIM : C_TEXT);

        lbl[0] = TRACK_CHARS[i];

        // Main track button area (left side)
        Rect mainR = {R_TRACKS[i].x, R_TRACKS[i].y, (int16_t)(R_TRACKS[i].w - 20), R_TRACKS[i].h};
        tft.fillRoundRect(mainR.x, mainR.y, mainR.w, mainR.h, 4, fill);
        if (brd != fill) tft.drawRoundRect(mainR.x, mainR.y, mainR.w, mainR.h, 4, brd);

        tft.setTextColor(txt, fill);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(lbl, mainR.x + mainR.w / 2, mainR.y + mainR.h / 2 + 3);

        // Mute badge area (right side)
        Rect muteR = {(int16_t)(R_TRACKS[i].x + R_TRACKS[i].w - 18), R_TRACKS[i].y, 18, R_TRACKS[i].h};
        uint16_t muteFill = mut ? C_MUTE : C_PANEL;
        tft.fillRoundRect(muteR.x, muteR.y, muteR.w, muteR.h, 4, muteFill);
        if (!mut) tft.drawRoundRect(muteR.x, muteR.y, muteR.w, muteR.h, 4, C_STROKE);

        if (mut) {
            setFontValueSmall();
            tft.setTextColor(C_TEXT, muteFill);
            tft.drawString("M", muteR.x + muteR.w / 2, muteR.y + muteR.h / 2 + 3);
            setFontUiLarge(); // restore
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — CENTER AREA (via sprite — atomic push, zero flicker)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_center(void) {
    if (ui.mode == UiMode::MIXER) {
        // Mixer draw is handled globally via main update to avoid sprite overlap issues
        // or can be drawn here if we expand the sprite width
        return;
    }

    s_ring.fillSprite(C_BG);

    if (ui.mode == UiMode::PATTERN_EDIT) {
        /* EUCLIDEAN RING (Simplified) */
        int t = ui.snapshot.activeTrack;
        uint16_t col = TRACK_COLORS[t];
        bool mut = ui.snapshot.trackMutes[t];

        /* Guide circle */
        s_ring.drawCircle(RING_CX, RING_CY, RING_RADII, blend16(col, C_BG, 80));

        // Step 1 marker (top)
        s_ring.drawLine(RING_CX, RING_CY - RING_RADII - 4, RING_CX, RING_CY - RING_RADII + 4, col);

        /* Playhead spoke */
        int len = ui.snapshot.patternLens[t];
        if (len > 0 && ui.snapshot.isPlaying) {
            float ang = (2.0f * (float)M_PI * (ui.snapshot.currentStep % len) / len) - (float)(M_PI / 2.0);
            s_ring.drawLine(RING_CX, RING_CY,
                            RING_CX + (int)((RING_RADII - 8) * cosf(ang)),
                            RING_CY + (int)((RING_RADII - 8) * sinf(ang)),
                            blend16(C_PLAY, C_BG, 40));
        }

        /* Step dots for active track only */
        if (len > 0) {
            int dr = 4;
            for (int i = 0; i < len; i++) {
                float ang = (2.0f * (float)M_PI * i / len) - (float)(M_PI / 2.0);
                int16_t px = (int16_t)(RING_CX + RING_RADII * cosf(ang));
                int16_t py = (int16_t)(RING_CY + RING_RADII * sinf(ang));
                bool active   = ui.snapshot.patterns[t][i] > 0;
                bool playhead = ui.snapshot.isPlaying && (i == ui.snapshot.currentStep % len);

                if (mut) {
                    if (active) s_ring.drawCircle(px, py, dr, blend16(col, C_BG, 40));
                } else if (playhead && active) {
                    s_ring.fillCircle(px, py, dr + 2, C_TEXT);
                } else if (playhead) {
                    s_ring.drawCircle(px, py, dr, col);
                } else if (active) {
                    s_ring.fillCircle(px, py, dr, col);
                } else {
                    s_ring.drawCircle(px, py, 2, C_STROKE);
                }
            }
        }

        /* Center info (Step / Length) */
        setFontHeroNumber(&s_ring);
        s_ring.setTextColor(C_TEXT, C_BG);
        s_ring.setTextDatum(MC_DATUM);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", len > 0 ? (ui.snapshot.currentStep % len) + 1 : 0);
        s_ring.drawString(buf, RING_CX, RING_CY - 8);

        setFontValueSmall(&s_ring);
        s_ring.setTextColor(C_TEXT_DIM, C_BG);
        snprintf(buf, sizeof(buf), "/ %d", len);
        s_ring.drawString(buf, RING_CX, RING_CY + 16);

    } else if (ui.mode == UiMode::SOUND_EDIT) {
        /* SOUND PREVIEW (Pulse) */
        int t = ui.snapshot.activeTrack;
        uint16_t col = TRACK_COLORS[t];
        int len = ui.snapshot.patternLens[t];

        bool isHit = false;
        if (ui.snapshot.isPlaying && len > 0) {
            isHit = ui.snapshot.patterns[t][ui.snapshot.currentStep % len] > 0;
            // Add a little visual sustain by checking recent hit.
            // In a real implementation we'd read an envelope follower from Engine.
        }

        int size = isHit ? 40 : 10;

        s_ring.fillCircle(RING_CX, RING_CY, size, blend16(col, C_BG, isHit ? 200 : 80));

        // Draw track letter in middle
        setFontHeroNumber(&s_ring);
        s_ring.setTextColor(C_BG);
        s_ring.setTextDatum(MC_DATUM);
        char lbl[2] = {TRACK_CHARS[t], 0};
        s_ring.drawString(lbl, RING_CX, RING_CY + 4);
    }

    s_ring.pushSprite(R_RING.x, R_RING.y);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — TABS (Bottom)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_tabs(void) {
    tft.fillRect(R_TABS.x, R_TABS.y, R_TABS.w, R_TABS.h, C_BG);
    tft.drawFastHLine(0, 204, 320, C_DIVIDER);

    bool seq   = (ui.mode == UiMode::PATTERN_EDIT);
    bool sound = (ui.mode == UiMode::SOUND_EDIT);
    bool mix   = (ui.mode == UiMode::MIXER);

    setFontUiLarge();

    draw_btn(NULL, &R_TAB_SEQ, "SEQ",
             seq ? C_ACCENT_SEQ : C_PANEL_2, C_STROKE, seq ? (uint16_t)C_BG : (uint16_t)C_TEXT);
    draw_btn(NULL, &R_TAB_SOUND, "SOUND",
             sound ? C_ACCENT_SEQ : C_PANEL_2, C_STROKE, sound ? (uint16_t)C_BG : (uint16_t)C_TEXT);
    draw_btn(NULL, &R_TAB_MIX, "MIX",
             mix ? C_ACCENT_SEQ : C_PANEL_2, C_STROKE, mix ? (uint16_t)C_BG : (uint16_t)C_TEXT);
}

/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — MIXER (Full Center + Panel width)
   ═══════════════════════════════════════════════════════════════════════════════ */
static void draw_mixer(void) {
    tft.fillRect(R_CENTER.x, R_CENTER.y, R_CENTER.w + R_PANEL.w, R_CENTER.h, C_BG);

    setFontUiLarge();

    for (int i = 0; i < TRACK_COUNT; i++) {
        int sx = R_SLIDERS[i].x;
        int sy = R_SLIDERS[i].y;
        int sw = R_SLIDERS[i].w;
        int sh = R_SLIDERS[i].h;

        int filled = (int)(ui.snapshot.voiceGain[i] * sh);
        bool mut = ui.snapshot.trackMutes[i];

        // Track background
        tft.fillRect(sx, sy, sw, sh, C_PANEL);

        // Filled level
        uint16_t col = TRACK_COLORS[i];
        uint16_t fillCol = mut ? blend16(col, C_BG, 40) : col;
        tft.fillRect(sx, sy + sh - filled, sw, filled, fillCol);

        // Border
        tft.drawRect(sx, sy, sw, sh, C_STROKE);

        // Label below
        tft.setTextColor(col, C_BG);
        tft.setTextDatum(MC_DATUM);
        char lbl[2] = {TRACK_CHARS[i], '\0'};
        tft.drawString(lbl, sx + sw / 2, sy + sh + 12);
    }
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
    if (ui.mode == UiMode::MIXER) return; // Mixer drawn separately

    uint16_t tc = TRACK_COLORS[ui.snapshot.activeTrack];

    s_panel.fillSprite(C_BG);
    s_panel.drawFastVLine(0, 0, PANEL_H, C_DIVIDER);

    for (int row = 0; row < 4; row++) {
        ParamMeta m;
        param_meta(row, &m);
        if (m.label[0] == '\0') continue;

        const ParamRow *pr = &PARAM_ROWS[row];

        // Focus visual outline (could map to last touched param)
        bool focused = (ui.activeHoldParam == row);

        /* Label */
        setFontUiSmall(&s_panel);
        s_panel.setTextColor(focused ? tc : C_TEXT_DIM, C_BG);
        s_panel.setTextDatum(TL_DATUM);
        s_panel.drawString(m.label, pr->minus.x, pr->touch.y + 2);

        /* − value + */
        setFontValueSmall(&s_panel);
        draw_btn(&s_panel, &pr->minus, "-", C_PANEL_2, C_STROKE, C_TEXT);

        // Geometric specific fonts for + and - could be tweaked here if we wanted pure geo,
        // but text labels work well and are robust.

        // Value Box
        draw_btn(&s_panel, &pr->value, m.value, C_PANEL_2, focused ? tc : C_STROKE, C_TEXT);

        setFontValueSmall(&s_panel);
        draw_btn(&s_panel, &pr->plus,  "+", C_PANEL_2, C_STROKE, C_TEXT);

        /* Mini progress bar */
        int bw = (int)(m.norm * (float)(pr->value.w - 4));
        s_panel.fillRect(pr->value.x + 2, pr->value.y - 4, pr->value.w - 4, 3, C_PANEL);
        if (bw > 0) {
            s_panel.fillRect(pr->value.x + 2, pr->value.y - 4, bw, 3, tc);
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
    if (tp.justReleased) {
        hold_stop();
        ui.touchPressedMs = 0;
        ui.touchLongTriggered = false;
        return;
    }

    int tx = tp.x, ty = tp.y;

    if (tp.justPressed) {
        ui.touchPressedMs = millis();
        ui.touchPressedX = tx;
        ui.touchPressedY = ty;
        ui.touchLongTriggered = false;

        /* ── Header (y < 40) ────────────────────────────────────────────────── */
        if (ty < 40) {
            if (R_PLAY.contains(tx, ty))  { postUiAction(UiActionType::TOGGLE_PLAY, 0, 0); return; }

            if (R_SLOT_BOX.contains(tx, ty)) {
                // Split the box hit test logic, left half dec, right half inc
                if (tx < R_SLOT_BOX.x + R_SLOT_BOX.w / 2) {
                    ui.activeSlot = (ui.activeSlot == 0) ? (CYDConfig::PatternSlots - 1) : (ui.activeSlot - 1);
                } else {
                    ui.activeSlot = (ui.activeSlot + 1) % CYDConfig::PatternSlots;
                }
                return;
            }

            if (R_SAVE.contains(tx, ty))  {
                set_status(PatternStore.saveSlot(ui.activeSlot) ? "Saved!" : "Save Fail");
                return;
            }

            if (R_LOAD.contains(tx, ty))  {
                set_status(PatternStore.loadSlot(ui.activeSlot) ? "Loaded!" : "Load Fail");
                return;
            }

            if (R_BPM_BOX.contains(tx, ty)) {
                // Split the BPM box hit test logic, left half dec, right half inc
                if (tx < R_BPM_BOX.x + R_BPM_BOX.w / 2) {
                    postUiAction(UiActionType::NUDGE_BPM, 0, -1);
                    hold_start(10, -1);
                } else {
                    postUiAction(UiActionType::NUDGE_BPM, 0, +1);
                    hold_start(10, +1);
                }
                return;
            }
            return;
        }

        /* ── Track selector (In R_TRACKS_AREA) ─────────────────────────────── */
        if (R_TRACKS_AREA.contains(tx, ty)) {
            for (int i = 0; i < TRACK_COUNT; i++) {
                if (R_TRACKS[i].contains(tx, ty)) {
                    // Right side of button is the mute badge
                    if (tx > R_TRACKS[i].x + R_TRACKS[i].w - 24) {
                         postUiAction(UiActionType::TOGGLE_MUTE, 0, i);
                         ui.touchLongTriggered = true; // Prevent long-press from toggling it again
                    } else {
                         postUiAction(UiActionType::SELECT_TRACK, 0, i);
                    }
                    return;
                }
            }
            return;
        }

        /* ── Tabs (y >= 204) ────────────────────────────────────────── */
        if (R_TABS.contains(tx, ty)) {
            if (R_TAB_SEQ.contains(tx, ty)) { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::PATTERN_EDIT); ui.mode = UiMode::PATTERN_EDIT; return; }
            if (R_TAB_SOUND.contains(tx, ty)) { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::SOUND_EDIT);  ui.mode = UiMode::SOUND_EDIT;  return; }
            if (R_TAB_MIX.contains(tx, ty)) { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::MIXER);       ui.mode = UiMode::MIXER;        return; }
            return;
        }

        /* ── Center Area ───────────────────────────────────────────────────── */
        if (R_CENTER.contains(tx, ty)) {
             // Future interactive logic for center area goes here
        }

        /* ── Param panel (x >= PANEL_SX, not mixer) ────────────────────────────── */
        if (ui.mode != UiMode::MIXER && R_PANEL.contains(tx, ty)) {
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

    // Drag / Move / Continued press interactions
    if (tp.pressed) {
        /* ── Mixer sliders (Mix mode, anywhere in CENTER+PANEL) ────────────── */
        if (ui.mode == UiMode::MIXER && (R_CENTER.contains(tx, ty) || R_PANEL.contains(tx, ty))) {
            for (int i = 0; i < TRACK_COUNT; i++) {
                if (tx >= R_SLIDERS[i].x - 10 && tx <= R_SLIDERS[i].x + R_SLIDERS[i].w + 10) { // relaxed X bounds
                    float norm = 1.0f - (float)(ty - R_SLIDERS[i].y) / R_SLIDERS[i].h;
                    norm = norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);
                    postUiAction(UiActionType::SET_VOICE_GAIN, i, (int)(norm * 100));
                    return;
                }
            }
        }

        /* ── Long press detection ──────────────────────────────────────────── */
        if (!ui.touchLongTriggered && ui.touchPressedMs > 0 && millis() - ui.touchPressedMs > 500) {
            int dx = tx - ui.touchPressedX;
            int dy = ty - ui.touchPressedY;
            if (dx*dx + dy*dy < 400) { // within ~20px radius
                ui.touchLongTriggered = true;

                // Track long press -> Mute
                if (R_TRACKS_AREA.contains(tx, ty)) {
                    for (int i = 0; i < TRACK_COUNT; i++) {
                        if (R_TRACKS[i].contains(tx, ty)) {
                            postUiAction(UiActionType::TOGGLE_MUTE, 0, i);
                            break;
                        }
                    }
                }
            }
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
  tft.setFont(&fonts::FreeMonoBold9pt7b); // Bold mono for legibility at small sizes

  /* ── Create sprites ────────────────────────────────────────────────────── */
  s_ring.createSprite(R_CENTER.w, R_CENTER.h);
  s_ring.setColorDepth(16);
  s_panel.createSprite(R_PANEL.w, R_PANEL.h);
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
          ui.forceRedraw = true;
          ui.dirtyStatus = true;
      }

      /* Touch */
      InputMgr.update();
      handleTouch(InputMgr.state());
      hold_tick();

      /* ── Decide what to redraw ─────────────────────────────────────────── */
      if (ui.snapshot.bpm != ui.lastSnapshot.bpm ||
          ui.snapshot.isPlaying != ui.lastSnapshot.isPlaying ||
          ui.activeSlot != ui.lastActiveSlot) {
          ui.dirtyHeader = true;
      }

      if (strcmp(ui.status, ui.lastStatus) != 0 || ui.snapshot.isPlaying != ui.lastSnapshot.isPlaying) {
          ui.dirtyStatus = true;
      }

      if (ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack ||
          memcmp(ui.snapshot.trackMutes, ui.lastSnapshot.trackMutes, sizeof(ui.snapshot.trackMutes)) != 0) {
          ui.dirtyTracks = true;
          if (ui.mode == UiMode::MIXER) ui.dirtyCenter = true;
      }

      if (ui.mode != ui.lastMode) {
          ui.dirtyTabs = true;
          ui.dirtyCenter = true;
          ui.dirtyPanel = true;
          ui.forceRedraw = true; // when changing mode, full refresh is safer
      }

      bool params_dirty = ui.activeHoldParam != -1 ||
          memcmp(ui.snapshot.trackSteps, ui.lastSnapshot.trackSteps, sizeof(ui.snapshot.trackSteps)) != 0 ||
          memcmp(ui.snapshot.trackHits, ui.lastSnapshot.trackHits, sizeof(ui.snapshot.trackHits)) != 0 ||
          memcmp(ui.snapshot.trackRotations, ui.lastSnapshot.trackRotations, sizeof(ui.snapshot.trackRotations)) != 0 ||
          memcmp(ui.snapshot.voiceParams, ui.lastSnapshot.voiceParams, sizeof(ui.snapshot.voiceParams)) != 0 ||
          memcmp(ui.snapshot.voiceGain, ui.lastSnapshot.voiceGain, sizeof(ui.snapshot.voiceGain)) != 0 ||
          memcmp(&ui.snapshot.bassParams, &ui.lastSnapshot.bassParams, sizeof(BassGrooveParams)) != 0 ||
          ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack;

      if (params_dirty) {
          ui.dirtyPanel = true;
          ui.dirtyCenter = true; // Mixer view depends on params/gains
      }

      bool step_change = (ui.snapshot.currentStep != ui.lastSnapshot.currentStep);
      bool pattern_change =
          memcmp(ui.snapshot.patterns, ui.lastSnapshot.patterns, sizeof(ui.snapshot.patterns)) != 0  ||
          memcmp(ui.snapshot.trackSteps, ui.lastSnapshot.trackSteps, sizeof(ui.snapshot.trackSteps)) != 0;

      if (step_change || pattern_change) {
          ui.dirtyCenter = true;
      }

      /* Periodic full redraw to clear any artefacts */
      bool full = ui.forceRedraw || (now - last_full > 8000u);
      if (full) {
          ui.dirtyHeader = true;
          ui.dirtyStatus = true;
          ui.dirtyTracks = true;
          ui.dirtyTabs = true;
          ui.dirtyCenter = true;
          ui.dirtyPanel = true;
          tft.fillScreen(C_BG);
          last_full = now;
          ui.forceRedraw = false;
      }

      tft.startWrite();

      if (ui.dirtyHeader) { draw_header(); ui.dirtyHeader = false; }
      if (ui.dirtyStatus) { draw_status(); ui.dirtyStatus = false; }
      if (ui.dirtyTracks) { draw_tracks(); ui.dirtyTracks = false; }
      if (ui.dirtyTabs)   { draw_tabs();   ui.dirtyTabs = false; }

      if (ui.mode == UiMode::MIXER) {
          if (ui.dirtyCenter || ui.dirtyPanel) {
              draw_mixer();
              ui.dirtyCenter = false;
              ui.dirtyPanel = false;
          }
      } else {
          if (ui.dirtyPanel) { draw_params(); ui.dirtyPanel = false; }
          // Center draws via Sprite every frame tick it's dirty, handled below
      }

      tft.endWrite();

      /* Ring: redraw every step tick or if pattern changed (via dirty flag) */
      if (ui.mode != UiMode::MIXER && ui.dirtyCenter) {
          if (now - last_ring >= 33u) {   /* cap ~30 fps for ring */
              draw_center();
              last_ring = now;
              ui.dirtyCenter = false;
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
