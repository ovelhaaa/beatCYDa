#pragma once

#include "../../audio/BassGroove.h"
#include <stdint.h>

namespace ui {
namespace bassfmt {

inline const char *modeShortName(GrooveMode mode) {
  switch (mode) {
  case GrooveMode::OFFBEAT:
    return "OFF";
  case GrooveMode::RANDOM:
    return "RND";
  case GrooveMode::MOTIF:
    return "MTF";
  case GrooveMode::FOLLOW_KICK:
  default:
    return "FK";
  }
}

inline const char *scaleShortName(BassScale scale) {
  switch (scale) {
  case BassScale::MAJOR:
    return "MAJ";
  case BassScale::DORIAN:
    return "DOR";
  case BassScale::PHRYGIAN:
    return "PHR";
  case BassScale::MINOR:
  default:
    return "MIN";
  }
}

inline const char *noteName(uint8_t midiNote) {
  static const char *kNames[12] = {"C",  "C#", "D",  "D#", "E", "F",
                                   "F#", "G",  "G#", "A",  "A#", "B"};
  return kNames[midiNote % 12];
}

inline int noteOctave(uint8_t midiNote) { return (midiNote / 12) - 1; }

} // namespace bassfmt
} // namespace ui
