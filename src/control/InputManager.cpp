#include "InputManager.h"

InputManager InputMgr;

InputManager::InputManager()
    : initialized(false), lastPollMs(0) {}

InputManager::~InputManager() {
}

bool InputManager::begin() {
  initialized = true;
  touchState = TouchPoint{};
  return true;
}


void InputManager::update() {
  if (!initialized) {
    return;
  }

  const uint32_t now = millis();
  if ((now - lastPollMs) < CYDConfig::TouchPollMs) {
    touchState.justPressed = false;
    touchState.justReleased = false;
    return;
  }
  lastPollMs = now;

  touchState.justPressed = false;
  touchState.justReleased = false;

  uint16_t mappedX = 0;
  uint16_t mappedY = 0;
  const bool isTouched = tft.getTouch(&mappedX, &mappedY);
  if (!isTouched) {
    if (touchState.pressed) {
      touchState.justReleased = true;
    }
    touchState.pressed = false;
    touchState.dragging = false;
    return;
  }

  if (!touchState.pressed) {
    touchState.justPressed = true;
    touchState.pressedAtMs = now;
    touchState.startX = mappedX;
    touchState.startY = mappedY;
    touchState.lastX = mappedX;
    touchState.lastY = mappedY;
  } else {
    const int dx = static_cast<int>(mappedX) - static_cast<int>(touchState.startX);
    const int dy = static_cast<int>(mappedY) - static_cast<int>(touchState.startY);
    if ((abs(dx) + abs(dy)) >= CYDConfig::DragThreshold) {
      touchState.dragging = true;
    }
  }

  touchState.pressed = true;
  touchState.x = mappedX;
  touchState.y = mappedY;
  touchState.lastX = mappedX;
  touchState.lastY = mappedY;
}
