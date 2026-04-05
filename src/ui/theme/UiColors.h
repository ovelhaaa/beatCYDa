#pragma once

#include <stdint.h>

namespace ui::theme {

struct UiColors {
  static constexpr uint16_t Bg = 0x0000;
  static constexpr uint16_t ModalScrim = 0x3186;
  static constexpr uint16_t Surface = 0x18C3;
  static constexpr uint16_t SurfacePressed = 0x10E2;
  static constexpr uint16_t Outline = 0x39E7;
  static constexpr uint16_t TextPrimary = 0xFFFF;
  static constexpr uint16_t TextSecondary = 0xBDF7;
  static constexpr uint16_t TextOnAccent = 0xFFFF;
  static constexpr uint16_t TextOnWarning = 0x2124;
  static constexpr uint16_t TextOnDanger = 0xFFFF;
  static constexpr uint16_t Accent = 0x2D7F;
  static constexpr uint16_t AccentPressed = 0x1C8B;
  static constexpr uint16_t Warning = 0xFD20;
  static constexpr uint16_t Danger = 0xF800;
  static constexpr uint16_t DangerPressed = 0xB000;
  static constexpr uint16_t Disabled = 0x630C;
};

} // namespace ui::theme
