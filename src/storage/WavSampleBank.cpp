#include "WavSampleBank.h"

namespace {
struct WavFmtChunk {
  uint16_t audioFormat;
  uint16_t channels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
};
} // namespace

bool WavSampleBank::readChunkHeader(File &file, char id[5], uint32_t &size) {
  if (file.read(reinterpret_cast<uint8_t *>(id), 4) != 4) {
    return false;
  }
  id[4] = '\0';
  if (file.read(reinterpret_cast<uint8_t *>(&size), sizeof(size)) != sizeof(size)) {
    return false;
  }
  return true;
}

bool WavSampleBank::loadMono16(const char *path, WavSample &outSample,
                               size_t maxFrames) {
  File file = SD.open(path, FILE_READ);
  if (!file) {
    return false;
  }

  char riffId[5];
  uint32_t riffSize = 0;
  char waveId[5];
  if (!readChunkHeader(file, riffId, riffSize) ||
      file.read(reinterpret_cast<uint8_t *>(waveId), 4) != 4) {
    file.close();
    return false;
  }
  waveId[4] = '\0';
  if (strncmp(riffId, "RIFF", 4) != 0 || strncmp(waveId, "WAVE", 4) != 0) {
    file.close();
    return false;
  }

  WavFmtChunk fmt = {};
  uint32_t dataSize = 0;
  uint32_t dataOffset = 0;

  while (file.available()) {
    char chunkId[5];
    uint32_t chunkSize = 0;
    if (!readChunkHeader(file, chunkId, chunkSize)) {
      break;
    }

    if (strncmp(chunkId, "fmt ", 4) == 0) {
      if (chunkSize < sizeof(WavFmtChunk)) {
        file.close();
        return false;
      }
      file.read(reinterpret_cast<uint8_t *>(&fmt), sizeof(WavFmtChunk));
      if (chunkSize > sizeof(WavFmtChunk)) {
        file.seek(file.position() + (chunkSize - sizeof(WavFmtChunk)));
      }
    } else if (strncmp(chunkId, "data", 4) == 0) {
      dataOffset = file.position();
      dataSize = chunkSize;
      file.seek(file.position() + chunkSize);
    } else {
      file.seek(file.position() + chunkSize);
    }
  }

  if (fmt.audioFormat != 1 || fmt.bitsPerSample != 16 || dataSize == 0 ||
      fmt.blockAlign == 0) {
    file.close();
    return false;
  }

  const size_t totalFrames =
      min(maxFrames, static_cast<size_t>(dataSize / fmt.blockAlign));
  outSample.sampleRate = fmt.sampleRate;
  outSample.channels = fmt.channels;
  outSample.data.resize(totalFrames);

  file.seek(dataOffset);
  for (size_t i = 0; i < totalFrames; ++i) {
    int16_t left = 0;
    int16_t right = 0;
    file.read(reinterpret_cast<uint8_t *>(&left), sizeof(left));
    if (fmt.channels > 1) {
      file.read(reinterpret_cast<uint8_t *>(&right), sizeof(right));
      outSample.data[i] = static_cast<int16_t>((left + right) / 2);
    } else {
      outSample.data[i] = left;
    }
  }

  file.close();
  return true;
}
