#pragma once

#include "UiRect.h"
#include <LovyanGFX.hpp>

namespace ui {

enum class UiButtonVariant : uint8_t {
  Primary,
  Secondary,
  Danger,
};

struct UiButton {
  const char *label{""};
  UiButtonVariant variant{UiButtonVariant::Primary};
  bool pressed{false};
  bool disabled{false};
  UiRect rect{};

  bool hitTest(int16_t x, int16_t y) const;
  void draw(lgfx::LGFX_Device &canvas) const;
};

} // namespace ui
