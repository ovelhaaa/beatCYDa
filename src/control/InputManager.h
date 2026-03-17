#pragma once

#include "../CYD_Config.h"
#include <Arduino.h>
#include "../ui/LGFX_CYD.h"

struct TouchPoint {
  bool pressed = false;
  bool justPressed = false;
  bool justReleased = false;
  bool dragging = false;
  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t startX = 0;
  uint16_t startY = 0;
  uint16_t lastX = 0;
  uint16_t lastY = 0;
  uint32_t pressedAtMs = 0;
};

class InputManager {
public:
  InputManager();
  ~InputManager();

  bool begin();
  void update();
  const TouchPoint &state() const { return touchState; }

private:
  TouchPoint touchState;
  bool initialized;
  uint32_t lastPollMs;
};

extern InputManager InputMgr;
