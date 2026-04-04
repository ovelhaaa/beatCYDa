#include "UiApp.h"

#include "../CYD_Config.h"
#include "core/UiActions.h"
#include "theme/UiTheme.h"
#include <Arduino.h>

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

bool compareTrackMutes(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  for (int i = 0; i < TRACK_COUNT; ++i) {
    if (lhs.trackMutes[i] != rhs.trackMutes[i]) {
      return false;
    }
  }

  return true;
}

bool compareTrackParams(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  for (int i = 0; i < TRACK_COUNT; ++i) {
    if (lhs.trackSteps[i] != rhs.trackSteps[i] || lhs.trackHits[i] != rhs.trackHits[i] ||
        lhs.trackRotations[i] != rhs.trackRotations[i]) {
      return false;
    }
    if (lhs.voiceGain[i] != rhs.voiceGain[i]) {
      return false;
    }
    if (lhs.voiceParams[i].pitch != rhs.voiceParams[i].pitch ||
        lhs.voiceParams[i].decay != rhs.voiceParams[i].decay ||
        lhs.voiceParams[i].timbre != rhs.voiceParams[i].timbre ||
        lhs.voiceParams[i].drive != rhs.voiceParams[i].drive) {
      return false;
    }
  }

  return true;
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
  _previousSnapshot = _snapshot;
  _previousScreen = _activeScreen;
  _snapshot.capture();
  _display.readTouch(_touch);
  updateUiStats(nowMs);

  IScreen *screen = activeScreen();
  const bool contentTouched = (screen != nullptr) ? screen->handleTouch(_touch, _snapshot) : false;
  if (contentTouched) {
    _invalidation.panelDirty = true;
  }

  handleBottomNavTouch();
  screen = activeScreen();

  if (_activeScreen != _previousScreen) {
    _invalidation.invalidateAll();
    if (screen != nullptr) {
      screen->invalidate();
    }
  }

  if (detectModelChanges()) {
    _invalidation.panelDirty = true;
  }

  if (_invalidation.fullScreenDirty || _invalidation.topBarDirty) {
    renderChrome(nowMs);
  }

  if (_invalidation.fullScreenDirty || _invalidation.panelDirty) {
    if (screen != nullptr) {
      screen->render(_display.canvas(), _snapshot);
    }
  }

  if (_invalidation.fullScreenDirty || _invalidation.bottomNavDirty) {
    renderBottomNav();
  }

  _invalidation.clear();
  _hasPreviousSnapshot = true;
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
  canvas.printf("BPM %d  %uFPS %luKB", _snapshot.bpm, _uiFps, static_cast<unsigned long>(_freeHeap / 1024UL));
}

void UiApp::updateUiStats(uint32_t nowMs) {
  ++_frameCounter;
  const uint32_t elapsed = nowMs - _statsLastMs;
  if (elapsed < CYDConfig::UiStatsRefreshMs) {
    return;
  }

  _uiFps = static_cast<uint16_t>((_frameCounter * 1000UL) / elapsed);
  _frameCounter = 0;
  _statsLastMs = nowMs;
  _freeHeap = ESP.getFreeHeap();
  _invalidation.topBarDirty = true;
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
    _performScreen.invalidate();
    _invalidation.bottomNavDirty = true;
    return;
  }

  if (_navPattern.hitTest(_touch.x, _touch.y)) {
    _activeScreen = UiScreenId::Pattern;
    dispatchUiAction(UiActionType::CHANGE_MODE, 0, static_cast<int>(UiMode::PATTERN_EDIT));
    _patternScreen.invalidate();
    _invalidation.bottomNavDirty = true;
    return;
  }

  if (_navSound.hitTest(_touch.x, _touch.y)) {
    _activeScreen = UiScreenId::Sound;
    dispatchUiAction(UiActionType::CHANGE_MODE, 0, static_cast<int>(UiMode::SOUND_EDIT));
    _soundScreen.invalidate();
    _invalidation.bottomNavDirty = true;
    return;
  }

  if (_navMix.hitTest(_touch.x, _touch.y)) {
    _activeScreen = UiScreenId::Mix;
    dispatchUiAction(UiActionType::CHANGE_MODE, 0, static_cast<int>(UiMode::MIXER));
    _mixScreen.invalidate();
    _invalidation.bottomNavDirty = true;
    return;
  }

  if (_navProject.hitTest(_touch.x, _touch.y)) {
    _activeScreen = UiScreenId::Project;
    dispatchUiAction(UiActionType::CHANGE_MODE, 0, static_cast<int>(UiMode::SYSTEM));
    _projectScreen.invalidate();
    _invalidation.bottomNavDirty = true;
  }
}

IScreen *UiApp::activeScreen() {
  if (_activeScreen == UiScreenId::Perform) {
    return &_performScreen;
  }
  if (_activeScreen == UiScreenId::Pattern) {
    return &_patternScreen;
  }
  if (_activeScreen == UiScreenId::Sound) {
    return &_soundScreen;
  }
  if (_activeScreen == UiScreenId::Mix) {
    return &_mixScreen;
  }
  return &_projectScreen;
}

bool UiApp::detectModelChanges() {
  if (!_hasPreviousSnapshot) {
    _invalidation.invalidateAll();
    return true;
  }

  if (_snapshot.bpm != _previousSnapshot.bpm || _snapshot.isPlaying != _previousSnapshot.isPlaying) {
    _invalidation.topBarDirty = true;
  }

  if (_snapshot.activeTrack != _previousSnapshot.activeTrack ||
      !compareTrackMutes(_snapshot, _previousSnapshot) ||
      !compareTrackParams(_snapshot, _previousSnapshot) ||
      _snapshot.masterVolume != _previousSnapshot.masterVolume) {
    return true;
  }

  return false;
}

} // namespace ui
