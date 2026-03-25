import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()

content = content.replace('snprintf(center_buf, sizeof(center_buf), "%c - %d/16 - ROT %d", TRACK_CHARS[tr], steps, rot);',
'int hits = ui.snapshot.trackHits[tr];\n    snprintf(center_buf, sizeof(center_buf), "%c - %d/%d - ROT %d", TRACK_CHARS[tr], hits, steps, rot);')

content = content.replace('snprintf(right_buf, sizeof(right_buf), "step %d/16", (ui.snapshot.currentStep % 16) + 1);',
'snprintf(right_buf, sizeof(right_buf), "step %d/%d", steps > 0 ? (ui.snapshot.currentStep % steps) + 1 : 1, steps);')

with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
