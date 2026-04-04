#pragma once

#include "LGFX_CYD.h"

namespace ui {

struct TouchPoint {
  int16_t x{0};
  int16_t y{0};
  bool pressed{false};
  bool justPressed{false};
  bool justReleased{false};
  bool dragging{false};
};

class LgfxDisplay {
public:
  bool begin();
  void setRotation(uint8_t rot);
  void setBrightness(uint8_t level);

  int16_t width() const;
  int16_t height() const;

  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) const;
  bool readTouch(TouchPoint &tp);

  lgfx::LGFX_Device &canvas();
  LGFX_Sprite createSprite();

private:
  bool _lastPressed{false};
  int16_t _lastX{0};
  int16_t _lastY{0};
  LGFX_CYD _display;
};

} // namespace ui
