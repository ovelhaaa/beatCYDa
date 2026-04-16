#pragma once

#include "UiRect.h"
#include "../Display.h"
#include <LovyanGFX.hpp>
#include <stdint.h>

namespace ui {

class UiEuclideanRings {
public:
  void setRect(const UiRect &rect);
  void setCompact(bool compact);
  void setInteractive(bool interactive);
  void setSingleTrack(bool singleTrack);

  void draw(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot);

  bool hitTestTrack(int16_t x, int16_t y, uint8_t &outTrack) const;
  bool hitTestStep(int16_t x, int16_t y, const UiStateSnapshot &snapshot, uint8_t &outTrack, uint8_t &outStep) const;

  void invalidateAll();
  void invalidateTrack(uint8_t track);

private:
  void ensureSprite();
  void updateBassEnvelope(const UiStateSnapshot &snapshot);
  void triggerBassEnvelope();
  void redraw(const UiStateSnapshot &snapshot);
  float ringRadiusForTrack(uint8_t track) const;

  UiRect _rect{};
  bool _compact{false};
  bool _interactive{false};
  bool _singleTrack{false};
  bool _dirty{true};
  int16_t _lastW{0};
  int16_t _lastH{0};
  int _lastBassStep{-1};
  int _lastRenderedStep{-1};
  bool _lastIsPlaying{false};
  float _bassEnvValue{0.0f};
  uint32_t _lastEnvMicros{0};
  mutable uint8_t _lastHitTrack{0xFF};
  mutable float _lastHitDist{0.0f};
  mutable uint32_t _lastHitMicros{0};

  mutable uint16_t _trackOuterRadius[TRACK_COUNT]{};
  mutable uint16_t _trackInnerRadius[TRACK_COUNT]{};

  lgfx::LGFX_Sprite _sprite{};
};

} // namespace ui
