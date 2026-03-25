import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()

# Instead of blindly replacing all colors, I will define aliases to keep the code compiling
# before I start replacing the UI components in later steps.
# I'll add the missing color aliases that the old code expects.

alias_block = """
/* Legacy aliases for old UI code, to be removed later */
constexpr uint16_t C_PANEL    = C_PANEL_BG;
constexpr uint16_t C_PANEL2   = C_FILL_BTN;
constexpr uint16_t C_DIM      = C_TXT_DIM;
constexpr uint16_t C_TEXT     = C_TXT_SEC;
constexpr uint16_t C_SELECT   = C_ACCENT;
constexpr uint16_t C_EXPR     = C_ACCENT;
constexpr uint16_t C_PLAY     = C_TXT_STAT;
"""

content = content.replace("static const uint16_t TRACK_COLORS[TRACK_COUNT]", alias_block + "\nstatic const uint16_t TRACK_COLORS[TRACK_COUNT]")

with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
