#include "UiModal.h"

#include "../theme/UiTheme.h"

namespace ui {

void UiModal::draw(lgfx::LGFX_Device &canvas) const {
  if (!visible) {
    return;
  }

  canvas.fillRect(0,
                  0,
                  theme::UiTheme::Metrics::ScreenW,
                  theme::UiTheme::Metrics::ScreenH,
                  theme::UiTheme::Colors::ModalScrim);
  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, theme::UiTheme::Metrics::RadiusMd, theme::UiTheme::Colors::Surface);
  canvas.drawRoundRect(rect.x, rect.y, rect.w, rect.h, theme::UiTheme::Metrics::RadiusMd, theme::UiTheme::Colors::Outline);

  canvas.setTextColor(theme::UiTheme::Colors::TextPrimary, theme::UiTheme::Colors::Surface);
  canvas.setTextSize(theme::UiTheme::Typography::BodySize);
  canvas.setCursor(rect.x + 8, rect.y + 8);
  canvas.print(title);
  canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Surface);
  canvas.setCursor(rect.x + 8, rect.y + 24);
  canvas.print(body);

  confirm.draw(canvas);
  cancel.draw(canvas);
}

} // namespace ui
