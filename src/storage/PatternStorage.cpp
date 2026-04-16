#include "PatternStorage.h"

#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

static const char *const TRACK_KEYS[TRACK_COUNT] = {"track_0", "track_1",
                                                    "track_2", "track_3",
                                                    "track_4"};

PatternStorage PatternStore;

bool PatternStorage::begin() {
  SPI.begin(CYDConfig::SdSclk, CYDConfig::SdMiso, CYDConfig::SdMosi, CYDConfig::SdCs);
  if (!SD.begin(CYDConfig::SdCs, SPI, 4000000)) {
    Serial.println("SD Card mount failed!");
    ready = false;
    return false;
  }
  
  if (!SD.exists(CYDConfig::PatternsDir)) {
    SD.mkdir(CYDConfig::PatternsDir);
  }
  ready = true;
  return true;
}

String PatternStorage::slotPath(uint8_t slot) const {
  char path[32];
  snprintf(path, sizeof(path), "%s/pattern_%02u.json", CYDConfig::PatternsDir,
           static_cast<unsigned>(slot));
  return String(path);
}

bool PatternStorage::saveSlot(uint8_t slot) {
  if (!ready) return false;
  
  String path = slotPath(slot);
  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;
  
  bool success = captureJson(file);
  file.close();
  return success;
}

bool PatternStorage::loadSlot(uint8_t slot) {
  if (!ready) return false;
  
  String path = slotPath(slot);
  if (!SD.exists(path)) return false;
  
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  
  bool success = applyJson(file);
  file.close();
  return success;
}

bool PatternStorage::captureJson(File &file) {
  StaticJsonDocument<1024> doc;

  engine.lockPattern();
  for (int i = 0; i < TRACK_COUNT; ++i) {
    JsonObject trackObj = doc.createNestedObject(TRACK_KEYS[i]);
    trackObj["steps"] = engine.tracks[i].steps;
    trackObj["hits"] = engine.tracks[i].hits;
    trackObj["rotation"] = engine.tracks[i].rotationOffset;
  }
  engine.unlockPattern();

  doc["bpm"] = engine.bpm.load();

  if (serializeJson(doc, file) == 0) {
    return false;
  }
  return true;
}

bool PatternStorage::applyJson(File &file) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) return false;

  engine.lockPattern();
  for (int i = 0; i < TRACK_COUNT; ++i) {
    JsonObject trackObj = doc[TRACK_KEYS[i]];
    if (!trackObj.isNull()) {
      engine.tracks[i].steps = trackObj["steps"] | engine.tracks[i].steps;
      engine.tracks[i].hits = trackObj["hits"] | engine.tracks[i].hits;
      engine.tracks[i].rotationOffset = trackObj["rotation"] | engine.tracks[i].rotationOffset;
    }
  }
  engine.unlockPattern();

  if (doc.containsKey("bpm")) {
    engine.bpm.store(doc["bpm"].as<int>());
  }
  
  for (int i = 0; i < TRACK_COUNT; ++i) {
    engine.recalculatePattern(i);
  }
  return true;
}
