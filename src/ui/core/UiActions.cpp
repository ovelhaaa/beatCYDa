#include "UiActions.h"
#include "../../control/ControlManager.h"

void dispatchUiAction(UiActionType type, int idx, int val) {
  IPCCommand cmd{};
  cmd.type = CommandType::UI_ACTION;
  cmd.voiceId = static_cast<uint8_t>(type);
  cmd.paramId = static_cast<uint8_t>(idx);
  cmd.value = static_cast<float>(val);
  CtrlMgr.sendCommand(cmd);
}

void dispatchSaveSlot(uint8_t slot) {
  dispatchUiAction(UiActionType::SAVE_SLOT, slot, 0);
}

void dispatchLoadSlot(uint8_t slot) {
  dispatchUiAction(UiActionType::LOAD_SLOT, slot, 0);
}
