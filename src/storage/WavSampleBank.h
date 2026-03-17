#pragma once

#include <Arduino.h>
#include <SD.h>
#include <vector>

struct WavSample {
  uint32_t sampleRate = 0;
  uint16_t channels = 0;
  std::vector<int16_t> data;
};

class WavSampleBank {
public:
  bool loadMono16(const char *path, WavSample &outSample, size_t maxFrames);

private:
  bool readChunkHeader(File &file, char id[5], uint32_t &size);
};
