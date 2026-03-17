#ifndef BASS_VOICE_H
#define BASS_VOICE_H

#include "DSP.h"
#include "esp_attr.h"
#include <Arduino.h>

// Use global LUT_SIZE and LUT_MASK from DSP.h

struct BassParams {
  float freq;       // Hz
  float release;    // 0..1 (Amp envelope tail)
  float brightness; // 0..1 (Filter base open)
  float harmonics;  // 0..1 (Phase distortion / wave shape)
  float drive;      // 0..1 (Saturation amount)
  float snap;       // 0..1 (Filter envelope amount)
};

class BassVoice {
public:
  void init(float sampleRate);
  void setParams(const BassParams &p);
  void trigger(float velocity = 1.0f);
  void trigger(float freq, float velocity);
  void setFreq(float freq);

  // High-performance DSP loop
  IRAM_ATTR float process();

  bool isActive() const { return active; }

private:
  bool active;
  bool legato;
  float srInv;
  BassParams params;

  // Glide State
  float currentFreq;
  float glideRate; // pre-calc

  // Envelopes
  float env;
  float envMul; // pre-calc
  float filtEnv;

  // Oscillator
  float phase;
  float subPhase; // Dedicated state for Actuall Sub (Freq/2)

  // SVF (State Variable Filter) - 303-style resonant filter
  float svfLow;  // Lowpass output state
  float svfBand; // Bandpass state
  float svfQ;    // Resonance coefficient (0.5 to 2.0)

  // DC Block
  float xm1, ym1;

  // Pre-calculated DSP coefficients
  float driveGain;
  float cutoffBase;

  // Inline helper using global sinLUT
  inline float getBassSin(float p) {
    int idx = (int)(p * LUT_SIZE);
    return sinLUT[idx & LUT_MASK];
  }
};

#endif
