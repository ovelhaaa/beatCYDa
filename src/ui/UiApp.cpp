#include "UiApp.h"

#include "../CYD_Config.h"
#include "../control/ControlManager.h"
#include "core/UiActions.h"
#include "core/BassUiFormat.h"
#include "theme/UiTheme.h"
#include <Arduino.h>
#include <string.h>

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
  setRect(button.rect,
          x,
          theme::UiTheme::Metrics::ScreenH - theme::UiTheme::Metrics::BottomNavH +
              theme::UiTheme::Metrics::BottomNavButtonYInset,
          theme::UiTheme::Metrics::BottomNavButtonW,
          theme::UiTheme::Metrics::BottomNavButtonH);
}

bool compareTrackMutes(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  for (int i = 0; i < TRACK_COUNT; ++i) {
    if (lhs.trackMutes[i] != rhs.trackMutes[i]) {
      return false;
    }
  }

  return true;
}

bool comparePatternData(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  for (int track = 0; track < TRACK_COUNT; ++track) {
    if (lhs.patternLens[track] != rhs.patternLens[track]) {
      return false;
    }
    for (int step = 0; step < 64; ++step) {
      if (lhs.patterns[track][step] != rhs.patterns[track][step]) {
        return false;
      }
    }
  }

  return true;
}

bool hasTrackSelectionChanges(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  return lhs.activeTrack != rhs.activeTrack;
}

bool hasTransportChanges(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  return lhs.isPlaying != rhs.isPlaying || lhs.currentStep != rhs.currentStep;
}

bool hasPerformPanelChanges(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  return hasTrackSelectionChanges(lhs, rhs) || hasTransportChanges(lhs, rhs) || lhs.bpm != rhs.bpm ||
         !compareTrackMutes(lhs, rhs) || !comparePatternData(lhs, rhs);
}

bool hasPatternPanelChanges(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  if (hasTrackSelectionChanges(lhs, rhs) || !compareTrackMutes(lhs, rhs) || !comparePatternData(lhs, rhs)) {
    return true;
  }

  const uint8_t activeTrack = lhs.activeTrack;
  if (activeTrack == VOICE_BASS) {
    return lhs.bassParams.mode != rhs.bassParams.mode ||
           lhs.bassParams.motifIndex != rhs.bassParams.motifIndex ||
           lhs.bassParams.swing != rhs.bassParams.swing ||
           lhs.bassParams.ghostProb != rhs.bassParams.ghostProb ||
           lhs.bassParams.accentProb != rhs.bassParams.accentProb;
  }


  return lhs.trackSteps[activeTrack] != rhs.trackSteps[activeTrack] ||
         lhs.trackHits[activeTrack] != rhs.trackHits[activeTrack] ||
         lhs.trackRotations[activeTrack] != rhs.trackRotations[activeTrack] ||
         lhs.voiceGain[activeTrack] != rhs.voiceGain[activeTrack];
}

bool hasSoundPanelChanges(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  if (hasTrackSelectionChanges(lhs, rhs) || !compareTrackMutes(lhs, rhs)) {
    return true;
  }

  const uint8_t activeTrack = lhs.activeTrack;
  if (activeTrack == VOICE_BASS) {
    return lhs.voiceParams[activeTrack].pitch != rhs.voiceParams[activeTrack].pitch ||
           lhs.voiceParams[activeTrack].decay != rhs.voiceParams[activeTrack].decay ||
           lhs.voiceParams[activeTrack].timbre != rhs.voiceParams[activeTrack].timbre ||
           lhs.voiceParams[activeTrack].drive != rhs.voiceParams[activeTrack].drive ||
           lhs.bassParams.mode != rhs.bassParams.mode || lhs.bassParams.motifIndex != rhs.bassParams.motifIndex ||
           lhs.bassParams.swing != rhs.bassParams.swing || lhs.bassParams.accentProb != rhs.bassParams.accentProb ||
           lhs.bassParams.ghostProb != rhs.bassParams.ghostProb ||
           lhs.bassParams.phraseVariation != rhs.bassParams.phraseVariation;
  }
  return lhs.voiceParams[activeTrack].pitch != rhs.voiceParams[activeTrack].pitch ||
         lhs.voiceParams[activeTrack].decay != rhs.voiceParams[activeTrack].decay ||
         lhs.voiceParams[activeTrack].timbre != rhs.voiceParams[activeTrack].timbre ||
         lhs.voiceParams[activeTrack].drive != rhs.voiceParams[activeTrack].drive;
}

bool hasMixPanelChanges(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  if (lhs.masterVolume != rhs.masterVolume) {
    return true;
  }

  for (int i = 0; i < TRACK_COUNT; ++i) {
    if (lhs.voiceGain[i] != rhs.voiceGain[i]) {
      return true;
    }
  }

  return false;
}

bool hasProjectPanelChanges(const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  return lhs.bpm != rhs.bpm || lhs.activeTrack != rhs.activeTrack;
}

bool hasPanelChangesForScreen(UiScreenId screenId, const UiStateSnapshot &lhs, const UiStateSnapshot &rhs) {
  if (screenId == UiScreenId::Perform) {
    return hasPerformPanelChanges(lhs, rhs);
  }
  if (screenId == UiScreenId::Pattern) {
    return hasPatternPanelChanges(lhs, rhs);
  }
  if (screenId == UiScreenId::Sound) {
    return hasSoundPanelChanges(lhs, rhs);
  }
  if (screenId == UiScreenId::Mix) {
    return hasMixPanelChanges(lhs, rhs);
  }
  if (screenId == UiScreenId::Project) {
    return hasProjectPanelChanges(lhs, rhs);
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
  processStorageFeedback(nowMs);
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

  if (screen != nullptr && screen->wantsContinuousRedraw(nowMs)) {
    _invalidation.panelDirty = true;
  }

  if (detectModelChanges()) {
    _invalidation.panelDirty = true;
  }

  if (_invalidation.fullScreenDirty) {
    renderBackground();
    renderTopBarShell();
  }

  if (_invalidation.fullScreenDirty || _invalidation.topBarDirty) {
    if (_invalidation.fullScreenDirty || _topBarTransportDirty) {
      renderTopBarTransport();
    }
    if (_invalidation.fullScreenDirty || _topBarMetricsDirty) {
      renderTopBarMetrics();
    }
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
  _topBarTransportDirty = false;
  _topBarMetricsDirty = false;
  _hasPreviousSnapshot = true;
}

void UiApp::renderBackground() {
  auto &canvas = _display.canvas();
  canvas.fillScreen(theme::UiTheme::Colors::Bg);
}

void UiApp::renderTopBarShell() {
  auto &canvas = _display.canvas();
  canvas.fillRoundRect(theme::UiTheme::Metrics::OuterMargin,
                       theme::UiTheme::Metrics::OuterMargin,
                       theme::UiTheme::Metrics::ScreenW - theme::UiTheme::Metrics::OuterMargin * 2,
                       theme::UiTheme::Metrics::TopBarH,
                       theme::UiTheme::Metrics::RadiusMd,
                       theme::UiTheme::Colors::Surface);
}

void UiApp::renderTopBarTransport() {
  auto &canvas = _display.canvas();
  canvas.fillRect(theme::UiTheme::Metrics::TopBarTransportX,
                  theme::UiTheme::Metrics::TopBarTransportY,
                  theme::UiTheme::Metrics::TopBarTransportW,
                  theme::UiTheme::Metrics::TopBarTransportH,
                  theme::UiTheme::Colors::Surface);
  canvas.setTextColor(theme::UiTheme::Colors::TextPrimary, theme::UiTheme::Colors::Surface);
  canvas.setTextSize(theme::UiTheme::Typography::BodySize);
  canvas.setCursor(theme::UiTheme::Metrics::TopBarTransportX, theme::UiTheme::Metrics::TopBarTextBaselineY);
  const bool hasTransientStatus =
      _transientStatusUntilMs > millis() && _transientStatus[0] != '\0';
  canvas.printf("beatCYDa %s", hasTransientStatus ? _transientStatus : (_snapshot.isPlaying ? "PLAY" : "STOP"));
}

void UiApp::renderTopBarMetrics() {
  auto &canvas = _display.canvas();
  canvas.fillRect(theme::UiTheme::Metrics::TopBarMetricsX,
                  theme::UiTheme::Metrics::TopBarMetricsY,
                  theme::UiTheme::Metrics::TopBarMetricsW,
                  theme::UiTheme::Metrics::TopBarMetricsH,
                  theme::UiTheme::Colors::Surface);
  canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Surface);
  canvas.setCursor(theme::UiTheme::Metrics::TopBarMetricsX + 2, theme::UiTheme::Metrics::TopBarTextBaselineY);
  const uint8_t root = _snapshot.bassParams.rootNote;
  canvas.printf("BPM %d %s%d %s %uFPS %luKB",
                _snapshot.bpm,
                bassfmt::noteName(root),
                bassfmt::noteOctave(root),
                bassfmt::modeShortName(_snapshot.bassParams.mode),
                _uiFps,
                static_cast<unsigned long>(_freeHeap / 1024UL));
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
  _topBarMetricsDirty = true;
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
  _navPerform.variant = _navPerform.pressed ? UiButtonVariant::Primary : UiButtonVariant::Secondary;
  _navPattern.variant = _navPattern.pressed ? UiButtonVariant::Primary : UiButtonVariant::Secondary;
  _navSound.variant = _navSound.pressed ? UiButtonVariant::Primary : UiButtonVariant::Secondary;
  _navMix.variant = _navMix.pressed ? UiButtonVariant::Primary : UiButtonVariant::Secondary;
  _navProject.variant = _navProject.pressed ? UiButtonVariant::Primary : UiButtonVariant::Secondary;

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
    if (_snapshot.bpm != _previousSnapshot.bpm) {
      _topBarMetricsDirty = true;
    }
    if (_snapshot.isPlaying != _previousSnapshot.isPlaying) {
      _topBarTransportDirty = true;
    }
  }

  return hasPanelChangesForScreen(_activeScreen, _snapshot, _previousSnapshot);
}

void UiApp::processStorageFeedback(uint32_t nowMs) {
  ControlManager::StorageEvent event{};
  while (CtrlMgr.pollStorageEvent(event)) {
    if (event.type == ControlManager::StorageEventType::SAVE_SLOT_DONE) {
      setTransientStatus(event.success ? "Saved!" : "Save Fail",
                         event.success ? CYDConfig::UiToastInfoMs : CYDConfig::UiToastWarningMs);
    } else if (event.type == ControlManager::StorageEventType::LOAD_SLOT_DONE) {
      setTransientStatus(event.success ? "Loaded!" : "Load Fail",
                         event.success ? CYDConfig::UiToastInfoMs : CYDConfig::UiToastWarningMs);
    }
  }

  if (_transientStatusUntilMs != 0 && nowMs >= _transientStatusUntilMs) {
    _transientStatusUntilMs = 0;
    _transientStatus[0] = '\0';
    _invalidation.topBarDirty = true;
    _topBarTransportDirty = true;
  }
}

void UiApp::setTransientStatus(const char *message, uint32_t timeoutMs) {
  snprintf(_transientStatus, sizeof(_transientStatus), "%s", message);
  _transientStatusUntilMs = millis() + timeoutMs;
  _invalidation.topBarDirty = true;
  _topBarTransportDirty = true;
}

} // namespace ui
