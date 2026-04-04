#pragma once

#include "../../audio/Engine.h"

struct Rect {
  int16_t x, y, w, h;
  bool contains(int16_t px, int16_t py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};

constexpr Rect R_PLAY{4, 8, 62, 30};
constexpr Rect R_SLOT_DEC{80, 8, 20, 30};
constexpr Rect R_SLOT_VAL{104, 8, 28, 30};
constexpr Rect R_SLOT_INC{136, 8, 20, 30};
constexpr Rect R_SAVE{160, 8, 24, 14};
constexpr Rect R_LOAD{160, 24, 24, 14};
constexpr Rect R_BPM_DEC{196, 8, 28, 30};
constexpr Rect R_BPM_VAL{228, 8, 52, 30};
constexpr Rect R_BPM_INC{284, 8, 28, 30};

constexpr Rect R_TRACKS[TRACK_COUNT] = {{4, 48, 28, 34}, {4, 86, 28, 34},
                                         {4, 124, 28, 34}, {4, 162, 28, 34},
                                         {4, 200, 28, 34}};

constexpr Rect R_RING{36, 52, 140, 140};
constexpr int RING_CX = 70;
constexpr int RING_CY = 70;
constexpr uint8_t RING_RADII[TRACK_COUNT] = {58, 48, 38, 28, 18};

constexpr Rect R_MUTE{36, 208, 52, 26};
constexpr Rect R_VOICE{88, 208, 52, 26};
constexpr Rect R_MIX{140, 208, 52, 26};

constexpr int PANEL_SX = 188;
constexpr int PANEL_SY = 44;
constexpr int PANEL_W = 132;
constexpr int PANEL_H = 168;

struct ParamRow {
  Rect minus;
  Rect value;
  Rect plus;
  Rect touch;
};

constexpr ParamRow PARAM_ROWS[4] = {
    {{8, 18, 28, 26}, {40, 18, 52, 26}, {96, 18, 28, 26}, {0, 4, PANEL_W, 42}},
    {{8, 68, 28, 26}, {40, 68, 52, 26}, {96, 68, 28, 26}, {0, 54, PANEL_W, 42}},
    {{8, 118, 28, 26}, {40, 118, 52, 26}, {96, 118, 28, 26}, {0, 104, PANEL_W, 42}},
    {{8, 142, 28, 26}, {40, 142, 52, 26}, {96, 142, 28, 26}, {0, 130, PANEL_W, 38}},
};

constexpr Rect R_SLIDERS[TRACK_COUNT] = {{201, 54, 12, 126}, {224, 54, 12, 126},
                                          {247, 54, 12, 126}, {270, 54, 12, 126},
                                          {293, 54, 12, 126}};

constexpr uint16_t TRACK_COLORS[TRACK_COUNT] = {
    0xCAA9u, 0xD568u, 0x4F1Fu, 0xFA74u, 0x5D6Cu,
};

constexpr char TRACK_CHARS[TRACK_COUNT] = {'K', 'S', 'C', 'O', 'B'};
constexpr const char *TRACK_LABELS[TRACK_COUNT] = {"KICK", "SNRE", "HATC", "HATO", "BASS"};
