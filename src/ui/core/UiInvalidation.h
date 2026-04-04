#pragma once

struct UiInvalidation {
  bool topBarDirty = true;
  bool ringDirty = true;
  bool panelDirty = true;
  bool bottomNavDirty = true;
  bool toastDirty = true;
  bool fullScreenDirty = true;

  void markAll() {
    topBarDirty = true;
    ringDirty = true;
    panelDirty = true;
    bottomNavDirty = true;
    toastDirty = true;
    fullScreenDirty = true;
  }

  void clear() {
    topBarDirty = false;
    ringDirty = false;
    panelDirty = false;
    bottomNavDirty = false;
    toastDirty = false;
    fullScreenDirty = false;
  }
};
