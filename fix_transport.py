import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()

# I see the second duplication wasn't fully removed in the file, and old SLOT constants were referenced.

old_structs_2 = """/* Transport bar */
static const Rect R_PLAY      = { 4,   8,  62,  30};
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
"""
content = content.replace(old_structs_2, "")

# Add missing aliases back until we rewrite draw_transport and handleTouch
content = content.replace("static const Rect R_SLOT_1    = {116,  6,  18,  18};", """static const Rect R_SLOT_DEC  = {80,   8,  20,  30};
static const Rect R_SLOT_VAL  = {104,  8,  28,  30};
static const Rect R_SLOT_INC  = {136,  8,  20,  30};
static const Rect R_SLOT_1    = {116,  6,  18,  18};""")

# Add R_MUTE, R_VOICE, R_MIX as old ones were removed above
content = content.replace("static const Rect R_FOOTER = {62, 224, 258, 16};", """static const Rect R_FOOTER = {62, 224, 258, 16};
/* Bottom action row */
static const Rect R_MUTE  = { 36, 208, 52, 26};
static const Rect R_VOICE = { 88, 208, 52, 26};
static const Rect R_MIX   = {140, 208, 52, 26};
""")


with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
