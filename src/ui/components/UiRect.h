#pragma once

#include <stdint.h>

namespace ui {

struct UiRect {
  int16_t x{0};
  int16_t y{0};
  int16_t w{0};
  int16_t h{0};

  bool contains(int16_t px, int16_t py) const {
    return px >= x && py >= y && px < (x + w) && py < (y + h);
  }
};

} // namespace ui
