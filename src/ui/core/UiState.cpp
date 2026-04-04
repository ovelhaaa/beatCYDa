#include "UiState.h"
#include <string.h>

void setStatus(UiRuntime &ui, const char *msg, uint32_t ms) {
  snprintf(ui.status, sizeof(ui.status), "%s", msg);
  ui.statusUntilMs = millis() + ms;
}

void updateStatusTimeout(UiRuntime &ui, uint32_t now) {
  if (ui.statusUntilMs && now >= ui.statusUntilMs) {
    ui.statusUntilMs = 0;
    snprintf(ui.status, sizeof(ui.status), "%s", ui.snapshot.isPlaying ? "PLAYING" : "READY");
    ui.forceRedraw = true;
  }
}

UiFramePlan buildFramePlan(const UiRuntime &ui, uint32_t now, uint32_t lastFullMs) {
  const bool slidersDirty =
      ui.activeHoldParam != -1 ||
      memcmp(ui.snapshot.trackSteps, ui.lastSnapshot.trackSteps, sizeof(ui.snapshot.trackSteps)) != 0 ||
      memcmp(ui.snapshot.trackHits, ui.lastSnapshot.trackHits, sizeof(ui.snapshot.trackHits)) != 0 ||
      memcmp(ui.snapshot.trackRotations, ui.lastSnapshot.trackRotations, sizeof(ui.snapshot.trackRotations)) != 0 ||
      memcmp(ui.snapshot.voiceParams, ui.lastSnapshot.voiceParams, sizeof(ui.snapshot.voiceParams)) != 0 ||
      memcmp(ui.snapshot.voiceGain, ui.lastSnapshot.voiceGain, sizeof(ui.snapshot.voiceGain)) != 0 ||
      memcmp(&ui.snapshot.bassParams, &ui.lastSnapshot.bassParams, sizeof(BassGrooveParams)) != 0;

  UiFramePlan plan;
  plan.anyChange = slidersDirty || (ui.snapshot.bpm != ui.lastSnapshot.bpm) ||
                   (ui.snapshot.isPlaying != ui.lastSnapshot.isPlaying) ||
                   (ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack) ||
                   (ui.mode != ui.lastMode) || (ui.activeSlot != ui.lastActiveSlot) ||
                   (strcmp(ui.status, ui.lastStatus) != 0) ||
                   memcmp(ui.snapshot.trackMutes, ui.lastSnapshot.trackMutes, sizeof(ui.snapshot.trackMutes)) != 0;

  plan.patternChange =
      memcmp(ui.snapshot.patterns, ui.lastSnapshot.patterns, sizeof(ui.snapshot.patterns)) != 0 ||
      memcmp(ui.snapshot.trackSteps, ui.lastSnapshot.trackSteps, sizeof(ui.snapshot.trackSteps)) != 0 ||
      ui.snapshot.activeTrack != ui.lastSnapshot.activeTrack;

  plan.stepChange = (ui.snapshot.currentStep != ui.lastSnapshot.currentStep);
  plan.full = ui.forceRedraw || (now - lastFullMs > 8000u);
  return plan;
}

void commitFrame(UiRuntime &ui) {
  ui.lastSnapshot = ui.snapshot;
  ui.lastMode = ui.mode;
  ui.lastActiveSlot = ui.activeSlot;
  snprintf(ui.lastStatus, sizeof(ui.lastStatus), "%s", ui.status);
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
      return ui.snapshot.bassParams.range;
    case 2:
      return static_cast<int>(ui.snapshot.bassParams.scaleType);
    default:
      return ui.snapshot.bassParams.rootNote;
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
