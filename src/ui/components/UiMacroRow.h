#pragma once

#include "UiRect.h"
#include <LovyanGFX.hpp>

namespace ui {

struct UiMacroRow {
  const char *label{""};
  const char *valueText{""};
  UiRect minusRect{};
  UiRect plusRect{};
  UiRect rowRect{};
  bool showBar{false};
  bool focus{false};
  bool minusPressed{false};
  bool plusPressed{false};
  uint8_t barFill{0};

  void draw(lgfx::LGFX_Device &canvas) const;
  bool hitMinus(int16_t x, int16_t y) const;
  bool hitPlus(int16_t x, int16_t y) const;
  bool hitSlider(int16_t x, int16_t y) const;
  uint8_t sliderPercentAt(int16_t x) const;
};

} // namespace ui
