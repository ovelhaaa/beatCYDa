/**
 * Tests for logic changes introduced in UiEuclideanRings.cpp.
 *
 * The changed code lives inside an anonymous namespace, so this file
 * replicates the exact functions verbatim and then exercises the key
 * algorithmic decisions: clampLen, dimColor, stepAngleRad, per-track
 * playStep computation, isPlayStep detection, enlarged step/hit sizes
 * when the playhead is on a step, and the isPlaying guard for the
 * playhead line.
 */

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

// ---------------------------------------------------------------------------
// Replicated helpers (verbatim from the anonymous namespace in
// UiEuclideanRings.cpp) so that we can exercise them without pulling in the
// full ESP32 / LovyanGFX dependency chain.
// ---------------------------------------------------------------------------

static constexpr float kPi = 3.14159265f;

static float stepAngleRad(int step, int patternLen) {
  const float turns = static_cast<float>(step) / static_cast<float>(patternLen);
  const float deg = -90.0f + (turns * 360.0f);
  return deg * (kPi / 180.0f);
}

static uint16_t dimColor(uint16_t rgb565, float factor) {
  const uint8_t r = static_cast<uint8_t>((((rgb565 >> 11) & 0x1F) * factor));
  const uint8_t g = static_cast<uint8_t>((((rgb565 >> 5) & 0x3F) * factor));
  const uint8_t b = static_cast<uint8_t>(((rgb565 & 0x1F) * factor));
  return static_cast<uint16_t>(((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
}

static int clampLen(int value) {
  if (value < 1) {
    return 1;
  }
  if (value > 64) {
    return 64;
  }
  return value;
}

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
// clampLen tests
// ---------------------------------------------------------------------------

void test_clampLen_below_minimum_returns_one() {
  if (clampLen(0) != 1) fail(__func__, "clampLen(0) should return 1");
  if (clampLen(-1) != 1) fail(__func__, "clampLen(-1) should return 1");
  if (clampLen(-100) != 1) fail(__func__, "clampLen(-100) should return 1");
  pass(__func__);
}

void test_clampLen_within_range_passthrough() {
  if (clampLen(1) != 1) fail(__func__, "clampLen(1) should return 1");
  if (clampLen(16) != 16) fail(__func__, "clampLen(16) should return 16");
  if (clampLen(32) != 32) fail(__func__, "clampLen(32) should return 32");
  if (clampLen(64) != 64) fail(__func__, "clampLen(64) should return 64");
  pass(__func__);
}

void test_clampLen_above_maximum_returns_64() {
  if (clampLen(65) != 64) fail(__func__, "clampLen(65) should return 64");
  if (clampLen(128) != 64) fail(__func__, "clampLen(128) should return 64");
  if (clampLen(1000) != 64) fail(__func__, "clampLen(1000) should return 64");
  pass(__func__);
}

// ---------------------------------------------------------------------------
// dimColor tests
// ---------------------------------------------------------------------------

void test_dimColor_black_stays_black() {
  if (dimColor(0x0000, 1.0f) != 0x0000)
    fail(__func__, "dimColor(0, 1.0) should stay 0");
  if (dimColor(0x0000, 0.5f) != 0x0000)
    fail(__func__, "dimColor(0, 0.5) should stay 0");
  pass(__func__);
}

void test_dimColor_full_factor_returns_original_color() {
  // A colour where all channels are at max: r=0x1F, g=0x3F, b=0x1F → 0xFFFF
  uint16_t full = 0xFFFF;
  if (dimColor(full, 1.0f) != full)
    fail(__func__, "dimColor(0xFFFF, 1.0) should equal 0xFFFF");
  pass(__func__);
}

void test_dimColor_zero_factor_returns_black() {
  uint16_t color = 0xF800; // red only
  if (dimColor(color, 0.0f) != 0x0000)
    fail(__func__, "dimColor with factor 0 should return 0");
  pass(__func__);
}

void test_dimColor_halves_channel_values() {
  // Construct a colour with known channels: r=20, g=40, b=16
  uint16_t color = static_cast<uint16_t>(((20u & 0x1Fu) << 11) | ((40u & 0x3Fu) << 5) | (16u & 0x1Fu));
  uint16_t dimmed = dimColor(color, 0.5f);
  uint8_t r = (dimmed >> 11) & 0x1F;
  uint8_t g = (dimmed >> 5) & 0x3F;
  uint8_t b = dimmed & 0x1F;
  if (r != static_cast<uint8_t>(20 * 0.5f))
    fail(__func__, "red channel not halved correctly");
  if (g != static_cast<uint8_t>(40 * 0.5f))
    fail(__func__, "green channel not halved correctly");
  if (b != static_cast<uint8_t>(16 * 0.5f))
    fail(__func__, "blue channel not halved correctly");
  pass(__func__);
}

// ---------------------------------------------------------------------------
// stepAngleRad tests
// ---------------------------------------------------------------------------

void test_stepAngleRad_step0_is_minus_90_degrees() {
  // Step 0 should map to -90 degrees (top of circle), regardless of patternLen
  float angle = stepAngleRad(0, 16);
  float expected = -90.0f * (kPi / 180.0f);
  if (std::fabs(angle - expected) > 1e-5f)
    fail(__func__, "step 0 angle should be -PI/2");
  pass(__func__);
}

void test_stepAngleRad_half_pattern_is_90_degrees() {
  // Step 8 of 16 = 0.5 * 360 - 90 = 90 degrees
  float angle = stepAngleRad(8, 16);
  float expected = 90.0f * (kPi / 180.0f);
  if (std::fabs(angle - expected) > 1e-5f)
    fail(__func__, "step 8/16 angle should be +PI/2");
  pass(__func__);
}

void test_stepAngleRad_full_pattern_is_270_degrees() {
  // Step 12 of 16 = 0.75 * 360 - 90 = 180 degrees
  float angle = stepAngleRad(12, 16);
  float expected = 180.0f * (kPi / 180.0f);
  if (std::fabs(angle - expected) > 1e-5f)
    fail(__func__, "step 12/16 angle should be PI");
  pass(__func__);
}

// ---------------------------------------------------------------------------
// Per-track playStep computation
// (Changed in PR: was using activeTrack only; now computed per-track using
//  each track's own patternLen via clampLen.)
// ---------------------------------------------------------------------------

// Simulate the computation for a single track as the source does:
//   const int len = clampLen(snapshot.patternLens[track]);
//   const int playStep = snapshot.currentStep % len;
static int computePlayStep(int currentStep, int patternLen) {
  const int len = clampLen(patternLen);
  return currentStep % len;
}

void test_playStep_basic_modulo() {
  // currentStep=5, patternLen=16 → playStep=5
  if (computePlayStep(5, 16) != 5)
    fail(__func__, "playStep(5, 16) should be 5");
  pass(__func__);
}

void test_playStep_wraps_at_pattern_length() {
  // currentStep=16, patternLen=16 → playStep=0
  if (computePlayStep(16, 16) != 0)
    fail(__func__, "playStep(16, 16) should wrap to 0");
  pass(__func__);
}

void test_playStep_large_step_wraps_correctly() {
  // currentStep=33, patternLen=16 → playStep=1
  if (computePlayStep(33, 16) != 1)
    fail(__func__, "playStep(33, 16) should be 1");
  pass(__func__);
}

void test_playStep_uses_clamped_length_for_zero_patternLen() {
  // patternLen=0 → clampLen returns 1 → playStep = currentStep % 1 = 0 always
  if (computePlayStep(7, 0) != 0)
    fail(__func__, "playStep with patternLen=0 should always be 0");
  if (computePlayStep(0, 0) != 0)
    fail(__func__, "playStep(0, 0) should be 0");
  pass(__func__);
}

void test_playStep_different_tracks_use_own_length() {
  // Track A has len=8, track B has len=16.  At currentStep=10:
  //   track A playStep = 10 % 8 = 2
  //   track B playStep = 10 % 16 = 10
  int currentStep = 10;
  int playStepA = computePlayStep(currentStep, 8);
  int playStepB = computePlayStep(currentStep, 16);
  if (playStepA != 2)
    fail(__func__, "track A playStep(10, 8) should be 2");
  if (playStepB != 10)
    fail(__func__, "track B playStep(10, 16) should be 10");
  if (playStepA == playStepB)
    fail(__func__, "tracks with different lengths should produce different playSteps");
  pass(__func__);
}

// Regression: before the PR, all tracks shared the active track's playStep.
// This means a non-active track with a different length would show the wrong
// step highlighted. The new per-track computation must give a different result.
void test_playStep_per_track_differs_from_active_track_result() {
  int currentStep = 10;
  int activeTrackLen = 16;
  int otherTrackLen = 6;

  int playStepActive = computePlayStep(currentStep, activeTrackLen); // 10
  int playStepOther = computePlayStep(currentStep, otherTrackLen);   // 4

  if (playStepActive == playStepOther)
    fail(__func__, "tracks with different lengths must produce different playSteps");
  pass(__func__);
}

// ---------------------------------------------------------------------------
// isPlayStep detection and step size enlargement
// ---------------------------------------------------------------------------

// Simulate: const bool isPlayStep = (step == playStep);
// Simulate: const int stepSize = (_compact ? 1 : 2) + (isPlayStep ? 1 : 0);
static int computeStepSize(bool compact, bool isPlayStep) {
  return (compact ? 1 : 2) + (isPlayStep ? 1 : 0);
}

void test_stepSize_enlarged_when_isPlayStep_compact() {
  int normalSize = computeStepSize(true, false);
  int playSize = computeStepSize(true, true);
  if (playSize != normalSize + 1)
    fail(__func__, "play step size should be +1 in compact mode");
  pass(__func__);
}

void test_stepSize_enlarged_when_isPlayStep_noncompact() {
  int normalSize = computeStepSize(false, false);
  int playSize = computeStepSize(false, true);
  if (playSize != normalSize + 1)
    fail(__func__, "play step size should be +1 in non-compact mode");
  pass(__func__);
}

void test_stepSize_not_enlarged_for_non_play_step() {
  // Any step that is NOT the current playStep must use the base size
  if (computeStepSize(false, false) != 2)
    fail(__func__, "non-play step size in non-compact mode should be 2");
  if (computeStepSize(true, false) != 1)
    fail(__func__, "non-play step size in compact mode should be 1");
  pass(__func__);
}

// ---------------------------------------------------------------------------
// Hit size enlargement
// Simulate: const int hitSize = active ? (_compact ? 2 : 3) : (_compact ? 1 : 2);
//           const int emphasizedHitSize = hitSize + (isPlayStep ? 1 : 0);
// ---------------------------------------------------------------------------

static int computeHitSize(bool active, bool compact, bool isPlayStep) {
  const int hitSize = active ? (compact ? 2 : 3) : (compact ? 1 : 2);
  return hitSize + (isPlayStep ? 1 : 0);
}

void test_hitSize_enlarged_when_isPlayStep_active_compact() {
  int normal = computeHitSize(true, true, false);
  int play   = computeHitSize(true, true, true);
  if (play != normal + 1)
    fail(__func__, "active compact hit at playStep should be +1");
  pass(__func__);
}

void test_hitSize_enlarged_when_isPlayStep_active_noncompact() {
  int normal = computeHitSize(true, false, false);
  int play   = computeHitSize(true, false, true);
  if (play != normal + 1)
    fail(__func__, "active non-compact hit at playStep should be +1");
  pass(__func__);
}

void test_hitSize_enlarged_when_isPlayStep_inactive_track() {
  int normal = computeHitSize(false, false, false);
  int play   = computeHitSize(false, false, true);
  if (play != normal + 1)
    fail(__func__, "inactive track hit at playStep should be +1");
  pass(__func__);
}

void test_hitSize_not_enlarged_for_non_play_step() {
  // active, non-compact: base = 3
  if (computeHitSize(true, false, false) != 3)
    fail(__func__, "active non-compact hitSize base should be 3");
  // active, compact: base = 2
  if (computeHitSize(true, true, false) != 2)
    fail(__func__, "active compact hitSize base should be 2");
  // inactive, non-compact: base = 2
  if (computeHitSize(false, false, false) != 2)
    fail(__func__, "inactive non-compact hitSize base should be 2");
  // inactive, compact: base = 1
  if (computeHitSize(false, true, false) != 1)
    fail(__func__, "inactive compact hitSize base should be 1");
  pass(__func__);
}

// ---------------------------------------------------------------------------
// isPlaying guard for playhead line
// (Changed in PR: playhead line is only drawn when snapshot.isPlaying is true.)
// ---------------------------------------------------------------------------

// Simulate the conditional: the playhead line drawing block is guarded by
//   if (snapshot.isPlaying) { ... drawLine ... }
// We verify the guard condition is correct boolean logic.

static bool shouldDrawPlayheadLine(bool isPlaying) {
  return isPlaying;
}

void test_playhead_line_drawn_only_when_playing() {
  if (!shouldDrawPlayheadLine(true))
    fail(__func__, "playhead line should be drawn when isPlaying=true");
  if (shouldDrawPlayheadLine(false))
    fail(__func__, "playhead line must NOT be drawn when isPlaying=false");
  pass(__func__);
}

// Regression: before the PR the playhead line was always drawn regardless of
// isPlaying state.  Verify the new guard produces the correct result at the
// boundary (just-stopped / just-started transition).
void test_playhead_line_guard_boundary_transition() {
  // Transitioning from playing to stopped
  bool wasPlaying = true;
  bool nowStopped = false;
  if (shouldDrawPlayheadLine(wasPlaying) == shouldDrawPlayheadLine(nowStopped))
    fail(__func__, "playing vs stopped must yield different draw decisions");
  pass(__func__);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main() {
  test_clampLen_below_minimum_returns_one();
  test_clampLen_within_range_passthrough();
  test_clampLen_above_maximum_returns_64();

  test_dimColor_black_stays_black();
  test_dimColor_full_factor_returns_original_color();
  test_dimColor_zero_factor_returns_black();
  test_dimColor_halves_channel_values();

  test_stepAngleRad_step0_is_minus_90_degrees();
  test_stepAngleRad_half_pattern_is_90_degrees();
  test_stepAngleRad_full_pattern_is_270_degrees();

  test_playStep_basic_modulo();
  test_playStep_wraps_at_pattern_length();
  test_playStep_large_step_wraps_correctly();
  test_playStep_uses_clamped_length_for_zero_patternLen();
  test_playStep_different_tracks_use_own_length();
  test_playStep_per_track_differs_from_active_track_result();

  test_stepSize_enlarged_when_isPlayStep_compact();
  test_stepSize_enlarged_when_isPlayStep_noncompact();
  test_stepSize_not_enlarged_for_non_play_step();

  test_hitSize_enlarged_when_isPlayStep_active_compact();
  test_hitSize_enlarged_when_isPlayStep_active_noncompact();
  test_hitSize_enlarged_when_isPlayStep_inactive_track();
  test_hitSize_not_enlarged_for_non_play_step();

  test_playhead_line_drawn_only_when_playing();
  test_playhead_line_guard_boundary_transition();

  std::cout << "All euclidean rings logic tests passed.\n";
  return 0;
}