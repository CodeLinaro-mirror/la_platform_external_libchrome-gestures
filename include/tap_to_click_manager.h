// Copyright 2026 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_TAP_TO_CLICK_MANAGER_H_
#define GESTURES_TAP_TO_CLICK_MANAGER_H_

#include <map>
#include <optional>
#include <set>

#include <gtest/gtest.h>

#include "include/finger_metrics.h"
#include "include/gestures.h"
#include "include/macros.h"
#include "include/prop_registry.h"
#include "include/tracer.h"

namespace gestures {

class HardwareStateBuffer;
class TapToClickManager;

enum class TapToClickState {
  kIdle,
  kFirstTapBegan,
  kTapComplete,
  kSubsequentTapBegan,
  kDrag,
  kDragRelease,
  kDragRetouch,
};

class TapRecord {
  FRIEND_TEST(ImmediateInterpreterTest, TapRecordTest);

 public:
  explicit TapRecord(const TapToClickManager* manager);
  void Update(const HardwareState& hwstate,
              const HardwareState& prev_hwstate,
              const std::set<short>& added,
              const std::set<short>& removed,
              const std::set<short>& dead);
  void Clear();

  // Returns true if any gesturing fingers appear to be moving.
  bool Moving(const HardwareState& hwstate, float dist_max) const;
  bool Motionless(const HardwareState& hwstate,
                  const HardwareState& prev_hwstate,
                  float max_speed) const;

  bool TapBegan() const;
  bool TapComplete() const;
  // Returns GESTURES_BUTTON_* value or 0, if tap was too light.
  int TapType() const;
  // Returns true if any contact has met the minimum pressure threshold.
  bool MinTapPressureMet() const;
  bool FingersBelowMaxAge() const;

 private:
  void NoteTouch(short the_id, const FingerState& fs);
  void NoteRelease(short the_id);
  void Remove(short the_id);

  float CotapMinPressure() const;

  std::map<short, FingerState> touched_;
  std::set<short> released_;
  // At least one finger must meet the minimum pressure requirement during a
  // tap. This set contains the fingers that have.
  std::set<short> min_tap_pressure_met_;
  // All fingers must meet the cotap pressure, which is half of the min tap
  // pressure.
  std::set<short> min_cotap_pressure_met_;
  // Used to fetch properties.
  const TapToClickManager* manager_;
  // T5R2: For these pads, we try to track individual IDs, but if we get an
  // input event with insufficient data, we switch into T5R2 mode, where we
  // just track the number of contacts. We still maintain the non-T5R2 records
  // which are useful for tracking if contacts move a lot.
  // The following are for T5R2 mode:
  bool t5r2_;  // if set, use T5R2 hacks
  unsigned short t5r2_touched_size_;   // number of contacts that have arrived
  unsigned short t5r2_released_size_;  // number of contacts that have left
  // Whether all the fingers have age less than "Tap Maximum Finger Age".
  bool fingers_below_max_age_;
};

class TapToClickManager {
  FRIEND_TEST(ImmediateInterpreterTest, AmbiguousPalmCoScrollTest);
  FRIEND_TEST(ImmediateInterpreterTest, ChangeTimeoutTest);
  FRIEND_TEST(ImmediateInterpreterTest, ClickTest);
  FRIEND_TEST(ImmediateInterpreterTest, FlingDepthTest);
  FRIEND_TEST(ImmediateInterpreterTest, GetGesturingFingersTest);
  FRIEND_TEST(ImmediateInterpreterTest, GetGesturingFingersWithEmptyStateTest);
  FRIEND_TEST(ImmediateInterpreterTest, PalmAtEdgeTest);
  FRIEND_TEST(ImmediateInterpreterTest, PalmReevaluateTest);
  FRIEND_TEST(ImmediateInterpreterTest, PalmTest);
  FRIEND_TEST(ImmediateInterpreterTest, PinchTests);
  FRIEND_TEST(ImmediateInterpreterTest, PinchInterruptedByButtonDown);
  FRIEND_TEST(ImmediateInterpreterTest, ScrollResetTapTest);
  FRIEND_TEST(ImmediateInterpreterTest, ScrollThenFalseTapTest);
  FRIEND_TEST(ImmediateInterpreterTest, SemiMtActiveAreaTest);
  FRIEND_TEST(ImmediateInterpreterTest, SemiMtNoPinchTest);
  FRIEND_TEST(ImmediateInterpreterTest, StationaryPalmTest);
  FRIEND_TEST(ImmediateInterpreterTest, SwipeTest);
  FRIEND_TEST(ImmediateInterpreterTest, TapRecordTest);
  FRIEND_TEST(ImmediateInterpreterTest, TapToClickKeyboardTest);
  FRIEND_TEST(ImmediateInterpreterTest, TapToClickLowPressureBeginOrEndTest);
  FRIEND_TEST(ImmediateInterpreterTest, ThumbRetainReevaluateTest);
  FRIEND_TEST(ImmediateInterpreterTest, ThumbRetainTest);
  FRIEND_TEST(ImmediateInterpreterTest, WarpedFingersTappingTest);
  FRIEND_TEST(ImmediateInterpreterTest, ZeroClickInitializationTest);
  FRIEND_TEST(ImmediateInterpreterTtcEnableTest, TapToClickEnableTest);
  FRIEND_TEST(DragScrollTest, DragScrollDisabledDefaultsToMove);
  FRIEND_TEST(DragScrollTest, DragScrollEnabledProducesScroll);
  FRIEND_TEST(DragScrollTest, DragScrollTransitionsToFlingOnLift);
  FRIEND_TEST(DragScrollTest, DragScrollRevertsToMove);
  FRIEND_TEST(DragScrollTest, DragScrollWithThreeMovingFingers);
  FRIEND_TEST(DragScrollTest, DragScrollEnabledNormalDrag);
  FRIEND_TEST(DragScrollTest, DragScrollTwoFingersOnly);

  friend class AvoidAccidentalPinchTest;
  friend class TapToClickStateMachineTest;
  friend class TapRecord;
  friend class DragScrollTest;

 public:
  TapToClickManager(PropRegistry* prop_reg,
                    const BoolProperty& three_finger_click_enable,
                    const BoolProperty& t5r2_three_finger_click_enable,
                    const DoubleProperty& evaluation_timeout,
                    const DoubleProperty& tapping_finger_min_separation);
  ~TapToClickManager() = default;

  void Initialize(const HardwareProperties* hwprops, Metrics* metrics);

  [[nodiscard]] std::optional<Gesture> UpdateTapGesture(
      const HardwareState* hwstate,
      const HardwareStateBuffer& state_buffer,
      const std::set<short>& gs_fingers,
      bool same_fingers,
      bool phys_click_in_progress,
      bool keyboard_recently_used,
      bool prev_gesture_was_scroll,
      stime_t now,
      stime_t* timeout);

  void UpdateTapState(
      const HardwareState* hwstate,
      const HardwareStateBuffer& state_buffer,
      const std::set<short>& gs_fingers,
      bool same_fingers,
      bool phys_click_in_progress,
      bool keyboard_recently_used,
      bool prev_gesture_was_scroll,
      stime_t now,
      unsigned* buttons_down,
      unsigned* buttons_up,
      stime_t* timeout);

  void NoteMovement(stime_t timestamp);
  void ResetTime();

  bool IsTapInProgress() const {
    return state_ == TapToClickState::kFirstTapBegan ||
           state_ == TapToClickState::kSubsequentTapBegan;
  }
  bool IsDragging() const {
    return state_ == TapToClickState::kDrag;
  }
  TapToClickState state() const { return state_; }

  float tap_min_pressure() const { return tap_min_pressure_.val_; }
  stime_t tap_max_finger_age() const { return tap_max_finger_age_.val_; }
  bool device_reports_pressure() const {
    return hwprops_ ? hwprops_->reports_pressure : false;
  }
  stime_t finger_origin_timestamp(short tracking_id) const {
    return metrics_ ? metrics_->GetFinger(tracking_id)->origin_time() : 0.0;
  }

 private:
  stime_t TimeoutForTtcState(TapToClickState state) const;
  void SetTapToClickState(TapToClickState state, stime_t now);

  // Returns true iff the given finger is too close to any other finger to
  // realistically be doing a tap gesture.
  bool FingerTooCloseToTap(const HardwareState& hwstate,
                           const FingerState& fs) const;

  // The current state:
  TapToClickState state_;

  // When we entered the state:
  stime_t state_entered_time_;

  TapRecord tap_record_;

  // Record time when the finger showed motion (uses different motion detection
  // than last_movement_timestamp_).
  stime_t tap_drag_last_motion_time_;

  // True when the finger was stationary for a while during tap to drag.
  bool tap_drag_finger_was_stationary_;

  // Time when the last motion (scroll, movement) occurred.
  stime_t last_movement_timestamp_;

  // Fingers which are prohibited from ever tapping.
  std::set<short> tap_dead_fingers_;

  std::set<short> prev_tap_gs_fingers_;

  const HardwareProperties* hwprops_;
  Metrics* metrics_;

  // Shared properties owned by ImmediateInterpreter:
  const BoolProperty& three_finger_click_enable_;
  const BoolProperty& t5r2_three_finger_click_enable_;
  const DoubleProperty& evaluation_timeout_;
  const DoubleProperty& tapping_finger_min_separation_;

  // Properties

  // Is Tap-To-Click enabled?
  BoolProperty tap_enable_;
  // Allows Tap-To-Click to be paused.
  BoolProperty tap_paused_;
  // General time limit [s] for tap gestures.
  DoubleProperty tap_timeout_;
  // General time limit [s] for time between taps.
  DoubleProperty inter_tap_timeout_;
  // Time [s] before a tap gets recognized as a drag.
  DoubleProperty tap_drag_delay_;
  // Time [s] it takes to stop dragging when you let go of the touchpad.
  DoubleProperty tap_drag_timeout_;
  // True if tap dragging is enabled. With it disabled we can respond quickly
  // to tap clicks.
  BoolProperty tap_drag_enable_;
  // True if drag lock is enabled.
  BoolProperty drag_lock_enable_;
  // Time [s] the finger has to be stationary to be considered dragging.
  DoubleProperty tap_drag_stationary_time_;
  // Distance [mm] a finger can move and still register a tap.
  DoubleProperty tap_move_dist_;
  // Minimum pressure a finger must have for it to click when tap to click is
  // on.
  DoubleProperty tap_min_pressure_;
  // Maximum distance [mm] per frame that a finger can move and still be
  // considered stationary.
  DoubleProperty tap_max_movement_;
  // Maximum finger age for a finger to trigger tap.
  DoubleProperty tap_max_finger_age_;
  // Motion (pointer movement, scroll) must halt for this length of time [s]
  // before a tap can generate a click.
  DoubleProperty motion_tap_prevent_timeout_;
  // If a finger is recognized as thumb, it has only this much time to change
  // its status and perform a click.
  DoubleProperty thumb_click_prevention_timeout_;

  DISALLOW_COPY_AND_ASSIGN(TapToClickManager);
};

}  // namespace gestures

#endif  // GESTURES_TAP_TO_CLICK_MANAGER_H_
