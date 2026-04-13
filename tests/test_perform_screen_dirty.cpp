/**
 * Tests for the PerformScreen dirty-flag tracking changes introduced in the PR.
 *
 * The PR added:
 *   1. A new member `int _snapshotStep{-1}` (PerformScreen.h)
 *   2. Inside render(): `if (_snapshotStep != snapshot.currentStep) { _ringsDirty = true; }`
 *   3. After rendering:  `_snapshotStep = snapshot.currentStep;`
 *
 * Because PerformScreen::render() requires a real lgfx display device (ESP32
 * hardware), we replicate the exact dirty-flag state machine as a lightweight
 * stand-alone simulation and exercise it here.
 */

#include <cassert>
#include <cstdlib>
#include <iostream>

// ---------------------------------------------------------------------------
// Minimal stand-alone simulation of the changed dirty-flag state in
// PerformScreen::render().  Only the logic that was added/changed in the PR
// is modelled; everything else is omitted.
// ---------------------------------------------------------------------------

struct SimSnapshot {
  int currentStep{0};
  bool isPlaying{false};
};

struct SimPerformScreen {
  // Added by PR (PerformScreen.h)
  int _snapshotStep{-1};

  // Pre-existing dirty flags (only ringsDirty is relevant here)
  bool _ringsDirty{true};
  bool _hasFrame{false};

  // Pre-existing last-state trackers (not changed, included for completeness)
  bool _lastPlaying{false};

  /**
   * Simulate the _snapshotStep / _ringsDirty portion of render().
   * Mirrors lines 155-162 of PerformScreen.cpp:
   *
   *   if (_hasFrame) {
   *     if (_lastPlaying != snapshot.isPlaying) { _ringsDirty = true; ... }
   *     if (_snapshotStep != snapshot.currentStep) { _ringsDirty = true; }
   *     ...
   *   }
   *   ...
   *   _lastPlaying   = snapshot.isPlaying;
   *   _snapshotStep  = snapshot.currentStep;
   *   _hasFrame = true;
   */
  void render(const SimSnapshot &snapshot) {
    if (_hasFrame) {
      if (_lastPlaying != snapshot.isPlaying) {
        _ringsDirty = true;
      }
      if (_snapshotStep != snapshot.currentStep) {
        _ringsDirty = true;
      }
    }

    // Simulate consuming the dirty flag (rings were redrawn)
    _ringsDirty = false;

    // State updates (end of render)
    _lastPlaying = snapshot.isPlaying;
    _snapshotStep = snapshot.currentStep;
    _hasFrame = true;
  }

  /**
   * Check whether _ringsDirty would have been set during the next render
   * without actually consuming it (used to inspect mid-render state).
   */
  bool wouldSetRingsDirty(const SimSnapshot &snapshot) const {
    if (!_hasFrame) return false; // first frame sets everything anyway
    if (_lastPlaying != snapshot.isPlaying) return true;
    if (_snapshotStep != snapshot.currentStep) return true;
    return false;
  }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void pass(const char *name) {
  std::cout << name << " passed!\n";
}

static void fail(const char *name, const char *reason) {
  std::cerr << name << " FAILED: " << reason << "\n";
  std::exit(1);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void test_initial_snapshotStep_is_minus_one() {
  SimPerformScreen screen;
  if (screen._snapshotStep != -1)
    fail(__func__, "_snapshotStep must be initialised to -1");
  pass(__func__);
}

void test_snapshotStep_updated_after_render() {
  SimPerformScreen screen;
  SimSnapshot snap;
  snap.currentStep = 5;

  screen.render(snap);

  if (screen._snapshotStep != 5)
    fail(__func__, "_snapshotStep should equal snapshot.currentStep after render");
  pass(__func__);
}

void test_ringsDirty_set_when_step_changes() {
  SimPerformScreen screen;

  // Establish a first frame so _hasFrame = true
  SimSnapshot snap;
  snap.currentStep = 0;
  screen.render(snap);

  // Advance the step
  snap.currentStep = 1;
  bool dirty = screen.wouldSetRingsDirty(snap);
  if (!dirty)
    fail(__func__, "_ringsDirty should be set when currentStep changes");
  pass(__func__);
}

void test_ringsDirty_not_set_when_step_unchanged() {
  SimPerformScreen screen;

  SimSnapshot snap;
  snap.currentStep = 3;
  screen.render(snap); // sets _snapshotStep = 3

  // Same step: no change
  bool dirty = screen.wouldSetRingsDirty(snap);
  if (dirty)
    fail(__func__, "_ringsDirty must NOT be set when currentStep is unchanged");
  pass(__func__);
}

void test_initial_render_sets_snapshotStep_from_minus_one() {
  // Before the first render, _snapshotStep = -1.  Any currentStep != -1 would
  // have triggered the dirty flag in prior frames, but the first frame always
  // redraws everything anyway (repaintAll path).  After the first render,
  // _snapshotStep must be updated to the real step value.
  SimPerformScreen screen;

  SimSnapshot snap;
  snap.currentStep = 0;
  screen.render(snap);

  if (screen._snapshotStep == -1)
    fail(__func__, "_snapshotStep must not remain -1 after the first render");
  if (screen._snapshotStep != 0)
    fail(__func__, "_snapshotStep should be 0 after first render with step=0");
  pass(__func__);
}

void test_ringsDirty_set_on_every_step_advance() {
  // Simulate a playing sequence advancing one step per render.
  SimPerformScreen screen;

  SimSnapshot snap;
  snap.currentStep = 0;
  snap.isPlaying = true;
  screen.render(snap);

  for (int step = 1; step <= 16; ++step) {
    snap.currentStep = step;
    bool dirty = screen.wouldSetRingsDirty(snap);
    if (!dirty) {
      std::cerr << __func__ << " FAILED: step " << step
                << " should have made _ringsDirty true\n";
      std::exit(1);
    }
    screen.render(snap);
  }
  pass(__func__);
}

void test_ringsDirty_not_set_while_step_holds_steady() {
  // When playback is paused the step counter stops advancing; rings should not
  // be marked dirty repeatedly.
  SimPerformScreen screen;

  SimSnapshot snap;
  snap.currentStep = 7;
  snap.isPlaying = false;
  screen.render(snap); // first render

  // Subsequent renders with the same step should not trigger ringsDirty
  for (int i = 0; i < 10; ++i) {
    bool dirty = screen.wouldSetRingsDirty(snap);
    if (dirty) {
      fail(__func__, "_ringsDirty must not be set when step is unchanged across renders");
    }
    screen.render(snap);
  }
  pass(__func__);
}

void test_snapshotStep_tracks_currentStep_across_multiple_renders() {
  SimPerformScreen screen;
  SimSnapshot snap;

  for (int step = 0; step < 32; ++step) {
    snap.currentStep = step;
    screen.render(snap);
    if (screen._snapshotStep != step) {
      std::cerr << __func__ << " FAILED: _snapshotStep=" << screen._snapshotStep
                << " expected " << step << "\n";
      std::exit(1);
    }
  }
  pass(__func__);
}

void test_ringsDirty_also_set_when_isPlaying_changes() {
  // Pre-existing behaviour: isPlaying change also triggers ringsDirty.
  // Verify it still works alongside the new _snapshotStep check.
  SimPerformScreen screen;

  SimSnapshot snap;
  snap.currentStep = 0;
  snap.isPlaying = false;
  screen.render(snap);

  // Keep same step, but start playing
  snap.isPlaying = true;
  bool dirty = screen.wouldSetRingsDirty(snap);
  if (!dirty)
    fail(__func__, "switching isPlaying must still set _ringsDirty");
  pass(__func__);
}

// Regression: before the PR there was no _snapshotStep tracking at all.
// If currentStep changed between renders, the rings would NOT redraw
// (until another dirty trigger occurred).  The new logic must always
// trigger a redraw when the step advances.
void test_regression_rings_redraw_on_each_step_advance() {
  SimPerformScreen screen;

  SimSnapshot snap;
  snap.currentStep = 4;
  snap.isPlaying = true;
  screen.render(snap); // baseline frame

  // Step advances but isPlaying stays true (no play-state change)
  snap.currentStep = 5;
  bool dirty = screen.wouldSetRingsDirty(snap);
  if (!dirty)
    fail(__func__, "regression: rings must redraw when step advances even if isPlaying is unchanged");
  pass(__func__);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main() {
  test_initial_snapshotStep_is_minus_one();
  test_snapshotStep_updated_after_render();
  test_ringsDirty_set_when_step_changes();
  test_ringsDirty_not_set_when_step_unchanged();
  test_initial_render_sets_snapshotStep_from_minus_one();
  test_ringsDirty_set_on_every_step_advance();
  test_ringsDirty_not_set_while_step_holds_steady();
  test_snapshotStep_tracks_currentStep_across_multiple_renders();
  test_ringsDirty_also_set_when_isPlaying_changes();
  test_regression_rings_redraw_on_each_step_advance();

  std::cout << "All PerformScreen dirty-flag tests passed.\n";
  return 0;
}