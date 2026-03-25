import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()

# Replace draw_transport
draw_transport_new = """static void draw_transport(void) {
    char buf[8];
    tft.fillRect(0, 0, CYDConfig::ScreenWidth, 30, C_BG);

    /* Play / Stop */
    bool pl = ui.snapshot.isPlaying;
    tft.fillRoundRect(R_PLAY.x, R_PLAY.y, R_PLAY.w, R_PLAY.h, 2, pl ? blend16(C_ACCENT, C_BG, 24) : C_FILL_BTN);
    tft.drawRoundRect(R_PLAY.x, R_PLAY.y, R_PLAY.w, R_PLAY.h, 2, pl ? C_ACCENT : C_BORDER);

    tft.setTextColor(pl ? C_ACCENT_L : C_TXT_STAT, pl ? blend16(C_ACCENT, C_BG, 24) : C_FILL_BTN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(pl ? "X" : ">", R_PLAY.x + R_PLAY.w / 2, R_PLAY.y + R_PLAY.h / 2 - 1);

    /* BPM Group */
    tft.fillRoundRect(R_BPM_GROUP.x, R_BPM_GROUP.y, R_BPM_GROUP.w, R_BPM_GROUP.h, 2, blend16(C_ACCENT, C_BG, 16));
    tft.drawRoundRect(R_BPM_GROUP.x, R_BPM_GROUP.y, R_BPM_GROUP.w, R_BPM_GROUP.h, 2, blend16(C_ACCENT, C_BG, 34));

    tft.setTextColor(blend16(C_ACCENT_L, C_BG, 120), blend16(C_ACCENT, C_BG, 16));
    tft.drawString("-", R_BPM_DEC.x + R_BPM_DEC.w / 2, R_BPM_DEC.y + R_BPM_DEC.h / 2 - 1);
    tft.drawString("+", R_BPM_INC.x + R_BPM_INC.w / 2, R_BPM_INC.y + R_BPM_INC.h / 2 - 1);

    snprintf(buf, sizeof(buf), "%3d", ui.snapshot.bpm);
    tft.setTextColor(C_ACCENT_L, blend16(C_ACCENT, C_BG, 16));
    tft.drawString(buf, R_BPM_VAL.x + R_BPM_VAL.w / 2, R_BPM_VAL.y + R_BPM_VAL.h / 2 - 1);

    /* Slot Selector */
    for (int i = 0; i < 3; i++) {
        Rect r = (i == 0) ? R_SLOT_1 : (i == 1) ? R_SLOT_2 : R_SLOT_3;
        bool active = (ui.activeSlot == i);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 0, active ? blend16(C_ACCENT, C_BG, 32) : C_BG);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 0, active ? blend16(C_ACCENT, C_BG, 102) : C_BORDER_DIM);
        tft.setTextColor(active ? C_ACCENT_L : C_TXT_DIM, active ? blend16(C_ACCENT, C_BG, 32) : C_BG);
        snprintf(buf, sizeof(buf), "%d", i + 1);
        tft.drawString(buf, r.x + r.w / 2, r.y + r.h / 2 - 1);
    }

    /* Save / Load */
    tft.drawRect(R_SAVE.x, R_SAVE.y, R_SAVE.w, R_SAVE.h, C_BORDER);
    tft.setTextColor(C_TXT_STAT, C_BG);
    tft.drawString("SV", R_SAVE.x + R_SAVE.w / 2, R_SAVE.y + R_SAVE.h / 2 - 1);

    tft.drawRect(R_LOAD.x, R_LOAD.y, R_LOAD.w, R_LOAD.h, C_BORDER);
    tft.drawString("LD", R_LOAD.x + R_LOAD.w / 2, R_LOAD.y + R_LOAD.h / 2 - 1);
}
"""
content = re.sub(r'static void draw_transport\(void\) \{.*?(?=/\* ═══════════════════════════════════════════════════════════════════════════════\n   DRAW — STATUS)', draw_transport_new, content, flags=re.DOTALL)


# Replace draw_tracks
draw_tracks_new = """static void draw_tracks(void) {
    tft.fillRect(0, 30, 62, 210, C_PANEL_BG);
    tft.drawFastVLine(61, 30, 210, C_DIV);

    tft.setTextColor(C_TXT_DIM, C_PANEL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("E U C L I D", 31, 37);

    char lbl[2] = {0, 0};
    for (int i = 0; i < TRACK_COUNT; i++) {
        bool sel = (ui.snapshot.activeTrack == i);
        bool mut = ui.snapshot.trackMutes[i];
        uint16_t col  = mut ? blend16(TRACK_COLORS[i], C_PANEL_BG, 60) : TRACK_COLORS[i];
        uint16_t dim_fill = blend16(C_BORDER_DIM, C_PANEL_BG, 60);

        tft.drawRect(R_TRACKS[i].x, R_TRACKS[i].y, R_TRACKS[i].w, R_TRACKS[i].h, sel ? blend16(col, C_PANEL_BG, 102) : C_DIV);

        if (sel) {
            tft.fillRect(R_TRACKS[i].x, R_TRACKS[i].y, 2, R_TRACKS[i].h, col);
        }

        tft.fillCircle(R_TRACKS[i].x + 12, R_TRACKS[i].y + R_TRACKS[i].h / 2, 2, col);

        lbl[0] = TRACK_CHARS[i];
        tft.setTextColor(sel ? col : C_TXT_DIM, C_PANEL_BG);
        tft.drawString(lbl, R_TRACKS[i].x + 28, R_TRACKS[i].y + R_TRACKS[i].h / 2);
    }
}
"""
content = re.sub(r'static void draw_tracks\(void\) \{.*?(?=/\* ═══════════════════════════════════════════════════════════════════════════════\n   DRAW — EUCLIDEAN)', draw_tracks_new, content, flags=re.DOTALL)


# Replace draw_ring
draw_ring_new = """static void draw_ring(void) {
    s_ring.fillSprite(C_PANEL_BG);

    /* Guide circles */
    for (int t = 0; t < 4; t++) { // 4 rings for K, S, C, O
        s_ring.drawCircle(RING_CX, RING_CY, RING_RADII[t], C_DIV);
        s_ring.drawCircle(RING_CX, RING_CY, RING_RADII[t] - 1, C_DIV);
    }

    /* Playhead spoke */
    if (ui.snapshot.isPlaying) {
        float ang = ((ui.snapshot.currentStep % 16) / 16.0f) * 2.0f * (float)M_PI - (float)(M_PI / 2.0);
        int px = RING_CX + (int)(25 * cosf(ang));
        int py = RING_CY + (int)(25 * sinf(ang));
        for (int i = 0; i < 25; i+=2) { // dashed line
            int dx = RING_CX + (int)(i * cosf(ang));
            int dy = RING_CY + (int)(i * sinf(ang));
            s_ring.drawPixel(dx, dy, blend16(C_ACCENT, C_PANEL_BG, 96));
        }

        // extra dot on K ring
        int kx = RING_CX + (int)(RING_RADII[0] * cosf(ang));
        int ky = RING_CY + (int)(RING_RADII[0] * sinf(ang));
        s_ring.fillCircle(kx, ky, 2, C_BG);
        s_ring.drawCircle(kx, ky, 2, C_ACCENT_L);
    }

    /* Step dots */
    for (int t = 0; t < 4; t++) { // Only first 4 tracks have rings
        int len = ui.snapshot.patternLens[t];
        if (len <= 0) continue;
        bool sel = (t == ui.snapshot.activeTrack);
        bool mut = ui.snapshot.trackMutes[t];
        uint16_t col = TRACK_COLORS[t];
        uint8_t  rad = RING_RADII[t];

        for (int i = 0; i < len; i++) {
            float ang = (2.0f * (float)M_PI * i / len) - (float)(M_PI / 2.0);
            int16_t px = (int16_t)(RING_CX + rad * cosf(ang));
            int16_t py = (int16_t)(RING_CY + rad * sinf(ang));
            bool active   = ui.snapshot.patterns[t][i] > 0;

            if (mut) {
                if (active) s_ring.fillCircle(px, py, 1, blend16(col, C_PANEL_BG, 60));
            } else if (active) {
                s_ring.fillCircle(px, py, 2, col);
            }
        }
    }

    s_ring.pushSprite(R_RING.x, R_RING.y);
}
"""
content = re.sub(r'static void draw_ring\(void\) \{.*?(?=/\* ═══════════════════════════════════════════════════════════════════════════════\n   DRAW — BOTTOM)', draw_ring_new, content, flags=re.DOTALL)


with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
