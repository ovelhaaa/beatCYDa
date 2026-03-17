#include "DSP.h"

float sinLUT[LUT_SIZE];

float fastRandom() {
  static unsigned long seed = 123456789;
  seed = (1664525 * seed + 1013904223);
  return (float)(seed & 0xFFFF) * (1.0f / 32768.0f) - 1.0f;
}

float fastSoftClip(float x) {
  if (x < -1.5f)
    return -1.0f;
  if (x > 1.5f)
    return 1.0f;
  return x - (0.1481f * x * x * x);
}

void initLUT() {
  for (int i = 0; i < LUT_SIZE; i++)
    sinLUT[i] = sinf((float)i * 2.0f * PI / (float)LUT_SIZE);
}

float getSinLUT(float phase) {
  float idxF = phase * LUT_SIZE;
  int idx = (int)idxF;
  int i0 = idx & LUT_MASK;
  int i1 = (idx + 1) & LUT_MASK;
  float frac = idxF - idx;
  return sinLUT[i0] + frac * (sinLUT[i1] - sinLUT[i0]);
}

float onePoleLPF(float in, float &state, float a) {
  state += (in - state) * a;
  return state;
}

float onePoleHPF(float in, float &state, float a) {
  float output = in - state;
  state += output * a;
  return output;
}
