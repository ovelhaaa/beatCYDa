#include "UiTouchRouter.h"
#include "../core/UiActions.h"
#include "../core/UiLayout.h"
#include "../../CYD_Config.h"

namespace {

/* ═══════════════════════════════════════════════════════════════════════════════
   HOLD-TO-ACCELERATE
   ═══════════════════════════════════════════════════════════════════════════════ */
uint32_t holdInterval(uint8_t count) {
  if (count < CYDConfig::HoldRepeatStage1Threshold)
    return CYDConfig::HoldRepeatIntervalStartMs;
  if (count < CYDConfig::HoldRepeatStage2Threshold)
    return CYDConfig::HoldRepeatIntervalStage1Ms;
  return CYDConfig::HoldRepeatIntervalStage2Ms;
}

void holdStart(UiRuntime &ui, int paramIndex, int delta) {
  ui.activeHoldAction = delta;
  ui.activeHoldParam = paramIndex;
  ui.holdTickCount = 0;
  ui.holdNextTickMs = millis() + CYDConfig::HoldRepeatStartDelayMs;
}

void holdStop(UiRuntime &ui) {
  ui.activeHoldAction = 0;
  ui.activeHoldParam = -1;
  ui.holdTickCount = 0;
}

void paramDelta(UiRuntime &ui, int row, int delta) {
  dispatchParamDelta(ui, row, delta);
  holdStart(ui, row, delta);
}

Rect sliderTouchRect(int index) {
  constexpr int kTouchPadX = 8;
  constexpr int kTouchPadTop = 14;
  constexpr int kTouchPadBottom = 18;

  const Rect &visual = R_SLIDERS[index];
  return {static_cast<int16_t>(visual.x - kTouchPadX),
          static_cast<int16_t>(visual.y - kTouchPadTop),
          static_cast<int16_t>(visual.w + (kTouchPadX * 2)),
          static_cast<int16_t>(visual.h + kTouchPadTop + kTouchPadBottom)};
}

void dispatchFaderValue(int index, int y) {
  const Rect &visual = R_SLIDERS[index];
  float norm = 1.0f - static_cast<float>(y - visual.y) / visual.h;
  norm = norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);
  dispatchUiAction(UiActionType::SET_VOICE_GAIN, index, static_cast<int>(norm * 100));
}

} // namespace

/* ═══════════════════════════════════════════════════════════════════════════════
   TOUCH HANDLER
   ═══════════════════════════════════════════════════════════════════════════════ */
void handleTouch(UiRuntime &ui, const TouchPoint &tp) {
  if (tp.justReleased) {
    holdStop(ui);
    ui.activeFaderIndex = -1;
    return;
  }

  if (tp.pressed && ui.activeFaderIndex >= 0) {
    dispatchFaderValue(ui.activeFaderIndex, tp.y);
    return;
  }

  if (!tp.justPressed)
    return;

  const int tx = tp.x;
  const int ty = tp.y;

  if (ty < 42) {
    if (R_PLAY.contains(tx, ty)) {
      dispatchUiAction(UiActionType::TOGGLE_PLAY, 0, 0);
      return;
    }
    if (R_SLOT_DEC.contains(tx, ty)) {
      ui.activeSlot = (ui.activeSlot == 0) ? (CYDConfig::PatternSlots - 1) : (ui.activeSlot - 1);
      return;
    }
    if (R_SLOT_INC.contains(tx, ty)) {
      ui.activeSlot = (ui.activeSlot + 1) % CYDConfig::PatternSlots;
      return;
    }
    if (R_SAVE.contains(tx, ty)) {
      dispatchSaveSlot(ui);
      return;
    }
    if (R_LOAD.contains(tx, ty)) {
      dispatchLoadSlot(ui);
      return;
    }
    if (R_BPM_DEC.contains(tx, ty)) {
      dispatchUiAction(UiActionType::NUDGE_BPM, 0, -1);
      holdStart(ui, 10, -1);
      return;
    }
    if (R_BPM_INC.contains(tx, ty)) {
      dispatchUiAction(UiActionType::NUDGE_BPM, 0, +1);
      holdStart(ui, 10, +1);
      return;
    }
    return;
  }

  if (tx < 36) {
    for (int i = 0; i < TRACK_COUNT; i++) {
      if (R_TRACKS[i].contains(tx, ty)) {
        dispatchUiAction(UiActionType::SELECT_TRACK, 0, i);
        return;
      }
    }
    return;
  }

  if (ty >= 208 && tx < 192) {
    if (R_MUTE.contains(tx, ty)) {
      dispatchUiAction(UiActionType::TOGGLE_MUTE, 0, ui.snapshot.activeTrack);
      return;
    }
    if (R_VOICE.contains(tx, ty)) {
      dispatchModeChange(ui, UiMode::SOUND_EDIT);
      return;
    }
    if (R_MIX.contains(tx, ty)) {
      dispatchModeChange(ui, UiMode::MIXER);
      return;
    }
    return;
  }

  if (R_RING.contains(tx, ty) && ui.mode != UiMode::PATTERN_EDIT) {
    dispatchModeChange(ui, UiMode::PATTERN_EDIT);
    return;
  }

  if (ui.mode == UiMode::MIXER && tx >= 192) {
    for (int i = 0; i < TRACK_COUNT; i++) {
      if (sliderTouchRect(i).contains(tx, ty)) {
        ui.activeFaderIndex = i;
        dispatchFaderValue(i, ty);
        return;
      }
    }
    return;
  }

  if (tx >= PANEL_SX) {
    const int lx = tx - PANEL_SX;
    const int ly = ty - PANEL_SY;
    for (int row = 0; row < 4; row++) {
      const ParamRow *pr = &PARAM_ROWS[row];
      if (!pr->touch.contains(lx, ly))
        continue;
      if (pr->minus.contains(lx, ly)) {
        paramDelta(ui, row, -1);
        return;
      }
      if (pr->plus.contains(lx, ly)) {
        paramDelta(ui, row, +1);
        return;
      }
      return;
    }
  }
}

void tickHold(UiRuntime &ui) {
  if (ui.activeHoldParam < 0 || ui.activeHoldAction == 0)
    return;

  const uint32_t now = millis();
  if (now < ui.holdNextTickMs)
    return;

  ui.holdTickCount++;
  int amount = ui.activeHoldAction;
  if (ui.holdTickCount > CYDConfig::HoldRepeatStage2Threshold) {
    amount *= CYDConfig::HoldRepeatStage2Multiplier;
  } else if (ui.holdTickCount > CYDConfig::HoldRepeatStage1Threshold) {
    amount *= CYDConfig::HoldRepeatStage1Multiplier;
  }

  if (ui.activeHoldParam == 10) {
    dispatchUiAction(UiActionType::NUDGE_BPM, 0, amount);
  } else {
    dispatchParamDelta(ui, ui.activeHoldParam, amount);
  }

  ui.holdNextTickMs = now + holdInterval(ui.holdTickCount);
}
