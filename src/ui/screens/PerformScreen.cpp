#include "PerformScreen.h"

#include "../UiAction.h"
#include "../core/UiActions.h"
#include "../theme/UiTheme.h"

namespace ui {

namespace {
UiMode screenToMode(UiScreenId screen) {
  switch (screen) {
  case UiScreenId::Pattern:
    return UiMode::PATTERN_EDIT;
  case UiScreenId::Sound:
    return UiMode::SOUND_EDIT;
  case UiScreenId::Mix:
    return UiMode::MIXER;
  case UiScreenId::Perform:
  case UiScreenId::Project:
  default:
    return UiMode::PERFORMANCE;
  }
}
} // namespace

void PerformScreen::begin() {
  _screen = UiScreenId::Perform;
  _activeTrack = 0;
  _isPlaying = true;
}

void PerformScreen::handleTouch(const TouchPoint &touch, UiInvalidation &invalidation) {
  _playButton.pressed = _playButton.hitTest(touch.x, touch.y) && touch.pressed;
  _muteButton.pressed = _muteButton.hitTest(touch.x, touch.y) && touch.pressed;

  if (!touch.justPressed) {
    return;
  }

  if (_playButton.hitTest(touch.x, touch.y)) {
    _isPlaying = !_isPlaying;
    _playButton.label = _isPlaying ? "PLAY" : "STOP";
    dispatchUiAction(UiActionType::TOGGLE_PLAY, 0, 0);
    invalidation.topBarDirty = true;
    return;
  }

  if (_muteButton.hitTest(touch.x, touch.y)) {
    dispatchUiAction(UiActionType::TOGGLE_MUTE, _activeTrack, _activeTrack);
    _trackChips[_activeTrack].muted = !_trackChips[_activeTrack].muted;
    invalidation.panelDirty = true;
    return;
  }

  for (uint8_t i = 0; i < 5; ++i) {
    if (_trackChips[i].hitTest(touch.x, touch.y)) {
      _activeTrack = i;
      dispatchUiAction(UiActionType::SELECT_TRACK, 0, i);
      for (uint8_t j = 0; j < 5; ++j) {
        _trackChips[j].selected = (j == i);
      }
      invalidation.panelDirty = true;
      return;
    }
  }

  for (uint8_t i = 0; i < 5; ++i) {
    if (_navButtons[i].hitTest(touch.x, touch.y)) {
      _screen = static_cast<UiScreenId>(i);
      for (uint8_t j = 0; j < 5; ++j) {
        _navButtons[j].variant = (j == i) ? UiButtonVariant::Primary : UiButtonVariant::Secondary;
      }
      dispatchUiAction(UiActionType::CHANGE_MODE, 0, static_cast<int>(screenToMode(_screen)));
      invalidation.bottomNavDirty = true;
      return;
    }
  }
}

void PerformScreen::render(lgfx::LGFX_Device &canvas, UiInvalidation &invalidation) {
  if (invalidation.fullScreenDirty) {
    canvas.fillScreen(theme::UiTheme::Colors::Bg);
    invalidation.topBarDirty = true;
    invalidation.panelDirty = true;
    invalidation.bottomNavDirty = true;
  }

  if (invalidation.topBarDirty) {
    drawTopBar(canvas);
    invalidation.topBarDirty = false;
  }

  if (invalidation.panelDirty) {
    drawContent(canvas);
    invalidation.panelDirty = false;
  }

  if (invalidation.bottomNavDirty) {
    drawBottomNav(canvas);
    invalidation.bottomNavDirty = false;
  }

  invalidation.fullScreenDirty = false;
}

void PerformScreen::drawTopBar(lgfx::LGFX_Device &canvas) {
  canvas.fillRect(0, 0, theme::UiTheme::Metrics::ScreenW, 78, theme::UiTheme::Colors::Bg);
  canvas.fillRoundRect(8, 8, 304, 28, theme::UiTheme::Metrics::RadiusMd, theme::UiTheme::Colors::Surface);
  canvas.setTextColor(theme::UiTheme::Colors::TextPrimary, theme::UiTheme::Colors::Surface);
  canvas.setTextSize(theme::UiTheme::Typography::BodySize);
  canvas.setCursor(16, 16);
  canvas.print("Perform");

  _playButton.draw(canvas);
  _muteButton.draw(canvas);
  _bpmCard.draw(canvas);
}

void PerformScreen::drawContent(lgfx::LGFX_Device &canvas) {
  canvas.fillRect(0, 78, theme::UiTheme::Metrics::ScreenW, 122, theme::UiTheme::Colors::Bg);

  for (const auto &chip : _trackChips) {
    chip.draw(canvas);
  }

  _macroRow.draw(canvas);

  canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
  canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
  canvas.setCursor(12, 132);
  canvas.printf("Active Track: T%u", static_cast<unsigned>(_activeTrack + 1));
  canvas.setCursor(12, 146);
  canvas.print("Sprint 3 - PerformScreen");
}

void PerformScreen::drawBottomNav(lgfx::LGFX_Device &canvas) {
  canvas.fillRect(0, 198, theme::UiTheme::Metrics::ScreenW, 42, theme::UiTheme::Colors::Bg);
  for (const auto &button : _navButtons) {
    button.draw(canvas);
  }
}

} // namespace ui
