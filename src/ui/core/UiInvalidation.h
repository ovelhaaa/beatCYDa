#pragma once

struct UiInvalidation {
  bool topBarDirty = true;
  bool ringDirty = true;
  bool panelDirty = true;
  bool bottomNavDirty = true;
  bool toastDirty = true;
  bool fullScreenDirty = true;

  void invalidateAll() {
    topBarDirty = true;
    ringDirty = true;
    panelDirty = true;
    bottomNavDirty = true;
    toastDirty = true;
    fullScreenDirty = true;
  }

  void markAll() { invalidateAll(); }

  void clear() {
    topBarDirty = false;
    ringDirty = false;
    panelDirty = false;
    bottomNavDirty = false;
    toastDirty = false;
    fullScreenDirty = false;
  }
};
