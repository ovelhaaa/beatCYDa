#include "ProjectScreen.h"

#include "../../CYD_Config.h"
#include "../core/UiActions.h"
#include "../theme/UiTheme.h"

namespace ui {
namespace {
constexpr int kPanelTopY = theme::UiTheme::Metrics::TopBarH;
constexpr int kPanelH = theme::UiTheme::Metrics::ContentH;

void setRect(UiRect &rect, int16_t x, int16_t y, int16_t w, int16_t h) {
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;
}
} // namespace

ProjectScreen::ProjectScreen() {
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

  layout();
  updateLabels();
}

void ProjectScreen::layout() {
  setRect(_titleChipRect,
          theme::UiTheme::Metrics::ProjectTitleChipX,
          theme::UiTheme::Metrics::ProjectTitleChipY,
          theme::UiTheme::Metrics::ProjectTitleChipW,
          theme::UiTheme::Metrics::ProjectTitleChipH);
  setRect(_selectedInfoRect,
          theme::UiTheme::Metrics::ProjectSelectedInfoX,
          theme::UiTheme::Metrics::ProjectSelectedInfoY,
          theme::UiTheme::Metrics::ProjectSelectedInfoW,
          theme::UiTheme::Metrics::ProjectSelectedInfoH);

  for (int i = 0; i < 8; ++i) {
    const int row = i / 4;
    const int col = i % 4;
    setRect(_slotButtons[i].rect,
            theme::UiTheme::Metrics::ProjectSlotGridX + col * (theme::UiTheme::Metrics::ProjectSlotW + theme::UiTheme::Metrics::ProjectSlotGapX),
            theme::UiTheme::Metrics::ProjectSlotGridY + row * (theme::UiTheme::Metrics::ProjectSlotH + theme::UiTheme::Metrics::ProjectSlotGapY),
            theme::UiTheme::Metrics::ProjectSlotW,
            theme::UiTheme::Metrics::ProjectSlotH);
  }

  const int actionX = theme::UiTheme::Metrics::ProjectSlotGridX;
  setRect(_loadButton.rect,
          actionX,
          theme::UiTheme::Metrics::ProjectActionY,
          theme::UiTheme::Metrics::ProjectActionW,
          theme::UiTheme::Metrics::ProjectActionH);
  setRect(_saveButton.rect,
          actionX + theme::UiTheme::Metrics::ProjectActionW + theme::UiTheme::Metrics::ProjectActionGap,
          theme::UiTheme::Metrics::ProjectActionY,
          theme::UiTheme::Metrics::ProjectActionW,
          theme::UiTheme::Metrics::ProjectActionH);
  setRect(_deleteButton.rect,
          actionX + (theme::UiTheme::Metrics::ProjectActionW + theme::UiTheme::Metrics::ProjectActionGap) * 2,
          theme::UiTheme::Metrics::ProjectActionY,
          theme::UiTheme::Metrics::ProjectActionW,
          theme::UiTheme::Metrics::ProjectActionH);

  setRect(_confirmModal.rect,
          theme::UiTheme::Metrics::ProjectModalX,
          theme::UiTheme::Metrics::ProjectModalY,
          theme::UiTheme::Metrics::ProjectModalW,
          theme::UiTheme::Metrics::ProjectModalH);
  setRect(_confirmModal.confirm.rect,
          theme::UiTheme::Metrics::ProjectModalX + 10,
          theme::UiTheme::Metrics::ProjectModalButtonY,
          theme::UiTheme::Metrics::ProjectModalButtonW,
          theme::UiTheme::Metrics::ProjectModalButtonH);
  setRect(_confirmModal.cancel.rect,
          theme::UiTheme::Metrics::ProjectModalX + theme::UiTheme::Metrics::ProjectModalW -
              theme::UiTheme::Metrics::ProjectModalButtonW - 10,
          theme::UiTheme::Metrics::ProjectModalButtonY,
          theme::UiTheme::Metrics::ProjectModalButtonW,
          theme::UiTheme::Metrics::ProjectModalButtonH);

  setRect(_toast.rect,
          theme::UiTheme::Metrics::ProjectToastX,
          theme::UiTheme::Metrics::ProjectToastY,
          theme::UiTheme::Metrics::ProjectToastW,
          theme::UiTheme::Metrics::ProjectToastH);
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
  markOverlayDirty();
}

void ProjectScreen::openConfirm(PendingAction action) {
  _pendingAction = action;
  _confirmModal.visible = true;
  _confirmModal.body = (action == PendingAction::Delete) ? "Delete selected slot?" : "Overwrite selected slot?";
  markOverlayDirty();
}

void ProjectScreen::closeConfirm() {
  _pendingAction = PendingAction::None;
  _confirmModal.visible = false;
  markOverlayDirty();
}

void ProjectScreen::markSlotsDirty() {
  _slotsDirty = true;
  _actionsDirty = true;
}

void ProjectScreen::markActionsDirty() {
  _actionsDirty = true;
}

void ProjectScreen::markOverlayDirty() {
  _overlayDirty = true;
}

void ProjectScreen::render(lgfx::LGFX_Device &canvas, const UiStateSnapshot &snapshot) {
  const bool forceFullRender = _dirty || !_hasFrame;

  if (_hasFrame) {
    if (_lastBpm != snapshot.bpm || _lastActiveTrack != snapshot.activeTrack) {
      _statusDirty = true;
    }

    if (_lastSelectedSlot != _selectedSlot) {
      markSlotsDirty();
      _statusDirty = true;
    }

    for (uint8_t i = 0; i < 8; ++i) {
      if (_lastSlotOccupied[i] != _slotOccupied[i]) {
        markSlotsDirty();
        break;
      }
    }
  }

  if (forceFullRender) {
    canvas.fillRect(0, kPanelTopY, theme::UiTheme::Metrics::ScreenW, kPanelH, theme::UiTheme::Colors::Bg);
    _headerDirty = true;
    _statusDirty = true;
    _slotsDirty = true;
    _actionsDirty = true;
    _overlayDirty = true;
  }

  _loadButton.disabled = !_slotOccupied[_selectedSlot];
  _deleteButton.disabled = !_slotOccupied[_selectedSlot];

  const bool toastVisible = _toastUntilMs > millis();
  if (_lastToastVisible != toastVisible || _lastModalVisible != _confirmModal.visible) {
    markOverlayDirty();
  }
  const bool baseSectionsDirty = _headerDirty || _statusDirty || _slotsDirty || _actionsDirty;
  if (_confirmModal.visible && baseSectionsDirty) {
    markOverlayDirty();
  }

  if (_headerDirty) {
    canvas.fillRect(_titleChipRect.x,
                    _titleChipRect.y,
                    _selectedInfoRect.x + _selectedInfoRect.w - _titleChipRect.x,
                    _titleChipRect.h,
                    theme::UiTheme::Colors::Bg);

    canvas.fillRoundRect(_titleChipRect.x,
                         _titleChipRect.y,
                         _titleChipRect.w,
                         _titleChipRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::Accent);
    canvas.drawRoundRect(_titleChipRect.x,
                         _titleChipRect.y,
                         _titleChipRect.w,
                         _titleChipRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::Outline);
    canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
    canvas.setTextColor(theme::UiTheme::Colors::TextOnAccent, theme::UiTheme::Colors::Accent);
    canvas.setCursor(_titleChipRect.x + 8, _titleChipRect.y + 8);
    canvas.print("PROJECT");

    _headerDirty = false;
  }

  if (_statusDirty) {
    canvas.fillRect(_selectedInfoRect.x,
                    _selectedInfoRect.y,
                    _selectedInfoRect.w,
                    _selectedInfoRect.h,
                    theme::UiTheme::Colors::Bg);
    canvas.fillRoundRect(_selectedInfoRect.x,
                         _selectedInfoRect.y,
                         _selectedInfoRect.w,
                         _selectedInfoRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::SurfacePressed);
    canvas.drawRoundRect(_selectedInfoRect.x,
                         _selectedInfoRect.y,
                         _selectedInfoRect.w,
                         _selectedInfoRect.h,
                         theme::UiTheme::Metrics::RadiusSm,
                         theme::UiTheme::Colors::Outline);
    canvas.setTextSize(theme::UiTheme::Typography::CaptionSize);
    canvas.setTextColor(theme::UiTheme::Colors::TextSecondary, theme::UiTheme::Colors::SurfacePressed);
    canvas.setCursor(_selectedInfoRect.x + 6, _selectedInfoRect.y + 8);
    canvas.printf("Selected: SLOT %u  BPM %u", static_cast<unsigned>(_selectedSlot + 1), static_cast<unsigned>(snapshot.bpm));
    _statusDirty = false;
  }

  if (_slotsDirty) {
    canvas.fillRect(theme::UiTheme::Metrics::ProjectSlotGridX,
                    theme::UiTheme::Metrics::ProjectSlotGridY,
                    (theme::UiTheme::Metrics::ProjectSlotW * 4) + (theme::UiTheme::Metrics::ProjectSlotGapX * 3),
                    (theme::UiTheme::Metrics::ProjectSlotH * 2) + theme::UiTheme::Metrics::ProjectSlotGapY,
                    theme::UiTheme::Colors::Bg);
    updateLabels();
    for (uint8_t i = 0; i < 8; ++i) {
      _slotButtons[i].variant = _slotOccupied[i] ? UiButtonVariant::Primary : UiButtonVariant::Secondary;
      _slotButtons[i].pressed = (i == _selectedSlot);
      _slotButtons[i].draw(canvas);
    }
    _slotsDirty = false;
  }

  if (_actionsDirty) {
    canvas.fillRect(_loadButton.rect.x,
                    _loadButton.rect.y,
                    _deleteButton.rect.x + _deleteButton.rect.w - _loadButton.rect.x,
                    _loadButton.rect.h,
                    theme::UiTheme::Colors::Bg);
    _loadButton.draw(canvas);
    _saveButton.draw(canvas);
    _deleteButton.draw(canvas);
    _actionsDirty = false;
  }

  if (_overlayDirty) {
    if (_lastToastVisible || toastVisible) {
      canvas.fillRect(_toast.rect.x, _toast.rect.y, _toast.rect.w, _toast.rect.h, theme::UiTheme::Colors::Bg);
    }
    if (_lastModalVisible || _confirmModal.visible) {
      canvas.fillRect(_confirmModal.rect.x, _confirmModal.rect.y, _confirmModal.rect.w, _confirmModal.rect.h, theme::UiTheme::Colors::Bg);
    }
    if (toastVisible) {
      _toast.draw(canvas);
    }
    if (_confirmModal.visible) {
      _confirmModal.draw(canvas);
    }
    _overlayDirty = false;
  }

  for (uint8_t i = 0; i < 8; ++i) {
    _lastSlotOccupied[i] = _slotOccupied[i];
  }
  _lastSelectedSlot = _selectedSlot;
  _lastBpm = snapshot.bpm;
  _lastActiveTrack = snapshot.activeTrack;
  _lastToastVisible = toastVisible;
  _lastModalVisible = _confirmModal.visible;
  _hasFrame = true;
  _dirty = false;
}

bool ProjectScreen::wantsContinuousRedraw(uint32_t nowMs) {
  const bool toastVisible = _toastUntilMs > nowMs;
  if (_confirmModal.visible || toastVisible) {
    return true;
  }

  return _lastToastVisible != toastVisible || _lastModalVisible != _confirmModal.visible;
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
        showToast("Slot saved", UiToastSeverity::Info, CYDConfig::UiToastInfoMs);
        markSlotsDirty();
      } else if (_pendingAction == PendingAction::Delete) {
        _slotOccupied[_selectedSlot] = false;
        showToast("Slot cleared", UiToastSeverity::Warning, CYDConfig::UiToastWarningMs);
        markSlotsDirty();
      }
      closeConfirm();
      return true;
    }

    if (_confirmModal.cancel.hitTest(tp.x, tp.y)) {
      closeConfirm();
      showToast("Canceled", UiToastSeverity::Warning, CYDConfig::UiToastWarningMs);
      return true;
    }

    return true;
  }

  for (uint8_t i = 0; i < 8; ++i) {
    if (_slotButtons[i].hitTest(tp.x, tp.y)) {
      _selectedSlot = i;
      markSlotsDirty();
      _statusDirty = true;
      return true;
    }
  }

  if (_loadButton.hitTest(tp.x, tp.y)) {
    dispatchLoadSlot(_selectedSlot);
    showToast("Slot loaded", UiToastSeverity::Info, CYDConfig::UiToastInfoMs);
    markActionsDirty();
    return true;
  }

  if (_saveButton.hitTest(tp.x, tp.y)) {
    if (_slotOccupied[_selectedSlot]) {
      openConfirm(PendingAction::Save);
    } else {
      dispatchSaveSlot(_selectedSlot);
      _slotOccupied[_selectedSlot] = true;
      showToast("Slot saved", UiToastSeverity::Info, CYDConfig::UiToastInfoMs);
      markSlotsDirty();
    }
    return true;
  }

  if (_slotOccupied[_selectedSlot] && _deleteButton.hitTest(tp.x, tp.y)) {
    openConfirm(PendingAction::Delete);
    return true;
  }

  (void)snapshot;
  return false;
}

void ProjectScreen::invalidate() {
  _dirty = true;
  _headerDirty = true;
  _statusDirty = true;
  _slotsDirty = true;
  _actionsDirty = true;
  _overlayDirty = true;
}

} // namespace ui
