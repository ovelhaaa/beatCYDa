#include "UiFader.h"

#include "../theme/UiTheme.h"

namespace ui {
namespace {
uint8_t clampPercent(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 100) {
    return 100;
  }
  return static_cast<uint8_t>(value);
}
} // namespace

UiRect UiFader::expandedHitRect() const {
  UiRect expanded{};
  expanded.x = visualRect.x - hitRectExpandPx;
  expanded.y = visualRect.y - hitRectExpandPx;
  expanded.w = visualRect.w + (hitRectExpandPx * 2);
  expanded.h = visualRect.h + (hitRectExpandPx * 2);
  return expanded;
}

bool UiFader::hitTest(int16_t x, int16_t y) const {
  return expandedHitRect().contains(x, y);
}

bool UiFader::beginCapture(int16_t x, int16_t y) {
  if (!hitTest(x, y)) {
    return false;
  }

  captured = true;
  return true;
}

uint8_t UiFader::updateFromY(int16_t y) {
  if (visualRect.h <= 0) {
    value = 0;
    return value;
  }

  const int relativeY = y - visualRect.y;
  const int normalized = 100 - ((relativeY * 100) / visualRect.h);
  value = clampPercent(normalized);
  return value;
}

void UiFader::endCapture() {
  captured = false;
}

void UiFader::draw(lgfx::LGFX_Device &canvas) const {
  canvas.drawRoundRect(visualRect.x,
                       visualRect.y,
                       visualRect.w,
                       visualRect.h,
                       theme::UiTheme::Metrics::RadiusSm,
                       theme::UiTheme::Colors::Outline);

  const int fillH = (visualRect.h * value) / 100;
  const int fillY = visualRect.y + visualRect.h - fillH;
  const uint16_t fillColor = captured ? theme::UiTheme::Colors::Accent : theme::UiTheme::Colors::Surface;

  if (fillH > 0) {
    canvas.fillRoundRect(visualRect.x + 2,
                         fillY + 2,
                         visualRect.w - 4,
                         fillH > 4 ? fillH - 4 : 1,
                         theme::UiTheme::Metrics::RadiusSm,
                         fillColor);
  }

  canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
  canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
  canvas.setCursor(visualRect.x + 2, visualRect.y + visualRect.h + 4);
  canvas.printf("T%u", static_cast<unsigned>(track + 1));
}

} // namespace ui
