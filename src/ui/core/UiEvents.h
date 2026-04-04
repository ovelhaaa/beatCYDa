#pragma once

#include "../LgfxDisplay.h"
#include "UiScreenId.h"

namespace ui {

struct UiNavEvent {
  UiScreenId target{UiScreenId::Perform};
};

struct UiTouchEvent {
  TouchPoint point;
};

} // namespace ui
