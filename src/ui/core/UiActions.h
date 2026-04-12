#pragma once

#include "../UiAction.h"

void dispatchUiAction(UiActionType type, int idx = 0, int val = 0);
void dispatchSaveSlot(uint8_t slot);
void dispatchLoadSlot(uint8_t slot);
