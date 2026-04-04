#include "UiApp.h"

#include "theme/UiTheme.h"

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
  _playButton.pressed = _playButton.hitTest(_touch.x, _touch.y) && _touch.pressed;
  _macroRow.focus = _macroRow.hitMinus(_touch.x, _touch.y) || _macroRow.hitPlus(_touch.x, _touch.y);
  _modal.visible = _trackChip.hitTest(_touch.x, _touch.y) && _touch.pressed;
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

  canvas.fillRect(10, 46, 300, 188, theme::UiTheme::Colors::Bg);
  _playButton.draw(canvas);
  _statusCard.draw(canvas);
  _trackChip.draw(canvas);
  _macroRow.draw(canvas);
  _toast.draw(canvas);
  _modal.draw(canvas);

  canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
  canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
  canvas.setCursor(12, 176);
  canvas.printf("frame=%lu touch=%s %d,%d", static_cast<unsigned long>(nowMs), _touch.pressed ? "down" : "up", _touch.x,
                _touch.y);
}

} // namespace ui
