#pragma once

#include "UiRect.h"
#include <LovyanGFX.hpp>

namespace ui {

enum class UiToastSeverity : uint8_t {
  Info,
  Warning,
  Danger,
};

struct UiToast {
  const char *message{""};
  UiToastSeverity severity{UiToastSeverity::Info};
  uint32_t timeoutMs{1400};
  UiRect rect{};

  void draw(lgfx::LGFX_Device &canvas) const;
};

} // namespace ui
