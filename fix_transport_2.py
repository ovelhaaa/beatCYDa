import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()

# I need to reliably remove the second duplicate block of rects.

start_str = "/* Transport bar */\nstatic const Rect R_PLAY      = { 4,   8,  62,  30};\n"
end_str = "};\n"

start_idx = content.find(start_str)
if start_idx != -1:
    end_idx = content.find(end_str, content.find("R_SLIDERS", start_idx)) + len(end_str)
    content = content[:start_idx] + content[end_idx:]

with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
