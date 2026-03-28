#define private public
#define protected public
#include "BassGroove.h"
#undef private
#undef protected

#include <iostream>
#include <cassert>
#include <cmath>

uint32_t esp_random() { return 12345; }

void test_midiToFreq() {
    BassGroove bg;

    // Test A4 (69)
    float freqA4 = bg.midiToFreq(69);
    assert(std::abs(freqA4 - 440.0f) < 0.1f);

    // Test C4 (60)
    float freqC4 = bg.midiToFreq(60);
    assert(std::abs(freqC4 - 261.63f) < 0.1f);

    // Test lowest note (0)
    float freq0 = bg.midiToFreq(0);
    assert(std::abs(freq0 - 8.175f) < 0.1f);

    // Test highest note (127)
    float freq127 = bg.midiToFreq(127);
    assert(std::abs(freq127 - 12543.85f) < 0.1f);

    std::cout << "test_midiToFreq passed!" << std::endl;
}

int main() {
    test_midiToFreq();
    std::cout << "All BassGroove tests passed!" << std::endl;
    return 0;
}
