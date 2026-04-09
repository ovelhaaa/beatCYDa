
#ifndef ENGINE_H
#define ENGINE_H

#include "../Config.h"
#include "../midi/MidiOutEngine.h"
#include "../ui/UiAction.h"
#include "BassGroove.h" // Bass Logic
// #include "SynthVoice.h" // Removed (Legacy)
#include "VoiceManager.h" // Use VoiceManager
#include <Arduino.h>
#include <Preferences.h> // ESP32 Preferences
#include <atomic>
#include <vector>

struct Track {
  int steps = 16;
  int hits = 4;
  uint8_t pattern[64];    // Fixed array - max 64 steps
  uint8_t patternLen = 0; // Actual pattern length
  int rotationOffset = 0; // Manual Euclidean Rotation

  // Voice Params
  float decay = 0.5f;
  float pitch = 0.5f;
  float color = 0.5f; // Timbre
  // Extended Params
  float drive = 0.0f;
  float snap = 0.0f;
  float harmonics = 0.0f;
};

struct PresetData {
  int t_hits[TRACK_COUNT];
  int t_steps[TRACK_COUNT];
  float synthPitch[TRACK_COUNT];
  float synthDecay[TRACK_COUNT];
  float synthTimbre[TRACK_COUNT];
  // Extended Arrays
  float synthDrive[TRACK_COUNT];
  float synthSnap[TRACK_COUNT];
  float synthHarmonics[TRACK_COUNT];

  int root;
  int scale;
  float bassDensity;
  int bassRange;
  int bassMode;
  int bassMotifIndex;
  float bassSwing;
  float bassAccentProb;
  float bassGhostProb;
  float bassPhraseVariation;
  int snareMode;
  int bpm; // Added BPM
};

class Engine {
public:
  // VOICES removed, handled by VoiceManager
  Track tracks[TRACK_COUNT];
  PresetData slots[16];

  // UI Sync
  std::atomic<uint32_t> trackVersions[TRACK_COUNT];

  std::atomic<int> bpm{80}; // Default 80
  std::atomic<bool> isPlaying{true};
  std::atomic<bool> trackMutes[TRACK_COUNT]; // New: Mutes for JAM mode
  std::atomic<int> globalRoot{0};
  std::atomic<int> globalScale{0};
  std::atomic<int> currentStep{0};
  std::atomic<UiMode> uiMode{UiMode::PATTERN_EDIT};
  UiMode lastMode = UiMode::PATTERN_EDIT;

  std::atomic<int> uiActiveTrack{0};
  std::atomic<int> uiActiveParamIndex{0}; // 0..N per mode
  std::atomic<bool> isParamEditMode{false};
  std::atomic<int> uiActivePatternAttribute{
      0}; // 0=Steps/Dens, 1=Hits/Rng, 2=Rot/Scl, 3=Root

  std::atomic<bool> engineReady{false};

  // Global Context
  std::atomic<float> masterVolume{1.0f};
  std::atomic<bool> isShiftHeld{false};
  std::atomic<bool> autoRotateDownbeat{
      false}; // Controls "downbeat guarantee" rotation

  SemaphoreHandle_t patternMutex = NULL;
  Preferences preferences;
  hw_timer_t *mainTimer = NULL;

  Engine();
  void init();

  void recalculatePattern(int id);
  void randomize();

  void saveSlot(int slotId, bool capture = true);
  void loadSlot(int slotId);
  void loadAllSlots();
  void applySlotToActive(int slotId);

  // ==========================================
  // UI ACTION HANDLER (THE BRAIN)
  // ==========================================
  // ==========================================
  // UI ACTION HANDLER (THE BRAIN)
  // ==========================================
  void handleUiAction(struct UiAction action);

  void play();
  void stop();
  void syncBassGrooveFromGlobals();
  void syncGlobalsFromBassGroove();
  void syncBassGrooveFromVoicePitch(float normalizedPitch);

  bool lockPattern();
  void unlockPattern();

  void updateTimerFreq(hw_timer_t *timer);

private:
  void captureActiveToSlot(int slotId); // Helper for saving
  void initFactoryPresets();            // Factory Presets
  void setFactoryPreset(int slotId);    // Helper for hardcoded loading
  void recalculatePatternUnlocked(int id);
  float cachedNoteFreqs[12][96];
  void initFrequencyCache();
  uint8_t bassRootClassToMidi(int rootClass) const;
  int bassMidiToRootClass(uint8_t rootNote) const;
  float bassRootNoteToNormalizedPitch(uint8_t rootNote) const;
  uint8_t normalizedPitchToBassRootNote(float normalizedPitch) const;
  BassScale bassScaleFromGlobal(int scale) const;
  int globalScaleFromBass(BassScale scale) const;

public:
  BassGroove bassGroove; // Generative Bass Logic
  MidiOutEngine midiOut;
};

extern Engine engine;

#endif