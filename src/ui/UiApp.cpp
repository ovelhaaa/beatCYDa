#include "UiApp.h"

#include "core/UiActions.h"
#include "theme/UiTheme.h"

namespace ui {
namespace {
void setRect(UiRect &rect, int16_t x, int16_t y, int16_t w, int16_t h) {
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;
}

void styleNavButton(UiButton &button, const char *label, int16_t x) {
  button.label = label;
  button.variant = UiButtonVariant::Secondary;
  setRect(button.rect, x, theme::UiTheme::Metrics::ScreenH - theme::UiTheme::Metrics::BottomNavH + 4, 60, 32);
}
} // namespace

UiApp::UiApp() {
  styleNavButton(_navPerform, "PERF", 8);
  styleNavButton(_navPattern, "PATT", 70);
  styleNavButton(_navSound, "SND", 132);
  styleNavButton(_navMix, "MIX", 194);
  styleNavButton(_navProject, "PROJ", 256);
}

bool UiApp::begin() {
  return _display.begin();
}

void UiApp::runFrame(uint32_t nowMs) {
  _snapshot.capture();
  _display.readTouch(_touch);

  if (_activeScreen == UiScreenId::Perform) {
    _performScreen.handleTouch(_touch, _snapshot);
  }

  handleBottomNavTouch();
  renderChrome(nowMs);

  if (_activeScreen == UiScreenId::Perform) {
    _performScreen.render(_display.canvas(), _snapshot);
  }

  renderBottomNav();
}

void UiApp::renderChrome(uint32_t nowMs) {
  auto &canvas = _display.canvas();
  canvas.fillScreen(theme::UiTheme::Colors::Bg);
  canvas.fillRoundRect(theme::UiTheme::Metrics::OuterMargin,
                       theme::UiTheme::Metrics::OuterMargin,
                       theme::UiTheme::Metrics::ScreenW - theme::UiTheme::Metrics::OuterMargin * 2,
                       theme::UiTheme::Metrics::TopBarH,
                       theme::UiTheme::Metrics::RadiusMd,
                       theme::UiTheme::Colors::Surface);

  canvas.setTextColor(theme::UiTheme::Colors::TextPrimary, theme::UiTheme::Colors::Surface);
  canvas.setTextSize(theme::UiTheme::Typography::BodySize);
  canvas.setCursor(16, 16);
  canvas.printf("beatCYDa %s", _snapshot.isPlaying ? "PLAY" : "STOP");

  canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Surface);
  canvas.setCursor(190, 16);
  canvas.printf("BPM %d  %lums", _snapshot.bpm, static_cast<unsigned long>(nowMs));
}

void UiApp::renderBottomNav() {
  auto &canvas = _display.canvas();
  canvas.fillRect(0,
                  theme::UiTheme::Metrics::ScreenH - theme::UiTheme::Metrics::BottomNavH,
                  theme::UiTheme::Metrics::ScreenW,
                  theme::UiTheme::Metrics::BottomNavH,
                  theme::UiTheme::Colors::Surface);

  _navPerform.pressed = (_activeScreen == UiScreenId::Perform);
  _navPattern.pressed = (_activeScreen == UiScreenId::Pattern);
  _navSound.pressed = (_activeScreen == UiScreenId::Sound);
  _navMix.pressed = (_activeScreen == UiScreenId::Mix);
  _navProject.pressed = (_activeScreen == UiScreenId::Project);

  _navPerform.draw(canvas);
  _navPattern.draw(canvas);
  _navSound.draw(canvas);
  _navMix.draw(canvas);
  _navProject.draw(canvas);
}

void UiApp::handleBottomNavTouch() {
  if (!_touch.justPressed) {
    return;
  }

  if (_navPerform.hitTest(_touch.x, _touch.y)) {
    _activeScreen = UiScreenId::Perform;
    dispatchUiAction(UiActionType::CHANGE_MODE, 0, static_cast<int>(UiMode::PERFORMANCE));
    return;
  }

  if (_navPattern.hitTest(_touch.x, _touch.y)) {
    _activeScreen = UiScreenId::Pattern;
    dispatchUiAction(UiActionType::CHANGE_MODE, 0, static_cast<int>(UiMode::PATTERN_EDIT));
    return;
  }

  if (_navSound.hitTest(_touch.x, _touch.y)) {
    _activeScreen = UiScreenId::Sound;
    dispatchUiAction(UiActionType::CHANGE_MODE, 0, static_cast<int>(UiMode::SOUND_EDIT));
    return;
  }

  if (_navMix.hitTest(_touch.x, _touch.y)) {
    _activeScreen = UiScreenId::Mix;
    return;
  }

  if (_navProject.hitTest(_touch.x, _touch.y)) {
    _activeScreen = UiScreenId::Project;
  }
}

} // namespace ui
