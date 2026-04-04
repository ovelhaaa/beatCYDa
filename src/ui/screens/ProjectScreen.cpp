#include "ProjectScreen.h"

#include "../core/UiActions.h"
#include "../theme/UiTheme.h"

namespace ui {
namespace {
void setRect(UiRect &rect, int16_t x, int16_t y, int16_t w, int16_t h) {
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;
}
} // namespace

ProjectScreen::ProjectScreen() {
  _headerCard.title = "PROJECT";
  _headerCard.value = "SLOTS";
  _headerCard.active = true;

  for (uint8_t i = 0; i < 8; ++i) {
    _slotButtons[i].variant = UiButtonVariant::Secondary;
  }

  _loadButton.label = "LOAD";
  _loadButton.variant = UiButtonVariant::Primary;
  _saveButton.label = "SAVE";
  _saveButton.variant = UiButtonVariant::Secondary;
  _deleteButton.label = "DEL";
  _deleteButton.variant = UiButtonVariant::Danger;

  _confirmModal.title = "CONFIRM";
  _confirmModal.body = "Overwrite slot?";
  _confirmModal.visible = false;
  _confirmModal.confirm.label = "YES";
  _confirmModal.confirm.variant = UiButtonVariant::Danger;
  _confirmModal.cancel.label = "NO";
  _confirmModal.cancel.variant = UiButtonVariant::Secondary;

  setRect(_toast.rect, 42, 184, 236, 28);
  layout();
  updateLabels();
}

void ProjectScreen::layout() {
  setRect(_headerCard.rect, 12, 46, 120, 48);

  constexpr int slotW = 66;
  constexpr int slotH = 30;
  constexpr int startX = 144;
  constexpr int startY = 48;
  constexpr int gapX = 10;
  constexpr int gapY = 8;

  for (int i = 0; i < 8; ++i) {
    const int row = i / 4;
    const int col = i % 4;
    setRect(_slotButtons[i].rect, startX + col * (slotW + gapX), startY + row * (slotH + gapY), slotW, slotH);
  }

  setRect(_loadButton.rect, 12, 110, 92, 34);
  setRect(_saveButton.rect, 110, 110, 92, 34);
  setRect(_deleteButton.rect, 208, 110, 100, 34);

  setRect(_confirmModal.rect, 44, 88, 232, 96);
  setRect(_confirmModal.confirm.rect, 58, 144, 96, 30);
  setRect(_confirmModal.cancel.rect, 166, 144, 96, 30);
}

void ProjectScreen::updateLabels() {
  for (uint8_t i = 0; i < 8; ++i) {
    snprintf(_slotLabels[i], sizeof(_slotLabels[i]), "SLOT %d", i + 1);
    _slotButtons[i].label = _slotLabels[i];
    _slotButtons[i].pressed = (i == _selectedSlot);
  }
}

void ProjectScreen::showToast(const char *message, UiToastSeverity severity, uint32_t timeoutMs) {
  _toast.message = message;
  _toast.severity = severity;
  _toast.timeoutMs = timeoutMs;
  _toastUntilMs = millis() + timeoutMs;
}

void ProjectScreen::openConfirm(PendingAction action) {
  _pendingAction = action;
  _confirmModal.visible = true;
  _confirmModal.body = (action == PendingAction::Delete) ? "Delete slot data?" : "Overwrite slot?";
}

void ProjectScreen::closeConfirm() {
  _pendingAction = PendingAction::None;
  _confirmModal.visible = false;
}

void ProjectScreen::render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) {
  canvas.fillRect(0,
                  theme::UiTheme::Metrics::TopBarH,
                  theme::UiTheme::Metrics::ScreenW,
                  theme::UiTheme::Metrics::ScreenH -
                      theme::UiTheme::Metrics::TopBarH - theme::UiTheme::Metrics::BottomNavH,
                  theme::UiTheme::Colors::Bg);

  _headerCard.draw(canvas);
  updateLabels();

  canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
  canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::Bg);
  canvas.setCursor(12, 98);
  canvas.printf("BPM %d  TRK %d", snapshot.bpm, snapshot.activeTrack + 1);

  for (uint8_t i = 0; i < 8; ++i) {
    _slotButtons[i].variant = _slotOccupied[i] ? UiButtonVariant::Primary : UiButtonVariant::Secondary;
    _slotButtons[i].draw(canvas);
  }

  _loadButton.disabled = !_slotOccupied[_selectedSlot];
  _deleteButton.disabled = !_slotOccupied[_selectedSlot];

  _loadButton.draw(canvas);
  _saveButton.draw(canvas);
  _deleteButton.draw(canvas);

  const bool toastVisible = _toastUntilMs > millis();
  if (toastVisible) {
    _toast.draw(canvas);
  }

  if (_confirmModal.visible) {
    _confirmModal.draw(canvas);
  }

  _dirty = false;
}

bool ProjectScreen::handleTouch(const TouchPoint &tp, const UiStateSnapshot &snapshot) {
  if (!tp.justPressed) {
    (void)snapshot;
    return false;
  }

  if (_confirmModal.visible) {
    if (_confirmModal.confirm.hitTest(tp.x, tp.y)) {
      if (_pendingAction == PendingAction::Save) {
        dispatchSaveSlot(_selectedSlot);
        _slotOccupied[_selectedSlot] = true;
        showToast("Slot saved");
      } else if (_pendingAction == PendingAction::Delete) {
        _slotOccupied[_selectedSlot] = false;
        showToast("Slot cleared", UiToastSeverity::Warning);
      }
      closeConfirm();
      _dirty = true;
      return true;
    }

    if (_confirmModal.cancel.hitTest(tp.x, tp.y)) {
      closeConfirm();
      showToast("Canceled", UiToastSeverity::Warning, 900);
      _dirty = true;
      return true;
    }

    return true;
  }

  for (uint8_t i = 0; i < 8; ++i) {
    if (_slotButtons[i].hitTest(tp.x, tp.y)) {
      _selectedSlot = i;
      _dirty = true;
      return true;
    }
  }

  if (_loadButton.hitTest(tp.x, tp.y)) {
    dispatchLoadSlot(_selectedSlot);
    showToast("Slot loaded");
    _dirty = true;
    return true;
  }

  if (_saveButton.hitTest(tp.x, tp.y)) {
    if (_slotOccupied[_selectedSlot]) {
      openConfirm(PendingAction::Save);
    } else {
      dispatchSaveSlot(_selectedSlot);
      _slotOccupied[_selectedSlot] = true;
      showToast("Slot saved");
    }
    _dirty = true;
    return true;
  }

  if (_deleteButton.hitTest(tp.x, tp.y)) {
    openConfirm(PendingAction::Delete);
    _dirty = true;
    return true;
  }

  (void)snapshot;
  return false;
}

void ProjectScreen::invalidate() {
  _dirty = true;
}

} // namespace ui
