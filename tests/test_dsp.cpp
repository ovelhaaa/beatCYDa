#include <iostream>
#include <cmath>
#include <cassert>
#include <vector>
#include <cstdlib>

// Mock Arduino environment
#include "Arduino.h"

// The implementation in DSP.cpp defines sinLUT, so we don't define it here.
// But we need the declarations from DSP.h
#include "../src/audio/DSP.h"

// Include the implementation directly to avoid complex linking for this simple test
#include "../src/audio/DSP.cpp"

void test_initLUT() {
    std::cout << "Running test_initLUT..." << std::endl;

    initLUT();

    for (int i = 0; i < LUT_SIZE; i++) {
        float expected = sinf((float)i * 2.0f * PI / (float)LUT_SIZE);
        if (std::abs(sinLUT[i] - expected) > 1e-6) {
            std::cerr << "Test failed at index " << i << ": expected " << expected << ", got " << sinLUT[i] << std::endl;
            exit(1);
        }
    }

    std::cout << "test_initLUT passed!" << std::endl;
}

int main() {
    test_initLUT();
    return 0;
}
