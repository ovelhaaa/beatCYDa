#include "UiApp.h"

#include "theme/UiTheme.h"

namespace ui {

UiApp::UiApp() {
  _playButton.label = "PLAY";
  _playButton.variant = UiButtonVariant::Primary;
  _playButton.rect = UiRect{12, 58, 72, 34};

  _statusCard.title = "STATUS";
  _statusCard.value = "NEW UI";
  _statusCard.active = true;
  _statusCard.rect = UiRect{92, 58, 104, 58};

  _trackChip.trackIndex = 0;
  _trackChip.active = true;
  _trackChip.muted = false;
  _trackChip.selected = true;
  _trackChip.rect = UiRect{204, 58, 56, 34};

  _macroRow.label = "STEPS";
  _macroRow.valueText = "16";
  _macroRow.minusRect = UiRect{208, 106, 24, 24};
  _macroRow.plusRect = UiRect{282, 106, 24, 24};
  _macroRow.rowRect = UiRect{92, 100, 216, 38};
  _macroRow.showBar = true;
  _macroRow.focus = false;
  _macroRow.barFill = 72;

  _toast.message = "Sprint 2 em andamento";
  _toast.severity = UiToastSeverity::Info;
  _toast.timeoutMs = 1400;
  _toast.rect = UiRect{12, 196, 220, 28};

  _modal.title = "Confirmar";
  _modal.body = "Salvar projeto?";
  _modal.confirm.label = "OK";
  _modal.confirm.variant = UiButtonVariant::Primary;
  _modal.confirm.rect = UiRect{122, 156, 56, 28};
  _modal.cancel.label = "X";
  _modal.cancel.variant = UiButtonVariant::Secondary;
  _modal.cancel.rect = UiRect{184, 156, 56, 28};
  _modal.rect = UiRect{96, 120, 152, 72};
  _modal.visible = false;
}

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
