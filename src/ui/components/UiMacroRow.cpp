#include "UiMacroRow.h"

#include "../theme/UiTheme.h"

namespace ui {

void UiMacroRow::draw(lgfx::LGFX_Device &canvas) const {
  const uint16_t fill = focus ? theme::UiTheme::Colors::Surface : theme::UiTheme::Colors::Bg;
  const uint16_t rowBorder = focus ? theme::UiTheme::Colors::Accent : theme::UiTheme::Colors::Outline;
  const uint16_t baseControlFill = focus ? theme::UiTheme::Colors::SurfacePressed : theme::UiTheme::Colors::Surface;
  const uint16_t minusFill = minusPressed ? theme::UiTheme::Colors::AccentPressed : baseControlFill;
  const uint16_t plusFill = plusPressed ? theme::UiTheme::Colors::AccentPressed : baseControlFill;
  const uint16_t minusText = minusPressed ? theme::UiTheme::Colors::TextOnAccent : theme::UiTheme::Colors::TextPrimary;
  const uint16_t plusText = plusPressed ? theme::UiTheme::Colors::TextOnAccent : theme::UiTheme::Colors::TextPrimary;
  canvas.fillRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, theme::UiTheme::Metrics::RadiusSm, fill);
  canvas.drawRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, theme::UiTheme::Metrics::RadiusSm, rowBorder);

  canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, fill);
  canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
  canvas.setCursor(rowRect.x + 6, rowRect.y + 4);
  canvas.print(label);

  canvas.setTextColor(theme::UiTheme::Colors::TextPrimary, fill);
  canvas.setCursor(rowRect.x + 6, rowRect.y + 16);
  canvas.print(valueText);

  canvas.fillRoundRect(minusRect.x, minusRect.y, minusRect.w, minusRect.h, theme::UiTheme::Metrics::RadiusSm, minusFill);
  canvas.drawRoundRect(minusRect.x, minusRect.y, minusRect.w, minusRect.h, theme::UiTheme::Metrics::RadiusSm,
                       theme::UiTheme::Colors::Outline);
  canvas.setTextColor(minusText, minusFill);
  canvas.setCursor(minusRect.x + 9, minusRect.y + 9);
  canvas.print("-");

  canvas.fillRoundRect(plusRect.x, plusRect.y, plusRect.w, plusRect.h, theme::UiTheme::Metrics::RadiusSm, plusFill);
  canvas.drawRoundRect(plusRect.x, plusRect.y, plusRect.w, plusRect.h, theme::UiTheme::Metrics::RadiusSm,
                       theme::UiTheme::Colors::Outline);
  canvas.setTextColor(plusText, plusFill);
  canvas.setCursor(plusRect.x + 9, plusRect.y + 9);
  canvas.print("+");

  if (showBar) {
    const int barX = rowRect.x + 84;
    const int barY = rowRect.y + rowRect.h - 10;
    const int barW = rowRect.w - 92;
    const int fillW = (barW * barFill) / 100;
    canvas.drawRect(barX, barY, barW, 6, rowBorder);
    canvas.fillRect(barX + 1, barY + 1, fillW > 1 ? fillW - 1 : 0, 4, theme::UiTheme::Colors::Accent);
  }
}

bool UiMacroRow::hitMinus(int16_t x, int16_t y) const {
  return minusRect.contains(x, y);
}

bool UiMacroRow::hitPlus(int16_t x, int16_t y) const {
  return plusRect.contains(x, y);
}

} // namespace ui
