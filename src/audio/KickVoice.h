#ifndef KICK_VOICE_H
#define KICK_VOICE_H

#include "DSP.h"
#include "esp_attr.h"
#include <Arduino.h>

struct KickParams {
  float tune;   // 0..1 (Base Pitch)
  float length; // 0..1 (Decay Length)
  float punch;  // 0..1 (Pitch Sweep + Click Depth)
  float drive;  // 0..1 (Saturation)
};

class KickVoice {
public:
  void init(float sampleRate);
  void setParams(const KickParams &p);
  void trigger(float velocity = 1.0f);

  // High-performance IRAM loop
  IRAM_ATTR float process();

  bool isActive() const { return active; }
  float getEnvelope() const { return envGain; }

private:
  // State
  bool active;
  float srInv;

  // Envelopes (Recursive)
  float envGain;
  float gainMul; // Pre-calculated

  float envPitch;
  float pitchMul; // Pre-calculated

  float clickEnv; // Deterministic Click

  // Oscillator
  float phase;
  float baseFreq;
  float sweepAmount;

  // DSP State
  float driveFactor;

  // SVF (State Variable Filter) for tonal control
  float svfLow;
  float svfBand;

  // DC Blocker
  float dcX1;
  float dcY1;

  // Pre-calculated filter coeffs
  float cutoffBase;
  float svfQ; // Resonance

  KickParams params;

  // No internal helper methods called in process loop for valid IRAM usage
  // unless inline or IRAM_ATTR.
};

#endif
