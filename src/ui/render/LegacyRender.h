#pragma once

#include "../core/UiState.h"

void initLegacyRender();
void renderLegacyFrame(UiRuntime &ui, uint32_t now, uint32_t &lastFullMs, uint32_t &lastRingMs);
