#include "UiChip.h"

#include "../theme/UiTheme.h"

namespace ui {

bool UiChip::hitTest(int16_t x, int16_t y) const {
  return rect.contains(x, y);
}

void UiChip::draw(lgfx::LGFX_Device &canvas) const {
  uint16_t fill = active ? theme::UiTheme::Colors::Accent : theme::UiTheme::Colors::Surface;
  if (muted) {
    fill = theme::UiTheme::Colors::Disabled;
  }

  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, theme::UiTheme::Metrics::RadiusSm, fill);
  canvas.drawRoundRect(rect.x, rect.y, rect.w, rect.h, theme::UiTheme::Metrics::RadiusSm,
                       selected ? theme::UiTheme::Colors::TextPrimary : theme::UiTheme::Colors::Outline);
  canvas.setTextColor(theme::UiTheme::Colors::TextPrimary, fill);
  canvas.setTextSize(theme::UiTheme::Typography::BodySize);
  canvas.setCursor(rect.x + 10, rect.y + 10);
  canvas.printf("T%u", static_cast<unsigned>(trackIndex + 1));
}

} // namespace ui
