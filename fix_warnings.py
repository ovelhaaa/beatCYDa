import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()

# I used '\\0' which became ' ' due to some weird python parsing. I should use '\0'
content = content.replace("'\\0'", "'\\0'") # In python raw string it will be \0
content = content.replace("m.label[0] == '\0'", "m.label[0] == '\\0'")
content = content.replace("char lbl[2] = {TRACK_CHARS[i], '\0'};", "char lbl[2] = {TRACK_CHARS[i], '\\0'};")

with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
