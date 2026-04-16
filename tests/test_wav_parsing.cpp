#include <iostream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#include "Arduino.h"
#include "SD.h"

SDMock SD;

// Since we include WavSampleBank.cpp, we shouldn't define WavFmtChunk here
// But we need to use it. We can't because it's in an anonymous namespace in WavSampleBank.cpp.
// Let's create a matching struct locally to build the binary data.
struct LocalWavFmtChunk {
  uint16_t audioFormat;
  uint16_t channels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
};

#include "../src/storage/WavSampleBank.h"
#include "../src/storage/WavSampleBank.cpp"

void test_div_by_zero() {
    std::cout << "Running test_div_by_zero (blockAlign = 0)..." << std::endl;

    WavSampleBank bank;
    WavSample sample;

    // Create a malicious WAV header with blockAlign = 0
    std::vector<uint8_t> maliciousWav;

    // RIFF header
    maliciousWav.insert(maliciousWav.end(), {'R', 'I', 'F', 'F'});
    uint32_t riffSize = 36 + 8;
    uint8_t* p = reinterpret_cast<uint8_t*>(&riffSize);
    maliciousWav.insert(maliciousWav.end(), p, p + 4);
    maliciousWav.insert(maliciousWav.end(), {'W', 'A', 'V', 'E'});

    // fmt chunk
    maliciousWav.insert(maliciousWav.end(), {'f', 'm', 't', ' '});
    uint32_t fmtSize = 16;
    p = reinterpret_cast<uint8_t*>(&fmtSize);
    maliciousWav.insert(maliciousWav.end(), p, p + 4);

    LocalWavFmtChunk fmt;
    fmt.audioFormat = 1;      // PCM
    fmt.channels = 1;
    fmt.sampleRate = 44100;
    fmt.byteRate = 88200;
    fmt.blockAlign = 0;       // VULNERABILITY: blockAlign is 0
    fmt.bitsPerSample = 16;
    p = reinterpret_cast<uint8_t*>(&fmt);
    maliciousWav.insert(maliciousWav.end(), p, p + sizeof(LocalWavFmtChunk));

    // data chunk
    maliciousWav.insert(maliciousWav.end(), {'d', 'a', 't', 'a'});
    uint32_t dataSize = 100;
    p = reinterpret_cast<uint8_t*>(&dataSize);
    maliciousWav.insert(maliciousWav.end(), p, p + 4);
    maliciousWav.resize(maliciousWav.size() + dataSize, 0);

    SD.currentFile = maliciousWav;

    // If blockAlign = 0, it should return false.
    bool result = bank.loadMono16("malicious.wav", sample, 1000);

    if (result) {
        std::cout << "test_div_by_zero FAILED: loadMono16 returned true for blockAlign=0" << std::endl;
        exit(1);
    } else {
        std::cout << "test_div_by_zero PASSED (returned false)" << std::endl;
    }
}

int main() {
    test_div_by_zero();
    return 0;
}
