#include "UiState.h"
#include <string.h>

void setStatus(UiRuntime &ui, const char *msg, uint32_t ms) {
  snprintf(ui.status, sizeof(ui.status), "%s", msg);
  ui.statusUntilMs = millis() + ms;
  ui.invalidation.toastDirty = true;
}

void updateStatusTimeout(UiRuntime &ui, uint32_t now) {
  if (ui.statusUntilMs && now >= ui.statusUntilMs) {
    ui.statusUntilMs = 0;
    snprintf(ui.status, sizeof(ui.status), "%s", ui.snapshot.isPlaying ? "PLAYING" : "READY");
    ui.forceRedraw = true;
    ui.invalidation.topBarDirty = true;
    ui.invalidation.toastDirty = true;
  }
}

void updateUiInvalidation(UiRuntime &ui, uint32_t now, uint32_t lastFullMs) {
  if (ui.forceRedraw || (now - lastFullMs > CYDConfig::UiLegacyFullRefreshFallbackMs)) {
    ui.invalidation.markAll();
    return;
  }

  if (ui.snapshot.bpm != ui.lastSnapshot.bpm || ui.snapshot.isPlaying != ui.lastSnapshot.isPlaying ||
      ui.activeSlot != ui.lastActiveSlot) {
    ui.invalidation.topBarDirty = true;
  }

  if (ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack || ui.mode != ui.lastMode) {
    ui.invalidation.ringDirty = true;
    ui.invalidation.panelDirty = true;
    ui.invalidation.bottomNavDirty = true;
  }

  if (ui.snapshot.currentStep != ui.lastSnapshot.currentStep) {
    ui.invalidation.ringDirty = true;
  }

  if (strcmp(ui.status, ui.lastStatus) != 0) {
    ui.invalidation.toastDirty = true;
  }

  for (int i = 0; i < TRACK_COUNT; ++i) {
    if (ui.snapshot.trackMutes[i] != ui.lastSnapshot.trackMutes[i]) {
      ui.invalidation.ringDirty = true;
      ui.invalidation.panelDirty = true;
      ui.invalidation.bottomNavDirty = true;
      break;
    }
  }

#ifndef NDEBUG
  if (memcmp(&ui.snapshot, &ui.lastSnapshot, sizeof(UiStateSnapshot)) != 0 || ui.mode != ui.lastMode ||
      ui.activeSlot != ui.lastActiveSlot || strcmp(ui.status, ui.lastStatus) != 0) {
    if (!ui.invalidation.topBarDirty && !ui.invalidation.ringDirty && !ui.invalidation.panelDirty &&
        !ui.invalidation.bottomNavDirty && !ui.invalidation.toastDirty && !ui.invalidation.fullScreenDirty) {
      ui.invalidation.markAll();
    }
  }
#endif
}

void commitFrame(UiRuntime &ui) {
  ui.lastSnapshot = ui.snapshot;
  ui.lastMode = ui.mode;
  ui.lastActiveSlot = ui.activeSlot;
  snprintf(ui.lastStatus, sizeof(ui.lastStatus), "%s", ui.status);
  ui.invalidation.clear();
}

int getParamDisplayValue(const UiRuntime &ui, int paramIndex) {
  const int track = ui.snapshot.activeTrack;
  const bool isBass = track == VOICE_BASS;
  const VoiceParams &params = ui.snapshot.voiceParams[track];

  if (ui.mode == UiMode::SOUND_EDIT) {
    if (isBass) {
      switch (paramIndex) {
      case 0:
        return static_cast<int>(params.pitch * 100.0f);
      case 1:
        return static_cast<int>(params.decay * 100.0f);
      case 2:
        return static_cast<int>(params.timbre * 100.0f);
      default:
        return static_cast<int>(params.drive * 100.0f);
      }
    }

    switch (paramIndex) {
    case 0:
      return static_cast<int>(params.pitch * 100.0f);
    case 1:
      return static_cast<int>(params.decay * 100.0f);
    case 2:
      return static_cast<int>(params.timbre * 100.0f);
    default:
      return (track == VOICE_KICK) ? static_cast<int>(params.drive * 100.0f)
                                   : static_cast<int>(ui.snapshot.voiceGain[track] * 100.0f);
    }
  }

  if (isBass) {
    switch (paramIndex) {
    case 0:
      return static_cast<int>(ui.snapshot.bassParams.density * 100.0f);
    case 1:
      return static_cast<int>(((ui.snapshot.bassParams.range - 1) / 11.0f) * 100.0f);
    case 2:
      return static_cast<int>(static_cast<uint8_t>(ui.snapshot.bassParams.scaleType) *
                              (100.0f / 3.0f));
    case 3:
      return static_cast<int>(((ui.snapshot.bassParams.rootNote - 24) / 24.0f) * 100.0f);
    case 4:
      return static_cast<int>(static_cast<uint8_t>(ui.snapshot.bassParams.mode) *
                              (100.0f / 3.0f));
    case 5:
      return static_cast<int>((ui.snapshot.bassParams.motifIndex & 0x03) * (100.0f / 3.0f));
    case 6:
      return static_cast<int>(ui.snapshot.bassParams.swing * 100.0f);
    case 7:
      return static_cast<int>(ui.snapshot.bassParams.accentProb * 100.0f);
    case 8:
      return static_cast<int>(ui.snapshot.bassParams.ghostProb * 100.0f);
    case 9:
      return static_cast<int>(ui.snapshot.bassParams.phraseVariation * 100.0f);
    case 10:
      return static_cast<int>(ui.snapshot.bassParams.slideProb * 100.0f);
    default:
      return static_cast<int>(ui.snapshot.bassParams.density * 100.0f);
    }
  }

  switch (paramIndex) {
  case 0:
    return ui.snapshot.trackSteps[track];
  case 1:
    return ui.snapshot.trackHits[track];
  case 2:
    return ui.snapshot.trackRotations[track];
  default:
    return static_cast<int>(ui.snapshot.voiceGain[track] * 100.0f);
  }
}