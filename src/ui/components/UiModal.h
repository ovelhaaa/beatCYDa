#pragma once

#include "UiButton.h"
#include "UiRect.h"
#include <LovyanGFX.hpp>

namespace ui {

struct UiModal {
  const char *title{""};
  const char *body{""};
  UiButton confirm{};
  UiButton cancel{};
  UiRect rect{};
  bool visible{false};

  void draw(lgfx::LGFX_Device &canvas) const;
};

} // namespace ui
