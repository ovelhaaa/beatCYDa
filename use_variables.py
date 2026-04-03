import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()

content = content.replace(
    's_ring.createSprite(50, 50);',
    's_ring.createSprite(R_RING.w, R_RING.h);'
)

content = content.replace(
    's_panel.createSprite(258, 174);',
    's_panel.createSprite(PANEL_W, PANEL_H);'
)

with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
