#include "UiApp.h"

#include "theme/UiTheme.h"
#include <Arduino.h>

namespace ui {

bool UiApp::begin() {
  if (!_display.begin()) {
    return false;
  }

  _invalidation.invalidateAll();
  return true;
}

void UiApp::runFrame(uint32_t nowMs) {
  _display.readTouch(_touch);
  renderSmokeTest(nowMs);
}

void UiApp::renderSmokeTest(uint32_t nowMs) {
  auto &canvas = _display.canvas();
  if (_invalidation.fullScreenDirty) {
    canvas.fillScreen(theme::UiTheme::Colors::Bg);
    canvas.fillRoundRect(theme::UiTheme::Metrics::OuterMargin,
                         theme::UiTheme::Metrics::OuterMargin,
                         theme::UiTheme::Metrics::ScreenW - theme::UiTheme::Metrics::OuterMargin * 2,
                         theme::UiTheme::Metrics::TopBarH,
                         theme::UiTheme::Metrics::RadiusMd,
                         theme::UiTheme::Colors::Surface);
    canvas.setTextColor(theme::UiTheme::Colors::TextPrimary, theme::UiTheme::Colors::Surface);
    canvas.setTextSize(theme::UiTheme::Typography::TitleSize);
    canvas.setCursor(16, 16);
    canvas.print("beatCYDa New UI");
    _invalidation.fullScreenDirty = false;
  }

  canvas.fillRect(10, 56, 300, 72, theme::UiTheme::Colors::Bg);
  canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
  canvas.setTextSize(theme::UiTheme::Typography::BodySize);
  canvas.setCursor(12, 58);
  canvas.printf("smoke test frame=%lu", static_cast<unsigned long>(nowMs));

  canvas.setCursor(12, 74);
  canvas.printf("touch: %s x=%d y=%d", _touch.pressed ? "down" : "up", _touch.x, _touch.y);

  if (_touch.pressed) {
    canvas.fillCircle(_touch.x, _touch.y, 6, theme::UiTheme::Colors::Accent);
  }
}

} // namespace ui
