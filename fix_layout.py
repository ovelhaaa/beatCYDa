import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()

layout_block = """/* ═══════════════════════════════════════════════════════════════════════════════
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

/* ── Old legacy structs, keeping to compile ────────────────────────────────── */
struct ParamRow {
    Rect minus;
    Rect value;
    Rect plus;
    Rect touch;
};
#define PANEL_SX 62
#define PANEL_SY 50
#define PANEL_W  258
#define PANEL_H  174

static const ParamRow PARAM_ROWS[4] = {
    {{ 8, 18, 28, 26}, {40, 18, 52, 26}, { 96, 18, 28, 26}, {0,  4, PANEL_W, 42}},
    {{ 8, 68, 28, 26}, {40, 68, 52, 26}, { 96, 68, 28, 26}, {0, 54, PANEL_W, 42}},
    {{ 8,118, 28, 26}, {40,118, 52, 26}, { 96,118, 28, 26}, {0,104, PANEL_W, 42}},
    {{ 8,142, 28, 26}, {40,142, 52, 26}, { 96,142, 28, 26}, {0,130, PANEL_W, 38}},
};

static const Rect R_SLIDERS[TRACK_COUNT] = {
    {201, 54, 12, 126},
    {224, 54, 12, 126},
    {247, 54, 12, 126},
    {270, 54, 12, 126},
    {293, 54, 12, 126},
};
"""

content = re.sub(
    r'/\* ═══════════════════════════════════════════════════════════════════════════════\n   LAYOUT.*?};',
    layout_block,
    content,
    flags=re.DOTALL
)

# Update sprite sizes for the new layout
content = content.replace("s_ring.createSprite(R_RING.w, R_RING.h);", "s_ring.createSprite(50, 50);")
content = content.replace("s_panel.createSprite(PANEL_W, PANEL_H);", "s_panel.createSprite(258, 174);")
content = content.replace("static LGFX_Sprite s_ring(&tft);   /* 140 × 140 — euclidean ring area  */", "static LGFX_Sprite s_ring(&tft);   /* 50 × 50 — euclidean ring area  */")
content = content.replace("static LGFX_Sprite s_panel(&tft);  /* 132 × 168 — right parameter panel */", "static LGFX_Sprite s_panel(&tft);  /* 258 × 174 — tab content panel */")


# Remove status text drawing logic which used R_BPM_VAL for overlay, since status logic changes in the footer
content = content.replace("""static void draw_status(void) {
    bool active = (ui.statusUntilMs > millis());
    const char *msg = active ? ui.status
                     : ui.snapshot.isPlaying
                         ? "PLAYING"
                         : "READY";
    draw_btn(NULL, &R_BPM_VAL, msg, C_PANEL2, active ? C_ACCENT : C_DIM,
             active ? (uint16_t)C_PLAY : (uint16_t)C_TEXT);
}""", "static void draw_status(void) {} // Deprecated, drawn in footer")

with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
