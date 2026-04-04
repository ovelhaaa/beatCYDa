#include "UiActions.h"
#include "../../control/ControlManager.h"
#include "../../storage/PatternStorage.h"

void dispatchUiAction(UiActionType type, int idx, int val) {
  IPCCommand cmd{};
  cmd.type = CommandType::UI_ACTION;
  cmd.voiceId = static_cast<uint8_t>(type);
  cmd.paramId = static_cast<uint8_t>(idx);
  cmd.value = static_cast<float>(val);
  CtrlMgr.sendCommand(cmd);
}

void dispatchModeChange(UiRuntime &ui, UiMode mode) {
  dispatchUiAction(UiActionType::CHANGE_MODE, 0, static_cast<int>(mode));
  ui.mode = mode;
}

void dispatchParamDelta(UiRuntime &ui, int paramIndex, int amount) {
  const int track = ui.snapshot.activeTrack;
  const bool isBass = track == VOICE_BASS;

  const int displayValue = getParamDisplayValue(ui, paramIndex);
  const int newValue = displayValue + amount;

  if (ui.mode == UiMode::SOUND_EDIT) {
    if (isBass) {
      dispatchUiAction(UiActionType::SET_BASS_PARAM, paramIndex, newValue);
      return;
    }
    dispatchUiAction(UiActionType::SET_SOUND_PARAM, paramIndex, newValue);
    return;
  }

  if (isBass) {
    dispatchUiAction(UiActionType::SET_BASS_PARAM, paramIndex, newValue);
    return;
  }

  switch (paramIndex) {
  case 0:
    dispatchUiAction(UiActionType::SET_STEPS, track, newValue);
    break;
  case 1:
    dispatchUiAction(UiActionType::SET_HITS, track, newValue);
    break;
  case 2:
    dispatchUiAction(UiActionType::SET_ROTATION, track, newValue);
    break;
  default:
    dispatchUiAction(UiActionType::SET_SOUND_PARAM, 3, newValue);
    break;
  }
}

void dispatchSaveSlot(UiRuntime &ui) {
  setStatus(ui, PatternStore.saveSlot(ui.activeSlot) ? "Saved!" : "Save Fail");
}

void dispatchLoadSlot(UiRuntime &ui) {
  setStatus(ui, PatternStore.loadSlot(ui.activeSlot) ? "Loaded!" : "Load Fail");
}
