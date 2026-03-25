import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()

# I am replacing draw_footer and adding draw_tabs
# Since draw_footer was originally for bottom action row, I will replace it with the new footer, and move the tabs logic into draw_tabs.

draw_tabs_and_footer = """static void draw_tabs(void) {
    bool is_seq = (ui.mode == UiMode::PATTERN_EDIT);
    bool is_dsp = (ui.mode == UiMode::SOUND_EDIT);
    bool is_mix = (ui.mode == UiMode::MIXER);

    /* SEQ Tab */
    tft.fillRect(R_TAB_SEQ.x, R_TAB_SEQ.y, R_TAB_SEQ.w, R_TAB_SEQ.h, is_seq ? C_FILL_TAB : C_PANEL_BG);
    tft.drawFastVLine(R_TAB_SEQ.x + R_TAB_SEQ.w - 1, R_TAB_SEQ.y, R_TAB_SEQ.h, C_DIV);
    if (is_seq) tft.drawFastHLine(R_TAB_SEQ.x, R_TAB_SEQ.y + R_TAB_SEQ.h - 1, R_TAB_SEQ.w, C_ACCENT);
    tft.setTextColor(is_seq ? C_ACCENT_L : C_TXT_DIM, is_seq ? C_FILL_TAB : C_PANEL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("S E Q", R_TAB_SEQ.x + R_TAB_SEQ.w / 2, R_TAB_SEQ.y + R_TAB_SEQ.h / 2);

    /* DSP Tab */
    tft.fillRect(R_TAB_DSP.x, R_TAB_DSP.y, R_TAB_DSP.w, R_TAB_DSP.h, is_dsp ? C_FILL_TAB : C_PANEL_BG);
    tft.drawFastVLine(R_TAB_DSP.x + R_TAB_DSP.w - 1, R_TAB_DSP.y, R_TAB_DSP.h, C_DIV);
    if (is_dsp) tft.drawFastHLine(R_TAB_DSP.x, R_TAB_DSP.y + R_TAB_DSP.h - 1, R_TAB_DSP.w, C_ACCENT);
    tft.setTextColor(is_dsp ? C_ACCENT_L : C_TXT_DIM, is_dsp ? C_FILL_TAB : C_PANEL_BG);
    tft.drawString("D S P", R_TAB_DSP.x + R_TAB_DSP.w / 2, R_TAB_DSP.y + R_TAB_DSP.h / 2);

    /* MIX Tab */
    tft.fillRect(R_TAB_MIX.x, R_TAB_MIX.y, R_TAB_MIX.w, R_TAB_MIX.h, is_mix ? C_FILL_TAB : C_PANEL_BG);
    if (is_mix) tft.drawFastHLine(R_TAB_MIX.x, R_TAB_MIX.y + R_TAB_MIX.h - 1, R_TAB_MIX.w, C_ACCENT);
    tft.setTextColor(is_mix ? C_ACCENT_L : C_TXT_DIM, is_mix ? C_FILL_TAB : C_PANEL_BG);
    tft.drawString("M I X", R_TAB_MIX.x + R_TAB_MIX.w / 2, R_TAB_MIX.y + R_TAB_MIX.h / 2);
}

static void draw_footer(void) {
    tft.fillRect(R_FOOTER.x, R_FOOTER.y, R_FOOTER.w, R_FOOTER.h, C_PANEL_BG);
    tft.drawFastHLine(R_FOOTER.x, R_FOOTER.y, R_FOOTER.w, C_DIV);

    int tr = ui.snapshot.activeTrack;

    // Left Zone
    if (ui.statusUntilMs > millis()) {
        tft.setTextColor(C_TXT_STAT, C_PANEL_BG);
        tft.setTextDatum(ML_DATUM);
        tft.drawString(ui.status, R_FOOTER.x + 6, R_FOOTER.y + 8);
    } else {
        bool pl = ui.snapshot.isPlaying;
        uint16_t fill = pl ? blend16(C_ACCENT, C_PANEL_BG, 33) : C_PANEL_BG;
        tft.fillRoundRect(R_FOOTER.x + 6, R_FOOTER.y + 2, 60, 12, 2, fill);
        if (pl) tft.drawRoundRect(R_FOOTER.x + 6, R_FOOTER.y + 2, 60, 12, 2, blend16(C_ACCENT, C_PANEL_BG, 76));
        tft.setTextColor(pl ? blend16(C_ACCENT_L, C_PANEL_BG, 119) : C_TXT_STAT, fill);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(pl ? "X PLAYING" : "> STOP", R_FOOTER.x + 6 + 30, R_FOOTER.y + 8);
    }

    // Center Zone
    char center_buf[32];
    int steps = ui.snapshot.trackSteps[tr];
    int rot = ui.snapshot.trackRotations[tr];
    snprintf(center_buf, sizeof(center_buf), "%c - %d/16 - ROT %d", TRACK_CHARS[tr], steps, rot);
    tft.setTextColor(C_TXT_STAT, C_PANEL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(center_buf, R_FOOTER.x + R_FOOTER.w / 2, R_FOOTER.y + 8);

    // Right Zone
    char right_buf[16];
    snprintf(right_buf, sizeof(right_buf), "step %d/16", (ui.snapshot.currentStep % 16) + 1);
    tft.setTextColor(C_TXT_DIM, C_PANEL_BG);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(right_buf, R_FOOTER.x + R_FOOTER.w - 6, R_FOOTER.y + 8);
}
"""
content = re.sub(r'static void draw_footer\(void\) \{.*?(?=/\* ═══════════════════════════════════════════════════════════════════════════════\n   DRAW — PARAMETER)', draw_tabs_and_footer, content, flags=re.DOTALL)

# Modify handleTouch logic structure for Topbar and Left Panel to match layout changes.
# I will do full handleTouch replacement in the next steps when I implement the contents.
# For now, I just ensure the full redraw loop calls draw_tabs
content = content.replace("draw_footer();\n          draw_params();\n          last_full = now;", "draw_tabs();\n          draw_footer();\n          draw_params();\n          last_full = now;")
content = content.replace("if (any_change) draw_footer();\n          if (any_change) draw_params();", "if (any_change) { draw_tabs(); draw_footer(); draw_params(); }")


with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
