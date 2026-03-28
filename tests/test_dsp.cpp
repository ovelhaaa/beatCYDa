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

void test_fastRandom() {
    std::cout << "Running test_fastRandom..." << std::endl;

    bool has_positive = false;
    bool has_negative = false;
    float previous_val = 0.0f;
    bool has_diff = false;

    // Test a reasonable number of iterations
    for (int i = 0; i < 1000; i++) {
        float val = fastRandom();

        // Check bounds [-1.0f, 1.0f]
        if (val < -1.0f || val > 1.0f) {
            std::cerr << "Test failed at index " << i << ": fastRandom() returned " << val << " which is out of bounds [-1.0f, 1.0f]" << std::endl;
            exit(1);
        }

        if (val > 0.0f) has_positive = true;
        if (val < 0.0f) has_negative = true;

        if (i > 0 && std::abs(val - previous_val) > 1e-6) {
            has_diff = true;
        }

        previous_val = val;
    }

    // Verify properties
    if (!has_positive || !has_negative) {
        std::cerr << "Test failed: fastRandom() did not produce both positive and negative values" << std::endl;
        exit(1);
    }

    if (!has_diff) {
        std::cerr << "Test failed: fastRandom() returned identical consecutive values" << std::endl;
        exit(1);
    }

    std::cout << "test_fastRandom passed!" << std::endl;
}

int main() {
    test_initLUT();
    test_fastRandom();
    return 0;
}
