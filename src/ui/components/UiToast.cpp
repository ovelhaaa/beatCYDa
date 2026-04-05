#include "UiToast.h"

#include "../theme/UiTheme.h"

namespace ui {

void UiToast::draw(lgfx::LGFX_Device &canvas) const {
  uint16_t fill = theme::UiTheme::Colors::Accent;
  uint16_t text = theme::UiTheme::Colors::TextOnAccent;
  if (severity == UiToastSeverity::Warning) {
    fill = theme::UiTheme::Colors::Warning;
    text = theme::UiTheme::Colors::TextOnWarning;
  } else if (severity == UiToastSeverity::Danger) {
    fill = theme::UiTheme::Colors::Danger;
    text = theme::UiTheme::Colors::TextOnDanger;
  }

  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, theme::UiTheme::Metrics::RadiusSm, fill);
  canvas.setTextColor(text, fill);
  canvas.setTextSize(theme::UiTheme::Typography::BodySize);
  canvas.setCursor(rect.x + 8, rect.y + 8);
  canvas.print(message);
}

} // namespace ui
