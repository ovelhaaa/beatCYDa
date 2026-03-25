import re

with open('src/ui/Display.cpp', 'r') as f:
    content = f.read()

# I need to properly replace handleTouch, it seems the previous regex left old code at the bottom of the function or I missed the curly brackets.

# Also it seems my old replace logic replaced something it shouldn't have or didn't replace it all. Let's find handleTouch in the file and replace it completely.

start_str = "static void handleTouch(const TouchPoint &tp) {"
end_str = "} // namespace\n"

start_idx = content.find(start_str)
end_idx = content.find(end_str)

if start_idx != -1 and end_idx != -1:
    content = content[:start_idx] + """static void handleTouch(const TouchPoint &tp) {
    if (tp.justReleased) { hold_stop(); return; }
    if (!tp.justPressed) return;

    int tx = tp.x, ty = tp.y;

    /* ── Topbar (y < 30) ────────────────────────────────────────────────── */
    if (ty < 30) {
        if (R_PLAY.contains(tx, ty))  { postUiAction(UiActionType::TOGGLE_PLAY, 0, 0); return; }
        if (R_BPM_DEC.contains(tx, ty)) { postUiAction(UiActionType::NUDGE_BPM, 0, -1); hold_start(10, -1); return; }
        if (R_BPM_INC.contains(tx, ty)) { postUiAction(UiActionType::NUDGE_BPM, 0, +1); hold_start(10, +1); return; }

        if (R_SLOT_1.contains(tx, ty)) { ui.activeSlot = 0; return; }
        if (R_SLOT_2.contains(tx, ty)) { ui.activeSlot = 1; return; }
        if (R_SLOT_3.contains(tx, ty)) { ui.activeSlot = 2; return; }

        if (R_SAVE.contains(tx, ty))  { set_status(PatternStore.saveSlot(ui.activeSlot) ? "SAVED!" : "SAVE ERR"); return; }
        if (R_LOAD.contains(tx, ty))  { set_status(PatternStore.loadSlot(ui.activeSlot) ? "LOADED!" : "LOAD ERR"); return; }
        return;
    }

    /* ── Left Panel (x < 62, y > 30) ───────────────────────────────────────────── */
    if (tx < 62 && ty > 30) {
        // Track selector
        for (int i = 0; i < TRACK_COUNT; i++) {
            if (R_TRACKS[i].contains(tx, ty)) {
                postUiAction(UiActionType::SELECT_TRACK, 0, i); return;
            }
        }

        // Euclid ring area tap to enter pattern edit and focus
        if (R_RING.contains(tx, ty)) {
            postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::PATTERN_EDIT);
            ui.mode = UiMode::PATTERN_EDIT;
            return;
        }
        return;
    }

    /* ── Tabs (x >= 62, 30 <= y < 50) ────────────────────────────────────────────────── */
    if (tx >= 62 && ty >= 30 && ty < 50) {
        if (R_TAB_SEQ.contains(tx, ty)) { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::PATTERN_EDIT); ui.mode = UiMode::PATTERN_EDIT; return; }
        if (R_TAB_DSP.contains(tx, ty)) { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::SOUND_EDIT); ui.mode = UiMode::SOUND_EDIT; return; }
        if (R_TAB_MIX.contains(tx, ty)) { postUiAction(UiActionType::CHANGE_MODE, 0, (int)UiMode::MIXER); ui.mode = UiMode::MIXER; return; }
        return;
    }

    /* ── Tab Content (x >= 62, 50 <= y < 224) ────────────────────────────────────────── */
    if (tx >= 62 && ty >= 50 && ty < 224) {
        int lx = tx - PANEL_SX;
        int ly = ty - PANEL_SY;

        if (ui.mode == UiMode::PATTERN_EDIT) {
            // Check grid clicks
            int cell_w = 28;
            int cell_h = 14;
            int start_x = 6;
            int start_y = 7;
            for (int i = 0; i < 16; i++) {
                int row = i / 8;
                int col = i % 8;
                int x = start_x + col * (cell_w + 2);
                if (col >= 4) x += 3;
                int y = start_y + row * (cell_h + 2);

                if (lx >= x && lx < x + cell_w && ly >= y && ly < y + cell_h) {
                    postUiAction(UiActionType::TOGGLE_STEP, ui.snapshot.activeTrack, i);
                    return;
                }
            }

            // Check steppers
            int stepper_y = start_y + 2 * (cell_h + 2) + 6;
            int step_btn_w = 14;
            int step_val_w = 30;
            int step_x = start_x + 28 + 6;
            int plus_x = step_x + step_btn_w + step_val_w;

            // HITS stepper (row 1 param)
            if (ly >= stepper_y && ly < stepper_y + step_btn_w) {
                if (lx >= step_x && lx < step_x + step_btn_w) { param_delta(1, -1); return; }
                if (lx >= plus_x && lx < plus_x + step_btn_w) { param_delta(1, +1); return; }
            }

            // ROT stepper (row 2 param)
            stepper_y += step_btn_w + 6;
            if (ly >= stepper_y && ly < stepper_y + step_btn_w) {
                if (lx >= step_x && lx < step_x + step_btn_w) { param_delta(2, -1); return; }
                if (lx >= plus_x && lx < plus_x + step_btn_w) { param_delta(2, +1); return; }
            }
            return;
        }

        if (ui.mode == UiMode::SOUND_EDIT) {
            // Click quadrants for DSP
            int padding = 6;
            int kw = (PANEL_W - 3*padding) / 2;
            int kh = (PANEL_H - 3*padding) / 2;

            for (int i=0; i<4; i++) {
                int row = i / 2;
                int col = i % 2;
                int x = padding + col * (kw + padding);
                int y = padding + row * (kh + padding);

                if (lx >= x && lx < x + kw && ly >= y && ly < y + kh) {
                    // Tap left half = minus, right half = plus
                    if (lx < x + kw/2) { param_delta(i, -1); return; }
                    else { param_delta(i, +1); return; }
                }
            }
            return;
        }

        if (ui.mode == UiMode::MIXER) {
            // Vertical click on mixer bars
            int padding_y = 5;
            int padding_x = 5;
            int bar_h = 18;
            int gap = 4;
            int lbl_w = 14;
            int val_w = 24;
            int total_w = PANEL_W - 2 * padding_x;
            int bar_w = total_w - lbl_w - val_w - 8;

            int y = padding_y;

            for (int i=0; i < TRACK_COUNT; i++) {
                int bx = padding_x + lbl_w + 4;
                if (lx >= bx && lx < bx + bar_w && ly >= y && ly < y + bar_h) {
                    float norm = (float)(lx - bx) / bar_w;
                    postUiAction(UiActionType::SET_VOICE_GAIN, i, (int)(norm * 100));
                    return;
                }

                // Clicking label toggles mute
                if (lx >= padding_x && lx < padding_x + lbl_w && ly >= y && ly < y + bar_h) {
                    postUiAction(UiActionType::TOGGLE_MUTE, 0, i);
                    return;
                }

                y += bar_h + gap;
            }
            return;
        }
    }
}
""" + content[end_idx:]


with open('src/ui/Display.cpp', 'w') as f:
    f.write(content)
