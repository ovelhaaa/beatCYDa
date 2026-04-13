#include "UiMacroRow.h"

#include "../theme/UiTheme.h"
#include <Arduino.h>

namespace ui {
namespace {
UiRect sliderRectForRow(const UiRect &rowRect) {
  UiRect slider{};
  slider.x = rowRect.x + 110;
  slider.y = rowRect.y + rowRect.h - 10;
  slider.w = rowRect.w - 120;
  slider.h = 6;
  return slider;
}
} // namespace

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
  canvas.setCursor(rowRect.x + 6, rowRect.y + 6);
  canvas.print(label);
  canvas.print(":");
  canvas.print(" ");
  canvas.setTextColor(theme::UiTheme::Colors::TextPrimary, fill);
  canvas.print(valueText);

  if (minusRect.w > 0 && minusRect.h > 0 && plusRect.w > 0 && plusRect.h > 0) {
    canvas.fillRoundRect(minusRect.x, minusRect.y, minusRect.w, minusRect.h, theme::UiTheme::Metrics::RadiusSm, minusFill);
    canvas.drawRoundRect(minusRect.x, minusRect.y, minusRect.w, minusRect.h, theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::Outline);
    canvas.setTextColor(minusText, minusFill);
    const int minusTextX = minusRect.x + (minusRect.w / 2) - 3;
    const int minusTextY = minusRect.y + (minusRect.h / 2) - 4;
    canvas.setCursor(minusTextX, minusTextY);
    canvas.print("-");

    canvas.fillRoundRect(plusRect.x, plusRect.y, plusRect.w, plusRect.h, theme::UiTheme::Metrics::RadiusSm, plusFill);
    canvas.drawRoundRect(plusRect.x, plusRect.y, plusRect.w, plusRect.h, theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::Outline);
    canvas.setTextColor(plusText, plusFill);
    const int plusTextX = plusRect.x + (plusRect.w / 2) - 3;
    const int plusTextY = plusRect.y + (plusRect.h / 2) - 4;
    canvas.setCursor(plusTextX, plusTextY);
    canvas.print("+");
  }

  if (showBar) {
    UiRect sliderRect = sliderRectForRow(rowRect);
    if (minusRect.w > 0 && minusRect.h > 0 && minusRect.x > sliderRect.x) {
      sliderRect.w = (minusRect.x - 8) - sliderRect.x;
    }
    const int barW = sliderRect.w;
    const int barY = sliderRect.y;

    if (barW > 4) {
      const int fillW = (barW * barFill) / 100;
      canvas.drawRect(sliderRect.x, barY, barW, sliderRect.h, rowBorder);
      canvas.fillRect(sliderRect.x + 1,
                      barY + 1,
                      fillW > 1 ? fillW - 1 : 0,
                      sliderRect.h - 2,
                      theme::UiTheme::Colors::Accent);
    }
  }
}

bool UiMacroRow::hitMinus(int16_t x, int16_t y) const { return minusRect.contains(x, y); }

bool UiMacroRow::hitPlus(int16_t x, int16_t y) const { return plusRect.contains(x, y); }

bool UiMacroRow::hitSlider(int16_t x, int16_t y) const {
  return sliderRectForRow(rowRect).contains(x, y);
}

uint8_t UiMacroRow::sliderPercentAt(int16_t x) const {
  const UiRect sliderRect = sliderRectForRow(rowRect);
  if (sliderRect.w <= 0) {
    return 0;
  }
  const int clamped = constrain(x, sliderRect.x, sliderRect.x + sliderRect.w);
  const int pct = ((clamped - sliderRect.x) * 100) / sliderRect.w;
  return static_cast<uint8_t>(constrain(pct, 0, 100));
}

} // namespace ui
