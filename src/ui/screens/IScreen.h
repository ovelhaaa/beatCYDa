#pragma once

#include "../LgfxDisplay.h"
#include "../core/UiScreenId.h"

struct UiStateSnapshot;

namespace ui {

class IScreen {
public:
  virtual ~IScreen() = default;
  virtual void layout() = 0;
  virtual void render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) = 0;
  virtual bool handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) = 0;
  virtual void invalidate() = 0;
  virtual UiScreenId id() const = 0;
};

} // namespace ui
