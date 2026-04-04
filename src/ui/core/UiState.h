#pragma once

#include "../Display.h"
#include "UiInvalidation.h"

struct UiFrameMetrics {
  uint32_t frameCount = 0;
  uint32_t lastRenderMicros = 0;
  uint32_t lastFreeHeap = 0;
};

struct UiRuntime {
  UiStateSnapshot snapshot;
  UiStateSnapshot lastSnapshot;
  uint8_t activeSlot = 0;
  uint8_t lastActiveSlot = 255;
  UiMode mode = UiMode::PATTERN_EDIT;
  UiMode lastMode = static_cast<UiMode>(0xFF);

  int activeHoldAction = 0;
  int activeHoldParam = -1;
  uint32_t holdNextTickMs = 0;
  int holdTickCount = 0;
  int activeFaderIndex = -1;

  char status[32] = "CYD ready";
  char lastStatus[32] = "";
  uint32_t statusUntilMs = 0;
  bool storageOpInProgress = false;
  UiActionType pendingStorageAction = UiActionType::SAVE_SLOT;
  uint8_t pendingStorageSlot = 0;
  uint32_t storageOpDeadlineMs = 0;
  bool forceRedraw = true;
  UiInvalidation invalidation;
  UiFrameMetrics metrics;
};

void setStatus(UiRuntime &ui, const char *msg, uint32_t ms = 1400);
void updateStatusTimeout(UiRuntime &ui, uint32_t now);
void updateUiInvalidation(UiRuntime &ui, uint32_t now, uint32_t lastFullMs);
void commitFrame(UiRuntime &ui);
int getParamDisplayValue(const UiRuntime &ui, int paramIndex);
