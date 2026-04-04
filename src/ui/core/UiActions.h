#pragma once

#include "UiState.h"

void dispatchUiAction(UiActionType type, int idx = 0, int val = 0);
void dispatchModeChange(UiRuntime &ui, UiMode mode);
void dispatchParamDelta(UiRuntime &ui, int paramIndex, int amount);
void dispatchSaveSlot(uint8_t slot);
void dispatchLoadSlot(uint8_t slot);
