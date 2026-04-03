import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()

colors_block = """/* ═══════════════════════════════════════════════════════════════════════════════
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

static const uint16_t TRACK_COLORS[TRACK_COUNT] = {
    RGB565(0xFF, 0x6A, 0x00),   /* K - Kick */
    RGB565(0x00, 0xCC, 0x66),   /* S - Snare */
    RGB565(0x00, 0xCC, 0xFF),   /* C - Clap */
    RGB565(0xCC, 0x44, 0xFF),   /* O - OpenHH */
    RGB565(0xFF, 0xCC, 0x00),   /* B - Bass */
};

static const char TRACK_CHARS[TRACK_COUNT] = {'K','S','C','O','B'};
static const char * const TRACK_LABELS[TRACK_COUNT] = {"KICK","SNRE","CLAP","HATO","BASS"};
"""

content = re.sub(
    r'/\* ═══════════════════════════════════════════════════════════════════════════════\n   COLOUR PALETTE.*?};',
    colors_block,
    content,
    flags=re.DOTALL
)

# Replace font initialization
content = content.replace('tft.setFont(&fonts::FreeMonoBold9pt7b); // Bold mono for legibility at small sizes', 'tft.setTextSize(1); // Default 6x8 font')

# Write back
with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
