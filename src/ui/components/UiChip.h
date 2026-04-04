#pragma once

#include "UiRect.h"
#include <LovyanGFX.hpp>

namespace ui {

struct UiChip {
  uint8_t trackIndex{0};
  bool active{false};
  bool muted{false};
  bool selected{false};
  UiRect rect{};

  bool hitTest(int16_t x, int16_t y) const;
  void draw(lgfx::LGFX_Device &canvas) const;
};

} // namespace ui
