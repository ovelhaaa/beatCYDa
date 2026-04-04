#pragma once

#include "UiRect.h"
#include <LovyanGFX.hpp>

namespace ui {

struct UiFader {
  uint8_t track{0};
  uint8_t value{50};
  bool captured{false};
  UiRect visualRect{};
  int8_t hitRectExpandPx{8};

  void draw(lgfx::LGFX_Device &canvas) const;
  bool hitTest(int16_t x, int16_t y) const;
  bool beginCapture(int16_t x, int16_t y);
  uint8_t updateFromY(int16_t y);
  void endCapture();

private:
  UiRect expandedHitRect() const;
};

} // namespace ui
