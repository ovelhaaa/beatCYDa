#include "UiApp.h"

#include <Arduino.h>

namespace ui {

bool UiApp::begin() {
  if (!_display.begin()) {
    return false;
  }

  _performScreen.begin();
  _invalidation.invalidateAll();
  _toastUntil = millis() + _toast.timeoutMs;
  return true;
}

void UiApp::runFrame(uint32_t nowMs) {
  _display.readTouch(_touch);
  _performScreen.handleTouch(_touch, _invalidation);

  auto &canvas = _display.canvas();
  _performScreen.render(canvas, _invalidation);

  if (_toastVisible) {
    _toast.draw(canvas);
    if (nowMs >= _toastUntil) {
      _toastVisible = false;
      canvas.fillRect(_toast.rect.x, _toast.rect.y, _toast.rect.w, _toast.rect.h, 0x0000);
    }
  }
}

} // namespace ui
