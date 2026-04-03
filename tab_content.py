import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()


new_draw_params = """
/* ═══════════════════════════════════════════════════════════════════════════════
   DRAW — TAB CONTENT
   ═══════════════════════════════════════════════════════════════════════════════ */

static void draw_tab_seq(void) {
    s_panel.fillSprite(C_PANEL_BG);

    int tr = ui.snapshot.activeTrack;
    uint16_t tc = TRACK_COLORS[tr];

    // Grid 16 steps (2x8)
    int cell_w = 28;
    int cell_h = 14;
    int start_x = 6;
    int start_y = 7;

    for (int i = 0; i < 16; i++) {
        int row = i / 8;
        int col = i % 8;

        int x = start_x + col * (cell_w + 2);
        if (col >= 4) x += 3; // gap after 4 beats

        int y = start_y + row * (cell_h + 2);

        bool active = ui.snapshot.patterns[tr][i] > 0;
        bool playhead = ui.snapshot.isPlaying && (i == ui.snapshot.currentStep % 16);

        if (playhead) {
            s_panel.fillRect(x, y, cell_w, cell_h, blend16(tc, C_PANEL_BG, 80));
            s_panel.drawRect(x, y, cell_w, cell_h, C_ACCENT_L);
        } else if (active) {
            s_panel.fillRect(x, y, cell_w, cell_h, blend16(C_FILL_BTN, C_PANEL_BG, 128)); // base fill
            // emulated diagonal overlay
            for (int dx = 0; dx < cell_w; dx+=2) {
                s_panel.drawLine(x + dx, y, x, y + dx > y + cell_h ? y + cell_h : y + dx, blend16(tc, C_PANEL_BG, 68));
            }
            s_panel.drawRect(x, y, cell_w, cell_h, blend16(tc, C_PANEL_BG, 102));
        } else {
            s_panel.fillRoundRect(x, y, cell_w, cell_h, 2, C_FILL_TAB);
            s_panel.drawRoundRect(x, y, cell_w, cell_h, 2, C_BORDER_DIM);
        }
    }

    // Steppers HITS and ROT
    int hits = ui.snapshot.trackHits[tr];
    int rot = ui.snapshot.trackRotations[tr];

    int stepper_y = start_y + 2 * (cell_h + 2) + 6;

    // HITS
    s_panel.setTextColor(C_TXT_STAT, C_PANEL_BG);
    s_panel.setTextDatum(ML_DATUM);
    s_panel.drawString("HITS", start_x, stepper_y + 8);

    int step_btn_w = 14;
    int step_val_w = 30;
    int step_x = start_x + 28 + 6;

    s_panel.fillRoundRect(step_x, stepper_y, step_btn_w, step_btn_w, 2, C_FILL_TAB);
    s_panel.drawRoundRect(step_x, stepper_y, step_btn_w, step_btn_w, 2, C_BORDER_DIM);
    s_panel.setTextColor(C_TXT_SEC, C_FILL_TAB);
    s_panel.setTextDatum(MC_DATUM);
    s_panel.drawString("-", step_x + step_btn_w/2, stepper_y + step_btn_w/2);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", hits);
    s_panel.setTextColor(C_ACCENT_L, C_PANEL_BG);
    s_panel.drawString(buf, step_x + step_btn_w + step_val_w/2, stepper_y + step_btn_w/2);

    int plus_x = step_x + step_btn_w + step_val_w;
    s_panel.fillRoundRect(plus_x, stepper_y, step_btn_w, step_btn_w, 2, C_FILL_TAB);
    s_panel.drawRoundRect(plus_x, stepper_y, step_btn_w, step_btn_w, 2, C_BORDER_DIM);
    s_panel.setTextColor(C_TXT_SEC, C_FILL_TAB);
    s_panel.drawString("+", plus_x + step_btn_w/2, stepper_y + step_btn_w/2);

    // ROT
    stepper_y += step_btn_w + 6;
    s_panel.setTextColor(C_TXT_STAT, C_PANEL_BG);
    s_panel.setTextDatum(ML_DATUM);
    s_panel.drawString("ROT", start_x, stepper_y + 8);

    s_panel.fillRoundRect(step_x, stepper_y, step_btn_w, step_btn_w, 2, C_FILL_TAB);
    s_panel.drawRoundRect(step_x, stepper_y, step_btn_w, step_btn_w, 2, C_BORDER_DIM);
    s_panel.setTextColor(C_TXT_SEC, C_FILL_TAB);
    s_panel.setTextDatum(MC_DATUM);
    s_panel.drawString("-", step_x + step_btn_w/2, stepper_y + step_btn_w/2);

    snprintf(buf, sizeof(buf), "%d", rot);
    s_panel.setTextColor(C_ACCENT_L, C_PANEL_BG);
    s_panel.drawString(buf, step_x + step_btn_w + step_val_w/2, stepper_y + step_btn_w/2);

    s_panel.fillRoundRect(plus_x, stepper_y, step_btn_w, step_btn_w, 2, C_FILL_TAB);
    s_panel.drawRoundRect(plus_x, stepper_y, step_btn_w, step_btn_w, 2, C_BORDER_DIM);
    s_panel.setTextColor(C_TXT_SEC, C_FILL_TAB);
    s_panel.drawString("+", plus_x + step_btn_w/2, stepper_y + step_btn_w/2);

    s_panel.pushSprite(PANEL_SX, PANEL_SY);
}

static void draw_tab_dsp(void) {
    s_panel.fillSprite(C_PANEL_BG);
    int tr = ui.snapshot.activeTrack;
    bool bass = tr == VOICE_BASS;

    int padding = 6;
    int kw = (PANEL_W - 3*padding) / 2;
    int kh = (PANEL_H - 3*padding) / 2;

    for (int i=0; i<4; i++) {
        int row = i / 2;
        int col = i % 2;
        int x = padding + col * (kw + padding);
        int y = padding + row * (kh + padding);

        s_panel.fillRoundRect(x, y, kw, kh, 3, C_FILL_BTN);
        s_panel.drawRoundRect(x, y, kw, kh, 3, C_DIV);

        ParamMeta m;
        param_meta(i, &m);
        if (m.label[0] == '\0') continue;

        uint16_t arc_col = (i == 2) ? RGB565(0x00, 0xAA, 0xFF) : C_ACCENT;

        int cx = x + kw/2;
        int cy = y + kh/2;
        int r = 14;

        // Background arc
        s_panel.drawArc(cx, cy, r, r-2, 135, 405, C_DIV);

        // Foreground arc
        int end_ang = 135 + (int)(m.norm * 270);
        if (end_ang > 135) {
            s_panel.drawArc(cx, cy, r, r-2, 135, end_ang, arc_col);
        }

        s_panel.setTextColor(C_TXT_STAT, C_FILL_BTN);
        s_panel.setTextDatum(MC_DATUM);
        s_panel.drawString(m.label, cx, y + 8);

        s_panel.setTextColor(C_ACCENT_L, C_FILL_BTN);
        s_panel.drawString(m.value, cx, y + kh - 8);
    }

    s_panel.pushSprite(PANEL_SX, PANEL_SY);
}

static void draw_tab_mix(void) {
    s_panel.fillSprite(C_PANEL_BG);

    int padding_y = 5;
    int padding_x = 5;
    int bar_h = 18;
    int gap = 4;

    int total_w = PANEL_W - 2 * padding_x;
    int lbl_w = 14;
    int val_w = 24;
    int bar_w = total_w - lbl_w - val_w - 8;

    int y = padding_y;
    char buf[8];

    for (int i=0; i < TRACK_COUNT; i++) {
        s_panel.setTextColor(TRACK_COLORS[i], C_PANEL_BG);
        s_panel.setTextDatum(ML_DATUM);
        char lbl[2] = {TRACK_CHARS[i], '\\0'};
        s_panel.drawString(lbl, padding_x, y + bar_h/2);

        int bx = padding_x + lbl_w + 4;
        s_panel.fillRoundRect(bx, y + 5, bar_w, 8, 2, C_FILL_TAB);
        s_panel.drawRoundRect(bx, y + 5, bar_w, 8, 2, C_BORDER_DIM);

        float gain = ui.snapshot.voiceGain[i];
        int fw = (int)(gain * bar_w);
        if (fw > 0) {
            uint16_t dim_col = blend16(TRACK_COLORS[i], C_PANEL_BG, 80);
            s_panel.fillRoundRect(bx, y + 5, fw, 8, 2, TRACK_COLORS[i]);
            s_panel.fillRect(bx + fw/2, y + 5, fw - fw/2, 8, blend16(dim_col, TRACK_COLORS[i], 128)); // Emulate gradient
        }

        s_panel.setTextColor(C_TXT_STAT, C_PANEL_BG);
        s_panel.setTextDatum(MR_DATUM);
        snprintf(buf, sizeof(buf), "%d", (int)(gain * 100));
        s_panel.drawString(buf, padding_x + total_w, y + bar_h/2);

        y += bar_h + gap;
    }

    y += gap; // Extra margin before MST
    s_panel.drawFastHLine(padding_x, y, total_w, C_DIV);
    y += gap + 1;

    s_panel.setTextColor(C_TXT_SEC, C_PANEL_BG);
    s_panel.setTextDatum(ML_DATUM);
    s_panel.drawString("M", padding_x, y + bar_h/2);

    int bx = padding_x + lbl_w + 4;
    s_panel.fillRoundRect(bx, y + 5, bar_w, 8, 2, C_FILL_TAB);
    s_panel.drawRoundRect(bx, y + 5, bar_w, 8, 2, C_BORDER_DIM);

    float mgain = ui.snapshot.masterVolume;
    int mfw = (int)(mgain * bar_w);
    if (mfw > 0) {
        s_panel.fillRoundRect(bx, y + 5, mfw, 8, 2, C_TXT_STAT);
    }

    s_panel.setTextColor(C_TXT_STAT, C_PANEL_BG);
    s_panel.setTextDatum(MR_DATUM);
    snprintf(buf, sizeof(buf), "%d", (int)(mgain * 100));
    s_panel.drawString(buf, padding_x + total_w, y + bar_h/2);

    s_panel.pushSprite(PANEL_SX, PANEL_SY);
}

static void draw_params(void) {
    if (ui.mode == UiMode::PATTERN_EDIT) draw_tab_seq();
    else if (ui.mode == UiMode::SOUND_EDIT) draw_tab_dsp();
    else if (ui.mode == UiMode::MIXER) draw_tab_mix();
}
"""

content = re.sub(r'static void draw_params\(void\) \{.*?(?=/\* ═══════════════════════════════════════════════════════════════════════════════\n   HOLD-TO-ACCELERATE)', new_draw_params, content, flags=re.DOTALL)

with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
