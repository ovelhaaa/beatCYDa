#include "UiCard.h"

#include "../theme/UiTheme.h"

namespace ui {

void UiCard::draw(lgfx::LGFX_Device &canvas) const {
  const uint16_t fill = active ? theme::UiTheme::Colors::Accent : theme::UiTheme::Colors::Surface;
  const uint16_t titleText = active ? theme::UiTheme::Colors::TextOnAccent : theme::UiTheme::Colors::TextSecondary;
  const uint16_t valueText = active ? theme::UiTheme::Colors::TextOnAccent : theme::UiTheme::Colors::TextPrimary;

  canvas.fillRoundRect(rect.x, rect.y, rect.w, rect.h, theme::UiTheme::Metrics::RadiusMd, fill);
  canvas.drawRoundRect(rect.x, rect.y, rect.w, rect.h, theme::UiTheme::Metrics::RadiusMd,
                       theme::UiTheme::Colors::Outline);

  canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
  canvas.setTextColor(titleText, fill);
  canvas.setCursor(rect.x + 8, rect.y + 6);
  canvas.print(title);

  canvas.setTextSize(theme::UiTheme::Typography::TitleSize);
  canvas.setTextColor(valueText, fill);
  canvas.setCursor(rect.x + 8, rect.y + 20);
  canvas.print(value);
}

} // namespace ui
