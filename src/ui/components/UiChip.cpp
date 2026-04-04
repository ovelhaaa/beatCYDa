#include "UiChip.h"

#include "../theme/UiTheme.h"

namespace ui {

bool UiChip::hitTest(int16_t x, int16_t y) const {
  return rect.contains(x, y);
}

void UiChip::draw(lgfx::LGFX_Device &canvas) const {
  uint16_t fill = active ? theme::UiTheme::Colors::Accent : theme::UiTheme::Colors::Surface;
  uint16_t border = selected ? theme::UiTheme::Colors::TextPrimary : theme::UiTheme::Colors::Outline;
  uint16_t text = theme::UiTheme::Colors::TextPrimary;

  if (muted) {
    fill = theme::UiTheme::Colors::Disabled;
    border = theme::UiTheme::Colors::Outline;
    text = theme::UiTheme::Colors::TextSecondary;
  } else if (active && selected) {
    fill = theme::UiTheme::Colors::AccentPressed;
  }

  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, theme::UiTheme::Metrics::RadiusSm, fill);
  canvas.drawRoundRect(rect.x, rect.y, rect.w, rect.h, theme::UiTheme::Metrics::RadiusSm, border);
  canvas.setTextColor(text, fill);
  canvas.setTextSize(theme::UiTheme::Typography::BodySize);
  canvas.setCursor(rect.x + 10, rect.y + 10);
  canvas.printf("T%u", static_cast<unsigned>(trackIndex + 1));
}

} // namespace ui
