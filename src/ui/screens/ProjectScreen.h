#pragma once

#include "../components/UiButton.h"
#include "../components/UiCard.h"
#include "../components/UiModal.h"
#include "../components/UiToast.h"
#include "../Display.h"
#include "IScreen.h"

struct UiStateSnapshot;

namespace ui {

class ProjectScreen : public IScreen {
public:
  ProjectScreen();

  void layout() override;
  void render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) override;
  bool handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) override;
  void invalidate() override;
  UiScreenId id() const override { return UiScreenId::Project; }

private:
  enum class PendingAction : uint8_t {
    None,
    Save,
    Delete,
  };

  void showToast(const char *message, UiToastSeverity severity = UiToastSeverity::Info, uint32_t timeoutMs = 1400);
  void openConfirm(PendingAction action);
  void closeConfirm();
  void updateLabels();

  bool _dirty{true};
  bool _slotOccupied[8]{};
  uint8_t _selectedSlot{0};
  uint32_t _toastUntilMs{0};
  PendingAction _pendingAction{PendingAction::None};

  UiCard _headerCard{};
  UiButton _slotButtons[8]{};
  UiButton _loadButton{};
  UiButton _saveButton{};
  UiButton _deleteButton{};
  UiToast _toast{};
  UiModal _confirmModal{};

  char _slotLabels[8][12]{};
};

} // namespace ui
