#pragma once

#include "UiRect.h"
#include <LovyanGFX.hpp>

namespace ui {

struct UiCard {
  const char *title{""};
  const char *value{""};
  bool active{false};
  UiRect rect{};

  void draw(lgfx::LGFX_Device &canvas) const;
};

} // namespace ui
