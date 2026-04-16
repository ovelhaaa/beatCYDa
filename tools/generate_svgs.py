import math
import os

TRACK_COUNT = 5
kBassTrackIndex = 4

colors = [
    "#caa9", "#d568", "#4f1f", "#fa74", "#5d6c" # Need to convert from RGB565 to hex
]

# 0xCAA9: R=11001 (25), G=010101 (21), B=01001 (9) -> 200, 85, 74
# 0xD568: R=11010 (26), G=101011 (43), B=01000 (8) -> 208, 172, 66
# 0x4F1F: R=01001 (9), G=111000 (56), B=11111 (31)-> 74, 226, 255
# 0xFA74: R=11111 (31), G=010011 (19), B=10100 (20)-> 255, 76, 165
# 0x5D6C: R=01011 (11), G=101011 (43), B=01100 (12)-> 90, 172, 99

def rgb565_to_hex(val):
    r = ((val >> 11) & 0x1F) * 255 // 31
    g = ((val >> 5) & 0x3F) * 255 // 63
    b = (val & 0x1F) * 255 // 31
    return f"#{r:02x}{g:02x}{b:02x}"

track_colors = [
    rgb565_to_hex(0xCAA9),
    rgb565_to_hex(0xD568),
    rgb565_to_hex(0x4F1F),
    rgb565_to_hex(0xFA74),
    rgb565_to_hex(0x5D6C)
]

def dim_color(hex_col, factor):
    r = int(int(hex_col[1:3], 16) * factor)
    g = int(int(hex_col[3:5], 16) * factor)
    b = int(int(hex_col[5:7], 16) * factor)
    return f"#{r:02x}{g:02x}{b:02x}"

def halve_color(hex_col):
    r = int(int(hex_col[1:3], 16) // 2)
    g = int(int(hex_col[3:5], 16) // 2)
    b = int(int(hex_col[5:7], 16) // 2)
    return f"#{r:02x}{g:02x}{b:02x}"

def boost_luma(hex_col, amt):
    r = min(255, int(int(hex_col[1:3], 16) + amt * 255))
    g = min(255, int(int(hex_col[3:5], 16) + amt * 255))
    b = min(255, int(int(hex_col[5:7], 16) + amt * 255))
    return f"#{r:02x}{g:02x}{b:02x}"

def bjorklund(steps, pulses):
    steps = int(steps)
    pulses = int(pulses)
    if pulses == 0:
        return [0] * steps
    if pulses > steps:
    pattern = []
    counts = []
    remainders = []
    divisor = steps - pulses
    remainders.append(pulses)
    level = 0
    while True:
        counts.append(divisor // remainders[level])
        remainders.append(divisor % remainders[level])
        divisor = remainders[level]
        level += 1
        if remainders[level] <= 1:
            break
    counts.append(divisor)

    def build(level):
        if level == -1:
            pattern.append(0)
        elif level == -2:
            pattern.append(1)
        else:
            for i in range(counts[level]):
                build(level - 1)
            if remainders[level] != 0:
                build(level - 2)

    build(level)
    pattern = list(reversed(pattern))
    idx = pattern.index(1)
    pattern = pattern[idx:] + pattern[:idx]
    return pattern

def generate_svg(filename, active_track, current_step, pattern_lens, pattern_hits, track_mutes, bass_env=0.0, compact=False):
    width = 174 if compact else 320
    height = 142 if compact else 240
    cx = width / 2
    cy = height / 2

    svg = f'<svg width="{width}" height="{height}" viewBox="0 0 {width} {height}" xmlns="http://www.w3.org/2000/svg">\n'
    svg += f'<rect width="100%" height="100%" fill="#000000"/>\n'

    tri_dist_outer = 12 if compact else 16
    tri_dist_inner = 4 if compact else 7
    tri_half_width = 2 if compact else 3
    svg += f'<polygon points="{cx - tri_half_width},{cy - tri_dist_outer} {cx + tri_half_width},{cy - tri_dist_outer} {cx},{cy - tri_dist_inner}" fill="{rgb565_to_hex(0xAD55)}"/>\n'

    def ring_radius(track):
        # Based on R_RING max and min
        # For compact: 140x140 -> rad max ~ 70
        # In code: RING_RADII = {58, 48, 38, 28, 18}
        # UiEuclideanRings::ringRadiusForTrack calculates minR + (span * t)
        # where span = min(w,h)/2 - 12 - minR, minR = 18
        # Let's use the code's math.
        minR = 18.0
        span = min(width, height) / 2.0 - 12.0 - minR
        t = (TRACK_COUNT - 1 - track) / (TRACK_COUNT - 1)
        return minR + span * t

    for track in range(TRACK_COUNT):
        if track == kBassTrackIndex: continue

        active = (track == active_track)
        muted = track_mutes[track]

        base_color = track_colors[track]
        dim = 1.0 if active else 0.7
        if muted: dim *= 0.45

        ring_color = dim_color(base_color, dim * 0.65)
        hit_color = dim_color(base_color, dim * (1.0 if active else 0.8))
        no_hit_color = dim_color(base_color, dim * (0.7 if active else 0.55))
        muted_hit_color = dim_color(base_color, dim * 0.55)
        line_color = halve_color(hit_color)
        play_halo_base = dim_color(base_color, dim * (0.75 if active else 0.6))
        play_halo_color = play_halo_base if active else boost_luma(play_halo_base, 0.2)

        l = pattern_lens[track]
        hits = pattern_hits[track]
        pat = bjorklund(l, hits)
        play_step = current_step % l
        radius = ring_radius(track)

        svg += f'<circle cx="{cx}" cy="{cy}" r="{radius}" fill="none" stroke="{ring_color}" stroke-width="1"/>\n'

        hit_coords = []
        for step in range(l):
            turns = step / l
            deg = -90.0 + (turns * 360.0)
            rad = deg * math.pi / 180.0
            x = cx + math.cos(rad) * radius
            y = cy + math.sin(rad) * radius
            is_play_step = (step == play_step)
            is_hit = pat[step]

            svg += f'<circle cx="{x}" cy="{y}" r="2" fill="{rgb565_to_hex(0x2104)}"/>\n'

            if is_hit:
                hit_coords.append((x,y))
                if muted:
                    svg += f'<circle cx="{x}" cy="{y}" r="4" fill="none" stroke="{muted_hit_color}" stroke-width="1"/>\n'
                else:
                    svg += f'<circle cx="{x}" cy="{y}" r="4" fill="{hit_color}"/>\n'
            else:
                svg += f'<circle cx="{x}" cy="{y}" r="3" fill="none" stroke="{no_hit_color}" stroke-width="1"/>\n'

            if is_play_step:
                halo_radius = 5 if compact else 7
                svg += f'<circle cx="{x}" cy="{y}" r="{halo_radius}" fill="{play_halo_color}" opacity="0.6"/>\n'
                if muted:
                    svg += f'<circle cx="{x}" cy="{y}" r="4" fill="none" stroke="{muted_hit_color}" stroke-width="1"/>\n'
                    svg += f'<circle cx="{x}" cy="{y}" r="2" fill="none" stroke="{muted_hit_color}" stroke-width="1"/>\n'
                elif is_hit:
                    svg += f'<circle cx="{x}" cy="{y}" r="4" fill="{hit_color}"/>\n'
                else:
                    svg += f'<circle cx="{x}" cy="{y}" r="4" fill="none" stroke="{no_hit_color}" stroke-width="1"/>\n'

        if len(hit_coords) >= 2:
            pts = " ".join([f"{x},{y}" for x, y in hit_coords]) + f" {hit_coords[0][0]},{hit_coords[0][1]}"
            svg += f'<polyline points="{pts}" fill="none" stroke="{line_color}" stroke-width="1"/>\n'

    bass_max_radius = 18
    svg += f'<circle cx="{cx}" cy="{cy}" r="{bass_max_radius+1}" fill="#000000"/>\n'
    bass_radius = bass_max_radius * bass_env
    if bass_radius > 0:
        svg += f'<circle cx="{cx}" cy="{cy}" r="{bass_radius}" fill="{track_colors[kBassTrackIndex]}"/>\n'

    svg += '</svg>'
    with open(filename, 'w') as f:
        f.write(svg)

os.makedirs('docs/mockups', exist_ok=True)

# Generate a few scenarios
generate_svg('docs/mockups/euclid_jam_active.svg', active_track=0, current_step=5, pattern_lens=[16, 16, 12, 8, 16], pattern_hits=[4, 2, 7, 3, 4], track_mutes=[False, False, False, False, False], bass_env=0.8, compact=False)
generate_svg('docs/mockups/euclid_jam_muted.svg', active_track=1, current_step=12, pattern_lens=[16, 16, 12, 8, 16], pattern_hits=[4, 2, 7, 3, 4], track_mutes=[False, True, False, True, False], bass_env=0.2, compact=False)
generate_svg('docs/mockups/euclid_perform_compact.svg', active_track=2, current_step=3, pattern_lens=[16, 16, 12, 8, 16], pattern_hits=[4, 2, 7, 3, 4], track_mutes=[False, False, False, False, False], bass_env=0.5, compact=True)
