#include "UiToast.h"

#include "../theme/UiTheme.h"

namespace ui {

void UiToast::draw(lgfx::LGFX_Device &canvas) const {
  uint16_t fill = theme::UiTheme::Colors::Accent;
  if (severity == UiToastSeverity::Warning) {
    fill = theme::UiTheme::Colors::Warning;
  } else if (severity == UiToastSeverity::Danger) {
    fill = theme::UiTheme::Colors::Danger;
  }

  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, theme::UiTheme::Metrics::RadiusSm, fill);
  canvas.setTextColor(theme::UiTheme::Colors::TextPrimary, fill);
  canvas.setTextSize(theme::UiTheme::Typography::BodySize);
  canvas.setCursor(rect.x + 8, rect.y + 8);
  canvas.print(message);
}

} // namespace ui
