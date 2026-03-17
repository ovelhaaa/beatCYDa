#ifndef HATS_VOICE_H
#define HATS_VOICE_H

#include "DSP.h"
#include <Arduino.h>

struct HatsParams {
  float decay;  // 0..1
  float timbre; // 0..1 (Metallic/Noise balance)
  bool open;    // Mode
};

class HatsVoice {
public:
  void init(float sampleRate);
  void setParams(const HatsParams &p);
  void trigger(float velocity = 1.0f);
  void choke() {
    active = false;
    env = 0.0f;
  }
  IRAM_ATTR float process();
  bool isActive() const { return active; }

private:
  bool active;
  float srInv;
  HatsParams params;

  // Envelopes
  float env;
  float envMul;

  // Osc State (4-oscillator cluster for metal)
  float phase[4];
  float inc[4];

  // SVF Filter (resonant HPF for sparkle)
  float svfLow;
  float svfBand;
  float svfQ;

  // Helper
  inline float getMetal();
};

#endif
