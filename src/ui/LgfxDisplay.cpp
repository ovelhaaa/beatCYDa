#include "LgfxDisplay.h"

#include "../CYD_Config.h"
#include <Arduino.h>

namespace ui {

bool LgfxDisplay::begin() {
  pinMode(CYDConfig::TftBacklight, OUTPUT);
  digitalWrite(CYDConfig::TftBacklight, HIGH);

  _display.init();
  _display.setRotation(CYDConfig::ScreenRotation);
  return true;
}

void LgfxDisplay::setRotation(uint8_t rot) { _display.setRotation(rot); }

void LgfxDisplay::setBrightness(uint8_t level) { _display.setBrightness(level); }

int16_t LgfxDisplay::width() const { return _display.width(); }

int16_t LgfxDisplay::height() const { return _display.height(); }

uint16_t LgfxDisplay::color565(uint8_t r, uint8_t g, uint8_t b) const {
  return _display.color565(r, g, b);
}

bool LgfxDisplay::readTouch(TouchPoint &tp) {
  uint16_t tx = 0;
  uint16_t ty = 0;
  const bool pressed = _display.getTouch(&tx, &ty);

  tp.x = static_cast<int16_t>(tx);
  tp.y = static_cast<int16_t>(ty);
  tp.pressed = pressed;
  tp.justPressed = pressed && !_lastPressed;
  tp.justReleased = !pressed && _lastPressed;
  tp.dragging = pressed && _lastPressed &&
                (abs(tp.x - _lastX) > 1 || abs(tp.y - _lastY) > 1);

  _lastPressed = pressed;
  _lastX = tp.x;
  _lastY = tp.y;

  return pressed;
}

lgfx::LGFX_Device &LgfxDisplay::canvas() { return _display; }

LGFX_Sprite LgfxDisplay::createSprite() { return LGFX_Sprite(&_display); }

} // namespace ui
