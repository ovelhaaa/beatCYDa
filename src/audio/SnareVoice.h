#ifndef SNARE_VOICE_H
#define SNARE_VOICE_H

#include "DSP.h"
#include "esp_attr.h"
#include <Arduino.h>
#include <stdint.h>

struct SnareParams {
  float tone;   // 0..1 (Tuning + Body)
  float decay;  // 0..1 (Length: Wire > Body)
  float timbre; // 0..1 (Wire Brightness + Click Amount)
  uint8_t mode; // 0=Snare, 1=Rim, 2=Clap
};

class SnareVoice {
public:
  void init(float sampleRate);
  void setParams(const SnareParams &p);

  void trigger(float velocity = 1.0f);

  IRAM_ATTR float process();

  bool isActive() const { return active; }

private:
  // Core
  bool active;
  float srInv;
  SnareParams params;
  uint32_t rngState;

  // Envelopes (Recursive multipliers)
  float envBody;
  float envMulBody;

  float envNoise;
  float envMulNoise;

  float envClick;
  float envMulClick;

  float envPitch;
  float envMulPitch;

  // Rim/Clap State
  float envRim;
  float envMulRim;

  // Clap Burst State (4 micro-hits)
  int clapBurstCount;     // 0..4 bursts remaining
  int clapBurstSamples;   // Samples until next burst
  float clapBurstEnvs[4]; // Individual burst envelopes

  // State
  float phase;
  float phaseInc;
  float phase2; // Osc 2
  float phaseInc2;
  float baseFreq;

  // DSP Coefficients (Pre-calc)
  float clickGain;
  float wireGain;
  float filterC1;
  float filterC2;

  // DSP State
  float wire1;
  float wire2;
  float dcX1;
  float dcY1;

  // Renderers
  float renderSnare();
  float renderRim();
  float renderClap();

  // Internal
  inline uint32_t xorShift() {
    rngState ^= (rngState << 13);
    rngState ^= (rngState >> 17);
    rngState ^= (rngState << 5);
    return rngState;
  }

  // Fast Saturator (Cubic)
  inline float fastSat(float x) {
    if (x < -1.5f)
      return -1.0f;
    if (x > 1.5f)
      return 1.0f;
    return x - (0.296f * x * x * x); // Tuned for speed/character
  }
};

#endif
