import math
import os

TRACK_COUNT = 5
kBassTrackIndex = 4

def rgb565_to_hex(val):
    r = ((val >> 11) & 0x1F) * 255 // 31
    g = ((val >> 5) & 0x3F) * 255 // 63
    b = (val & 0x1F) * 255 // 31
    return f"#{r:02x}{g:02x}{b:02x}"

def dim_color(hex_col, factor):
    r = int(int(hex_col[1:3], 16) * factor)
    g = int(int(hex_col[3:5], 16) * factor)
    b = int(int(hex_col[5:7], 16) * factor)
    return f"#{r:02x}{g:02x}{b:02x}"

Bg = "#000000"
Surface = rgb565_to_hex(0x18C3)
SurfacePressed = rgb565_to_hex(0x10E2)
Outline = rgb565_to_hex(0x39E7)
TextPrimary = rgb565_to_hex(0xFFFF)
TextSecondary = rgb565_to_hex(0xBDF7)
TextOnAccent = rgb565_to_hex(0xFFFF)
TextOnDanger = rgb565_to_hex(0xFFFF)
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

track_labels = ["KICK", "SNRE", "HATC", "HATO", "BASS"]

def generate_svg_header(width, height):
    return f'''<svg width="{width}" height="{height}" viewBox="0 0 {width} {height}" xmlns="http://www.w3.org/2000/svg">
<style>
    .bg {{ fill: {Bg}; }}
    .surface {{ fill: {Surface}; }}
    .surface-pressed {{ fill: {SurfacePressed}; }}
    .outline {{ stroke: {Outline}; fill: none; stroke-width: 1; }}
    .outline-thick {{ stroke: {Outline}; fill: none; stroke-width: 2; }}
    .text-primary {{ fill: {TextPrimary}; font-family: monospace; font-size: 14px; font-weight: bold; }}
    .text-secondary {{ fill: {TextSecondary}; font-family: monospace; font-size: 12px; }}
    .text-caption {{ fill: {TextPrimary}; font-family: monospace; font-size: 10px; }}
    .accent {{ fill: {Accent}; }}
    .danger {{ fill: {Danger}; }}
    .transport {{ fill: {TransportActive}; }}
</style>
<rect width="100%" height="100%" class="bg"/>
'''

def rect(x, y, w, h, rx, classes, fill_override=None, stroke_override=None):
    style = ""
    if fill_override: style += f"fill: {fill_override}; "
    if stroke_override: style += f"stroke: {stroke_override}; "
    style_attr = f'style="{style}"' if style else ''
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" class="{classes}" {style_attr}/>\n'

def text(x, y, content, classes, align="start", fill_override=None):
    style = f'style="fill: {fill_override};"' if fill_override else ''
    return f'<text x="{x}" y="{y}" class="{classes}" text-anchor="{align}" {style}>{content}</text>\n'

def button(x, y, w, h, label, variant="secondary"):
    fill_class = "surface"
    text_class = "text-primary"
    if variant == "primary":
        fill_class = "accent"
    elif variant == "danger":
        fill_class = "danger"
    elif variant == "transport":
        fill_class = "transport"
        text_class = "text-primary text-on-transport"

    out = rect(x, y, w, h, 4, f"{fill_class} outline")
    out += text(x + w/2, y + h/2 + 5, label, text_class, "middle")
    return out

def chip(x, y, w, h, label, active=False, track_idx=-1):
    fill_class = "surface"
    text_class = "text-primary"
    fill_ov = None
    stroke_ov = None

    if active:
        if track_idx >= 0:
            stroke_ov = track_colors[track_idx]
            fill_ov = dim_color(track_colors[track_idx], 0.3)
        else:
            fill_class = "accent"

    out = rect(x, y, w, h, 4, f"{fill_class} outline", fill_ov, stroke_ov)
    text_fill = track_colors[track_idx] if (track_idx >= 0 and not active) else None
    out += text(x + w/2, y + h/2 + 5, label, text_class, "middle", text_fill)
    return out

def slider_horizontal(x, y, w, h, val_pct, color, label, val_text):
    out = rect(x, y, w, h, 4, "surface outline")
    fill_w = int((w - 4) * val_pct)
    if fill_w > 0:
        out += rect(x + 2, y + 2, fill_w, h - 4, 2, "", color)
    out += text(x + 8, y + h/2 + 4, label, "text-primary")
    out += text(x + w - 8, y + h/2 + 4, val_text, "text-primary", "end")
    return out

def slider_vertical(x, y, w, h, val_pct, color, label):
    out = rect(x, y, w, h, 4, "surface outline")
    fill_h = int((h - 4) * val_pct)
    if fill_h > 0:
        out += rect(x + 2, y + h - 2 - fill_h, w - 4, fill_h, 2, "", color)
    out += text(x + w/2, y + h + 14, label, "text-caption", "middle")
    return out

def bottom_nav(active_idx):
    out = ""
    labels = ["PERF", "PATT", "SOUND", "MIX", "PROJ"]
    w = 62
    h = 32
    start_x = (320 - (5*w)) / 2
    y = 240 - 40 + 4

    for i, label in enumerate(labels):
        x = start_x + i * w
        variant = "primary" if i == active_idx else "secondary"
        out += button(x, y, w, h, label, variant)
    return out

def top_bar(playing=True, bpm=120):
    out = ""
    out += button(16, 6, 160, 20, "PLAYING" if playing else "STOPPED", "transport" if playing else "danger")
    out += text(188, 20, f"BPM: {bpm}", "text-primary")
    return out

def generate_perform_screen(filename):
    svg = generate_svg_header(320, 240)
    svg += top_bar(playing=True)
    svg += rect(8, 40, 186, 154, 8, "surface outline")
    svg += f'<image href="euclid_perform_compact.svg" x="14" y="46" width="174" height="142" />\n'
    svg += button(202, 42, 110, 30, "STOP", "danger")
    svg += button(202, 76, 110, 30, "MUTE", "secondary")
    svg += button(202, 110, 30, 30, "-")
    svg += rect(234, 110, 46, 30, 4, "surface-pressed outline")
    svg += text(257, 130, "120", "text-primary", "middle")
    svg += button(282, 110, 30, 30, "+")
    svg += button(202, 146, 30, 30, "<")
    svg += chip(234, 146, 46, 30, "HATC", True, 2)
    svg += button(282, 146, 30, 30, ">")
    svg += bottom_nav(0)
    svg += "</svg>"
    with open(filename, 'w') as f: f.write(svg)

def generate_pattern_screen(filename):
    svg = generate_svg_header(320, 240)
    svg += top_bar(playing=False)

    # Track selector
    svg += button(8, 40, 36, 28, "<")
    svg += chip(48, 40, 88, 28, "KICK 1/5", True, 0)
    svg += button(140, 40, 36, 28, ">")

    # Tools
    svg += button(184, 40, 128, 28, "TOOLS")

    # Rows
    y_start = 74
    svg += slider_horizontal(8, y_start, 168, 28, 16/64, track_colors[0], "STEPS", "16")
    svg += slider_horizontal(8, y_start+32, 168, 28, 4/16, track_colors[0], "HITS", "4")
    svg += slider_horizontal(8, y_start+64, 168, 28, 0, track_colors[0], "ROTATE", "0")

    # Embedded Euclid Jam
    svg += rect(184, 74, 124, 124, 8, "surface outline")
    svg += f'<image href="euclid_jam_active.svg" x="184" y="74" width="124" height="124" preserveAspectRatio="xMidYMid slice"/>\n'

    svg += bottom_nav(1)
    svg += "</svg>"
    with open(filename, 'w') as f: f.write(svg)

def generate_sound_screen(filename):
    svg = generate_svg_header(320, 240)
    svg += top_bar()

    # Track chips
    for i in range(5):
        svg += chip(12 + i*60, 42, 56, 32, track_labels[i], i==1, i)

    y_start = 82
    svg += slider_horizontal(12, y_start, 296, 32, 0.4, track_colors[1], "PITCH", "40%")
    svg += slider_horizontal(12, y_start+38, 296, 32, 0.6, track_colors[1], "DECAY", "60%")
    svg += slider_horizontal(12, y_start+76, 296, 32, 0.2, track_colors[1], "TONE", "20%")

    svg += bottom_nav(2)
    svg += "</svg>"
    with open(filename, 'w') as f: f.write(svg)

def generate_mix_screen(filename):
    svg = generate_svg_header(320, 240)
    svg += top_bar()

    # Master card
    svg += rect(12, 46, 120, 48, 8, "surface outline")
    svg += text(72, 70, "MASTER 85%", "text-primary", "middle")

    # Faders
    for i in range(5):
        val = [0.8, 0.6, 0.7, 0.4, 0.9][i]
        svg += slider_vertical(20 + i*60, 98, 40, 92, val, track_colors[i], track_labels[i])

    svg += bottom_nav(3)
    svg += "</svg>"
    with open(filename, 'w') as f: f.write(svg)

def generate_project_screen(filename):
    svg = generate_svg_header(320, 240)
    svg += top_bar()

    svg += chip(12, 42, 96, 24, "PROJECT", True)
    svg += text(116, 58, "SLOT 01 - DEFAULT", "text-primary")

    # Slots grid
    for row in range(3):
        for col in range(4):
            idx = row*4 + col
            x = 12 + col*76
            y = 74 + row*32
            label = f"{idx+1:02d}"
            variant = "primary" if idx == 0 else "secondary"
            svg += button(x, y, 70, 24, label, variant)

    # Actions
    svg += button(12, 172, 98, 24, "SAVE")
    svg += button(117, 172, 98, 24, "LOAD")
    svg += button(222, 172, 86, 24, "CLEAR", "danger")

    svg += bottom_nav(4)
    svg += "</svg>"
    with open(filename, 'w') as f: f.write(svg)


os.makedirs('docs/mockups', exist_ok=True)
generate_perform_screen('docs/mockups/screen_perform.svg')
generate_pattern_screen('docs/mockups/screen_pattern.svg')
generate_sound_screen('docs/mockups/screen_sound.svg')
generate_mix_screen('docs/mockups/screen_mix.svg')
generate_project_screen('docs/mockups/screen_project.svg')
