#pragma once

#include "../Display.h"

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

  char status[32] = "CYD ready";
  char lastStatus[32] = "";
  uint32_t statusUntilMs = 0;
  bool forceRedraw = true;
};

struct UiFramePlan {
  bool anyChange = false;
  bool patternChange = false;
  bool stepChange = false;
  bool full = false;
};

void setStatus(UiRuntime &ui, const char *msg, uint32_t ms = 1400);
void updateStatusTimeout(UiRuntime &ui, uint32_t now);
UiFramePlan buildFramePlan(const UiRuntime &ui, uint32_t now, uint32_t lastFullMs);
void commitFrame(UiRuntime &ui);
int getParamDisplayValue(const UiRuntime &ui, int paramIndex);
