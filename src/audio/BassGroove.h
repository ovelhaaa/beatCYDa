#ifndef BASS_GROOVE_H
#define BASS_GROOVE_H

#include "VoiceManager.h" // For VoiceID (implied usage via global voiceManager)
#include <Arduino.h>
#include <stdint.h>

/**
 * BassGroove
 *
 * Generative logic for the Bass Voice.
 * Handles Harmony (Scales), Rhythm (Mode specific), and Movement (Walker).
 * Output: Triggers VoiceManager directly.
 */

enum class BassScale : uint8_t { MINOR = 0, MAJOR, DORIAN, PHRYGIAN };

enum class GrooveMode : uint8_t { FOLLOW_KICK = 0, OFFBEAT, RANDOM, MOTIF };

struct BassGrooveParams {
  // Harmony
  uint8_t rootNote; // MIDI Note (24-48 typical)
  BassScale scaleType;
  int8_t octaveOffset; // -1, 0, +1

  // Rhythm
  GrooveMode mode;
  float density;       // 0.0 .. 1.0 (Probability of triggering)
  float minIntervalMs; // Minimum time between notes

  // Movement (Walker)
  int range;       // Max interval jump (semitones approx, or scale degrees)
  float slideProb; // 0.0 .. 1.0
  float phraseVariation; // 0.0 .. 1.0 (A/B phrase contrast + cadential pull)

  // Motif
  uint8_t motifIndex; // Fixed motif index per scale/mode

  // Feel / dynamics
  float swing;      // 0.0 .. 1.0 (delay applied to even steps)
  float ghostProb;  // 0.0 .. 1.0
  float accentProb; // 0.0 .. 1.0
};

class BassGroove {
public:
  void init(float sampleRate);

  // Update logic parameters
  void updateParams(const BassGrooveParams &newParams);

  // Getter for UI
  BassGrooveParams getParams() const { return params; }

  // Call this on every sequencer tick (e.g. 1/16th)
  void onTick(int currentStep);

  // Call this when the Kick triggers (for sync)
  void onKick();

  // Call this periodically or per sample block to track time
  void process(float dt_ms);

private:
  float sampleRate;
  BassGrooveParams params;

  // State
  uint8_t lastNote; // Last MIDI note triggered
  float timeSinceLastTriggerMs;

  // 303 State
  int degree;    // 0..SCALE_SIZE-1
  int octave;    // Relative octave
  bool altState; // Alternation state A/B
  int phraseStep;    // 0..15
  int phraseVariant; // 0=A, 1=B

  // Motif state
  bool hasPendingMotifDegree;
  int pendingMotifDegree;
  bool hasPendingTrigger;
  bool pendingAccent;
  float pendingTriggerDelayMs;

  // Kick Sync State
  bool kickReceived;

  // Cached scale pointer for performance
  const int *cachedScale;
  int cachedScaleLen;

  // Thread-local RNG state (ESP32 random)
  uint32_t rngState;

  // Internal Logic
  void trigger();
  void trigger(bool forceAccent); // Added overload
  uint8_t generateNote();
  float midiToFreq(uint8_t note);

  // Random helper
  bool checkProb(float p);
  float randomUnit();

  // Scale Definition
  static constexpr int SCALE_SIZE = 5;
};

#endif // BASS_GROOVE_H
