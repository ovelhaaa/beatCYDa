import math
import os

TRACK_COUNT = 5
kBassTrackIndex = 4

def rgb565_to_hex(val):
    r = ((val >> 11) & 0x1F) * 255 // 31
    g = ((val >> 5) & 0x3F) * 255 // 63
    b = (val & 0x1F) * 255 // 31
    return f"#{r:02x}{g:02x}{b:02x}"

# UI Theme Colors (from UiColors.h)
Bg = "#000000" # 0x0000
Surface = rgb565_to_hex(0x18C3)
SurfacePressed = rgb565_to_hex(0x10E2)
Outline = rgb565_to_hex(0x39E7)
TextPrimary = rgb565_to_hex(0xFFFF)
TextSecondary = rgb565_to_hex(0xBDF7)
TextOnAccent = rgb565_to_hex(0xFFFF)
Accent = rgb565_to_hex(0x2D7F)
Danger = rgb565_to_hex(0xF800)
TransportActive = rgb565_to_hex(0x07E0)

track_colors = [
    rgb565_to_hex(0xCAA9),
    rgb565_to_hex(0xD568),
    rgb565_to_hex(0x4F1F),
    rgb565_to_hex(0xFA74),
    rgb565_to_hex(0x5D6C)
]

def generate_svg_header(width, height):
    return f'''<svg width="{width}" height="{height}" viewBox="0 0 {width} {height}" xmlns="http://www.w3.org/2000/svg">
<style>
    .bg {{ fill: {Bg}; }}
    .surface {{ fill: {Surface}; }}
    .surface-pressed {{ fill: {SurfacePressed}; }}
    .outline {{ stroke: {Outline}; fill: none; }}
    .text-primary {{ fill: {TextPrimary}; font-family: monospace; font-size: 14px; font-weight: bold; }}
    .text-secondary {{ fill: {TextSecondary}; font-family: monospace; font-size: 12px; }}
    .accent {{ fill: {Accent}; }}
    .danger {{ fill: {Danger}; }}
    .transport {{ fill: {TransportActive}; }}
</style>
<rect width="100%" height="100%" class="bg"/>
'''

def rect(x, y, w, h, rx, classes):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" class="{classes}"/>\n'

def text(x, y, content, classes, align="start"):
    return f'<text x="{x}" y="{y}" class="{classes}" text-anchor="{align}">{content}</text>\n'

def button(x, y, w, h, label, variant="secondary"):
    fill_class = "surface"
    if variant == "primary": fill_class = "accent"
    elif variant == "danger": fill_class = "danger"
    elif variant == "transport": fill_class = "transport"

    text_class = "text-primary"
    if variant != "secondary": text_class += " text-on-accent" # Need custom class or just override fill

    out = rect(x, y, w, h, 4, f"{fill_class} outline")
    out += text(x + w/2, y + h/2 + 5, label, text_class, "middle")
    return out

def bottom_nav(active_idx):
    out = ""
    labels = ["PERF", "PATT", "SOUND", "MIX", "PROJ"]
    w = 62
    h = 32
    gap = 0
    start_x = (320 - (5*w)) / 2
    y = 240 - 40 + 4

    for i, label in enumerate(labels):
        x = start_x + i * w
        variant = "primary" if i == active_idx else "secondary"
        out += button(x, y, w, h, label, variant)
    return out

def top_bar(playing=True, bpm=120):
    out = ""
    # Transport
    out += button(16, 12, 160, 20, "PLAYING" if playing else "STOPPED", "transport" if playing else "danger")
    # BPM
    out += text(188, 27, f"BPM: {bpm}", "text-primary")
    return out

def generate_perform_screen(filename):
    svg = generate_svg_header(320, 240)
    svg += top_bar()

    # Hero card
    svg += rect(8, 40, 186, 154, 8, "surface outline")

    # Rings embedded (simulate)
    svg += f'<image href="euclid_perform_compact.svg" x="14" y="46" width="174" height="142" />\n'

    # Controls
    svg += button(202, 42, 110, 30, "STOP", "danger")
    svg += button(202, 76, 110, 30, "MUTE", "secondary")

    # BPM stepper
    svg += button(202, 110, 30, 30, "-")
    svg += rect(234, 110, 46, 30, 4, "surface-pressed outline")
    svg += text(257, 130, "120", "text-primary", "middle")
    svg += button(282, 110, 30, 30, "+")

    # Track carousel
    svg += button(202, 146, 30, 30, "<")
    svg += text(257, 166, "HATC", "text-primary", "middle")
    svg += button(282, 146, 30, 30, ">")

    svg += bottom_nav(0)
    svg += "</svg>"

    with open(filename, 'w') as f:
        f.write(svg)

os.makedirs('docs/mockups', exist_ok=True)
generate_perform_screen('docs/mockups/screen_perform.svg')
