#ifndef DSP_H
#define DSP_H

#include <Arduino.h>
#include <math.h>

// Functions
float fastRandom();
float fastSoftClip(float x);

// Velocity curves: map linear 0-1 to curved 0-1
// curve > 1.0 = exponential (harder), curve < 1.0 = logarithmic (softer)
inline float velocityCurve(float vel, float curve = 2.0f) {
  return powf(vel, curve);
}

// Global LUT
#define LUT_SIZE 2048
#define LUT_MASK 2047
extern float sinLUT[LUT_SIZE];
void initLUT();
float getSinLUT(float phase);

// Filters
float onePoleLPF(float in, float &state, float a);
float onePoleHPF(float in, float &state, float a);

#endif
