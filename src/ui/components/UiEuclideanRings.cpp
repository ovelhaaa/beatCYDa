#include "UiEuclideanRings.h"

#include "../core/UiLayout.h"
#include "../theme/UiTheme.h"

#include <math.h>

namespace ui {
namespace {
constexpr float kPi = 3.14159265f;

float stepAngleRad(int step, int patternLen) {
  const float turns = static_cast<float>(step) / static_cast<float>(patternLen);
  const float deg = -90.0f + (turns * 360.0f);
  return deg * (kPi / 180.0f);
}

uint16_t dimColor(uint16_t rgb565, float factor) {
  const uint8_t r = static_cast<uint8_t>((((rgb565 >> 11) & 0x1F) * factor));
  const uint8_t g = static_cast<uint8_t>((((rgb565 >> 5) & 0x3F) * factor));
  const uint8_t b = static_cast<uint8_t>(((rgb565 & 0x1F) * factor));
  return static_cast<uint16_t>(((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
}

int clampLen(int value) {
  if (value < 1) {
    return 1;
  }
  if (value > 64) {
    return 64;
  }
  return value;
}

void drawBassClef(lgfx::LGFX_Sprite &sprite, int cx, int cy, uint16_t color, uint16_t bg) {
  sprite.drawCircle(cx - 3, cy - 1, 5, color);
  sprite.fillCircle(cx - 3, cy - 1, 2, color);
  sprite.drawCircle(cx - 3, cy - 1, 1, bg);
  sprite.drawFastVLine(cx + 2, cy - 11, 20, color);
  sprite.fillCircle(cx + 4, cy - 4, 1, color);
  sprite.fillCircle(cx + 4, cy + 3, 1, color);
}
} // namespace

void UiEuclideanRings::setRect(const UiRect &rect) {
  if (_rect.x == rect.x && _rect.y == rect.y && _rect.w == rect.w && _rect.h == rect.h) {
    return;
  }
  _rect = rect;
  _dirty = true;
}

void UiEuclideanRings::setCompact(bool compact) {
  if (_compact == compact) {
    return;
  }
  _compact = compact;
  _dirty = true;
}

void UiEuclideanRings::setInteractive(bool interactive) {
  _interactive = interactive;
}

void UiEuclideanRings::setSingleTrack(bool singleTrack) {
  if (_singleTrack == singleTrack) {
    return;
  }
  _singleTrack = singleTrack;
  _dirty = true;
}

void UiEuclideanRings::ensureSprite() {
  if (_rect.w <= 0 || _rect.h <= 0) {
    return;
  }

  if (_lastW == _rect.w && _lastH == _rect.h && _sprite.width() == _rect.w && _sprite.height() == _rect.h) {
    return;
  }

  if (_sprite.width() > 0 || _sprite.height() > 0) {
    _sprite.deleteSprite();
  }

  _sprite.setColorDepth(16);
  _sprite.createSprite(_rect.w, _rect.h);
  _lastW = _rect.w;
  _lastH = _rect.h;
  _dirty = true;
}

float UiEuclideanRings::ringRadiusForTrack(uint8_t track) const {
  const float minDim = static_cast<float>((_rect.w < _rect.h) ? _rect.w : _rect.h);
  const float maxR = (minDim * 0.5f) - (_compact ? 8.0f : 10.0f);

  if (_singleTrack) {
    return maxR;
  }

  const float minR = _compact ? 18.0f : 16.0f;
  const float span = maxR - minR;
  const float t = static_cast<float>(TRACK_COUNT - 1 - track) / static_cast<float>(TRACK_COUNT - 1);
  return minR + (span * t);
}

void UiEuclideanRings::redraw(const UiStateSnapshot &snapshot) {
  if (_sprite.width() <= 0 || _sprite.height() <= 0) {
    return;
  }

  const int cx = _rect.w / 2;
  const int cy = _rect.h / 2;
  _sprite.fillSprite(theme::UiTheme::Colors::Bg);

  for (int i = 0; i < TRACK_COUNT; ++i) {
    const float radius = ringRadiusForTrack(static_cast<uint8_t>(i));
    _trackOuterRadius[i] = static_cast<uint16_t>(radius + 4.5f);
    _trackInnerRadius[i] = static_cast<uint16_t>((radius > 5.0f) ? (radius - 4.5f) : 0.0f);
  }

  const int startTrack = _singleTrack ? snapshot.activeTrack : 0;
  const int endTrack = _singleTrack ? (snapshot.activeTrack + 1) : TRACK_COUNT;

  for (int track = startTrack; track < endTrack; ++track) {
    const bool active = (track == snapshot.activeTrack);
    const bool muted = snapshot.trackMutes[track];

    uint16_t baseColor = TRACK_COLORS[track % TRACK_COUNT];
    float dim = active ? 1.00f : 0.70f;
    if (muted) {
      dim *= 0.45f;
    }
    const uint16_t ringColor = dimColor(baseColor, dim * 0.65f);
    const uint16_t stepColor = dimColor(baseColor, dim * 0.45f);
    const uint16_t hitColor = dimColor(baseColor, dim * (active ? 1.0f : 0.75f));
    const uint16_t lineColor = dimColor(baseColor, dim * (active ? 0.95f : 0.6f));

    const int len = clampLen(snapshot.patternLens[track]);
    const float radius = ringRadiusForTrack(static_cast<uint8_t>(track));

    _sprite.drawCircle(cx, cy, static_cast<int>(radius), ringColor);

    int hitCount = 0;
    int hitX[64]{};
    int hitY[64]{};

    for (int step = 0; step < len; ++step) {
      const float a = stepAngleRad(step, len);
      const int x = cx + static_cast<int>(cosf(a) * radius);
      const int y = cy + static_cast<int>(sinf(a) * radius);
      _sprite.fillCircle(x, y, _compact ? 1 : 2, stepColor);

      if (snapshot.patterns[track][step]) {
        hitX[hitCount] = x;
        hitY[hitCount] = y;
        hitCount++;
        const int hitSize = active ? (_compact ? 2 : 3) : (_compact ? 1 : 2);
        _sprite.fillCircle(x, y, hitSize, hitColor);
      }
    }

    if (hitCount == 2) {
      _sprite.drawLine(hitX[0], hitY[0], hitX[1], hitY[1], lineColor);
    } else if (hitCount >= 3) {
      for (int i = 0; i < hitCount; ++i) {
        const int n = (i + 1) % hitCount;
        _sprite.drawLine(hitX[i], hitY[i], hitX[n], hitY[n], lineColor);
      }
    }

    if (active && !_compact && hitCount >= 3) {
      for (int i = 0; i < hitCount; ++i) {
        const int n = (i + 1) % hitCount;
        _sprite.drawLine(hitX[i], hitY[i], hitX[n], hitY[n], dimColor(hitColor, 0.6f));
      }
    }
  }

  const int activeLen = clampLen(snapshot.patternLens[snapshot.activeTrack]);
  const int playStep = snapshot.currentStep % activeLen;
  const float playAngle = stepAngleRad(playStep, activeLen);
  const float playRadius = _singleTrack ? ringRadiusForTrack(snapshot.activeTrack) : ringRadiusForTrack(0);
  const int playX = cx + static_cast<int>(cosf(playAngle) * (playRadius + (_compact ? 4.0f : 6.0f)));
  const int playY = cy + static_cast<int>(sinf(playAngle) * (playRadius + (_compact ? 4.0f : 6.0f)));
  _sprite.drawLine(cx, cy, playX, playY, theme::UiTheme::Colors::TextSecondary);

  if (_compact) {
    _sprite.setTextSize(theme::UiTheme::Typography::CaptionSize);
    _sprite.setTextDatum(textdatum_t::middle_center);
    _sprite.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
    if (snapshot.activeTrack == VOICE_BASS) {
      drawBassClef(_sprite, cx, cy - 5, theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
    } else {
      _sprite.drawString(TRACK_LABELS[snapshot.activeTrack], cx, cy - 6);
    }

    char density[16];
    snprintf(density, sizeof(density), "%d/%d", snapshot.trackHits[snapshot.activeTrack], snapshot.trackSteps[snapshot.activeTrack]);
    _sprite.drawString(density, cx, cy + 8);
    _sprite.setTextDatum(textdatum_t::top_left);
  }
}

void UiEuclideanRings::draw(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) {
  ensureSprite();
  if (_dirty) {
    redraw(snapshot);
    _dirty = false;
  }

  if (_sprite.width() > 0 && _sprite.height() > 0) {
    _sprite.pushSprite(&canvas, _rect.x, _rect.y);
  }
}

bool UiEuclideanRings::hitTestTrack(int16_t x, int16_t y, uint8_t &outTrack) const {
  if (!_interactive || !_rect.contains(x, y)) {
    return false;
  }

  const int16_t lx = x - _rect.x;
  const int16_t ly = y - _rect.y;
  const int16_t cx = _rect.w / 2;
  const int16_t cy = _rect.h / 2;
  const int32_t dx = lx - cx;
  const int32_t dy = ly - cy;
  const float dist = sqrtf(static_cast<float>(dx * dx + dy * dy));

  for (int i = 0; i < TRACK_COUNT; ++i) {
    if (dist >= _trackInnerRadius[i] && dist <= _trackOuterRadius[i]) {
      outTrack = static_cast<uint8_t>(i);
      return true;
    }
  }

  return false;
}

bool UiEuclideanRings::hitTestStep(int16_t x, int16_t y, const UiStateSnapshot &snapshot, uint8_t &outTrack, uint8_t &outStep) const {
  if (!_interactive || !_rect.contains(x, y)) {
    return false;
  }

  uint8_t track = 0;
  if (!hitTestTrack(x, y, track)) {
    return false;
  }

  const int16_t lx = x - _rect.x;
  const int16_t ly = y - _rect.y;
  const int16_t cx = _rect.w / 2;
  const int16_t cy = _rect.h / 2;
  const float angle = atan2f(static_cast<float>(ly - cy), static_cast<float>(lx - cx));
  float deg = (angle * 180.0f / kPi) + 90.0f;
  if (deg < 0.0f) {
    deg += 360.0f;
  }

  const int len = clampLen(snapshot.patternLens[track]);
  const int step = static_cast<int>((deg / 360.0f) * static_cast<float>(len)) % len;

  outTrack = track;
  outStep = static_cast<uint8_t>(step);
  return true;
}

void UiEuclideanRings::invalidateAll() {
  _dirty = true;
}

void UiEuclideanRings::invalidateTrack(uint8_t track) {
  (void)track;
  _dirty = true;
}

} // namespace ui
