#include "UiButton.h"

#include "../theme/UiTheme.h"
#include <cstring>

namespace ui {

bool UiButton::hitTest(int16_t x, int16_t y) const {
  return !disabled && rect.contains(x, y);
}

void UiButton::draw(lgfx::LGFX_Device &canvas) const {
  uint16_t fill = theme::UiTheme::Colors::Accent;
  uint16_t fillPressed = theme::UiTheme::Colors::AccentPressed;
  if (variant == UiButtonVariant::Secondary) {
    fill = theme::UiTheme::Colors::Surface;
    fillPressed = theme::UiTheme::Colors::SurfacePressed;
  } else if (variant == UiButtonVariant::Danger) {
    fill = theme::UiTheme::Colors::Danger;
    fillPressed = theme::UiTheme::Colors::DangerPressed;
  }

  if (disabled) {
    fill = theme::UiTheme::Colors::Disabled;
  } else if (pressed) {
    fill = fillPressed;
  }

  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, theme::UiTheme::Metrics::RadiusSm, fill);
  canvas.drawRoundRect(rect.x, rect.y, rect.w, rect.h, theme::UiTheme::Metrics::RadiusSm,
                       theme::UiTheme::Colors::Outline);

  canvas.setTextSize(theme::UiTheme::Typography::BodySize);
  const uint16_t textColor = disabled ? theme::UiTheme::Colors::TextSecondary : theme::UiTheme::Colors::TextPrimary;
  canvas.setTextColor(textColor, fill);
  const int textWidth = static_cast<int>(strlen(label)) * 6 * theme::UiTheme::Typography::BodySize;
  const int tx = rect.x + (rect.w - textWidth) / 2;
  const int ty = rect.y + (rect.h - 8 * theme::UiTheme::Typography::BodySize) / 2;
  canvas.setCursor(tx, ty);
  canvas.print(label);
}

} // namespace ui
