import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()

# remove duplicate arrays
content = re.sub(
    r"static const char TRACK_CHARS\[TRACK_COUNT\] = \{'K','S','C','O','B'\};\nstatic const char \* const TRACK_LABELS\[TRACK_COUNT\] = \{\"KICK\",\"SNRE\",\"HATC\",\"HATO\",\"BASS\"\};",
    "",
    content,
    count=1
)

with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
