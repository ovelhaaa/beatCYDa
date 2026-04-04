#pragma once

#include "../../CYD_Config.h"
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
  uint32_t timeoutMs{CYDConfig::UiToastInfoMs};
  UiRect rect{};

  void draw(lgfx::LGFX_Device &canvas) const;
};

} // namespace ui
