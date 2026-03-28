#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <stdint.h>
#include <math.h>

#define PI 3.14159265358979323846

template<typename T>
T constrain(T amt, T low, T high) {
    if (amt < low) return low;
    if (amt > high) return high;
    return amt;
}

typedef int i2s_port_t;
#define I2S_NUM_0 0

typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0

typedef void* SemaphoreHandle_t;
typedef void* hw_timer_t;

#endif // MOCK_ARDUINO_H
