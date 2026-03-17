#pragma once

#include "../audio/Engine.h"
#include "WavSampleBank.h"
#include <Arduino.h>
#include <FS.h>

class PatternStorage {
public:
  bool begin();

  bool saveSlot(uint8_t slot);
  bool loadSlot(uint8_t slot);
  String slotPath(uint8_t slot) const;
  bool isReady() const { return ready; }

  WavSampleBank &sampleBank() { return wavBank; }

private:
  bool captureJson(fs::File &file);
  bool applyJson(fs::File &file);

  bool ready = false;
  WavSampleBank wavBank;
};

extern PatternStorage PatternStore;
