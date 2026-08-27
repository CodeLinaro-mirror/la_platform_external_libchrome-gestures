// Copyright 2026 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/tap_to_click_manager.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <optional>
#include <tuple>

#include "include/gestures.h"
#include "include/immediate_interpreter.h"
#include "include/logging.h"
#include "include/util.h"

using std::bind;
using std::for_each;

namespace gestures {

namespace {

const char* TapToClickStateName(TapToClickState state) {
  switch (state) {
    case TapToClickState::kIdle: return "Idle";
    case TapToClickState::kFirstTapBegan: return "FirstTapBegan";
    case TapToClickState::kTapComplete: return "TapComplete";
    case TapToClickState::kSubsequentTapBegan: return "SubsequentTapBegan";
    case TapToClickState::kDrag: return "Drag";
    case TapToClickState::kDragRelease: return "DragRelease";
    case TapToClickState::kDragRetouch: return "DragRetouch";
    default: return "<unknown>";
  }
}

} // namespace

TapRecord::TapRecord(const TapToClickManager* manager)
    : manager_(manager),
      t5r2_(false),
      t5r2_touched_size_(0),
      t5r2_released_size_(0),
      fingers_below_max_age_(true) {}

void TapRecord::NoteTouch(short the_id, const FingerState& fs) {
  // New finger must be close enough to an existing finger.
  if (!touched_.empty()) {
    bool reject_new_finger = true;
    for (const auto& [tracking_id, existing_fs] : touched_) {
      if (manager_->metrics_->CloseEnoughToGesture(
              Vector2(existing_fs),
              Vector2(fs))) {
        reject_new_finger = false;
        break;
      }
    }
    if (reject_new_finger)
      return;
  }
  touched_[the_id] = fs;
}

void TapRecord::NoteRelease(short the_id) {
  if (touched_.find(the_id) != touched_.end())
    released_.insert(the_id);
}

void TapRecord::Remove(short the_id) {
  min_tap_pressure_met_.erase(the_id);
  min_cotap_pressure_met_.erase(the_id);
  touched_.erase(the_id);
  released_.erase(the_id);
}

float TapRecord::CotapMinPressure() const {
  return manager_->tap_min_pressure() * 0.5;
}

void TapRecord::Update(const HardwareState& hwstate,
                       const HardwareState& prev_hwstate,
                       const std::set<short>& added,
                       const std::set<short>& removed,
                       const std::set<short>& dead) {
  if (!t5r2_ && (hwstate.touch_cnt > hwstate.finger_cnt ||
                 prev_hwstate.touch_cnt > prev_hwstate.finger_cnt)) {
    Log("TapRecord::Update: switching to T5R2 mode (%d > %d || %d > %d)",
        hwstate.touch_cnt, hwstate.finger_cnt, prev_hwstate.touch_cnt,
        prev_hwstate.finger_cnt);
    t5r2_ = true;
    t5r2_touched_size_ = touched_.size();
    t5r2_released_size_ = released_.size();
  }
  if (t5r2_) {
    short diff = static_cast<short>(hwstate.touch_cnt) -
        static_cast<short>(prev_hwstate.touch_cnt);
    if (diff > 0)
      t5r2_touched_size_ += diff;
    else if (diff < 0)
      t5r2_released_size_ += -diff;
  }
  for (short tracking_id : added) {
    Log("TapRecord::Update: Added: %d", tracking_id);
  }
  for (short tracking_id: removed) {
    Log("TapRecord::Update: Removed: %d", tracking_id);
  }
  for (short tracking_id : dead) {
    Log("TapRecord::Update: Dead: %d", tracking_id);
  }
  for_each(dead.begin(), dead.end(),
           bind(&TapRecord::Remove, this, std::placeholders::_1));
  for (short tracking_id : added) {
    NoteTouch(tracking_id, *hwstate.GetFingerState(tracking_id));
  }
  for_each(removed.begin(), removed.end(),
           bind(&TapRecord::NoteRelease, this, std::placeholders::_1));
  // Check if min tap/cotap pressure met yet.
  const float cotap_min_pressure = CotapMinPressure();
  for (auto& [tracking_id, existing_fs] : touched_) {
    const FingerState* fs = hwstate.GetFingerState(tracking_id);
    if (fs == nullptr)
      continue;
    if (fs->pressure >= manager_->tap_min_pressure() ||
        !manager_->device_reports_pressure())
      min_tap_pressure_met_.insert(fs->tracking_id);
    if (fs->pressure >= cotap_min_pressure ||
        !manager_->device_reports_pressure()) {
      min_cotap_pressure_met_.insert(fs->tracking_id);
      if (existing_fs.pressure < cotap_min_pressure &&
          manager_->device_reports_pressure()) {
        // Update existing record, since the old one hadn't met the cotap
        // pressure.
        existing_fs = *fs;
      }
    }
    stime_t finger_age = hwstate.timestamp -
        manager_->finger_origin_timestamp(fs->tracking_id);
    if (finger_age > manager_->tap_max_finger_age())
      fingers_below_max_age_ = false;
  }
}

void TapRecord::Clear() {
  min_tap_pressure_met_.clear();
  min_cotap_pressure_met_.clear();
  t5r2_ = false;
  t5r2_touched_size_ = 0;
  t5r2_released_size_ = 0;
  fingers_below_max_age_ = true;
  touched_.clear();
  released_.clear();
}

bool TapRecord::Moving(const HardwareState& hwstate,
                       const float dist_max) const {
  const float cotap_min_pressure = CotapMinPressure();
  for (const auto& [tracking_id, existing_fs] : touched_) {
    const FingerState* fs = hwstate.GetFingerState(tracking_id);
    if (!fs)
      continue;
    // Only look for moving when current frame meets cotap pressure and
    // our history contains a contact that's met cotap pressure.
    if ((fs->pressure < cotap_min_pressure ||
        existing_fs.pressure < cotap_min_pressure) &&
        manager_->device_reports_pressure())
      continue;
    // Compute distance moved.
    float dist_x = fs->position_x - existing_fs.position_x;
    float dist_y = fs->position_y - existing_fs.position_y;
    // Respect WARP flags.
    if (fs->flags & GESTURES_FINGER_WARP_X_TAP_MOVE)
      dist_x = 0.0;
    if (fs->flags & GESTURES_FINGER_WARP_Y_TAP_MOVE)
      dist_y = 0.0;

    bool moving =
        dist_x * dist_x + dist_y * dist_y > dist_max * dist_max;
    if (moving)
      return true;
  }
  return false;
}

bool TapRecord::Motionless(const HardwareState& hwstate,
                           const HardwareState& prev_hwstate,
                           const float max_speed) const {
  const float cotap_min_pressure = CotapMinPressure();
  for (const auto& [tracking_id, _] : touched_) {
    const FingerState* fs = hwstate.GetFingerState(tracking_id);
    const FingerState* prev_fs = prev_hwstate.GetFingerState(tracking_id);
    if (!fs || !prev_fs)
      continue;
    // Only look for moving when current frame meets cotap pressure and
    // our history contains a contact that's met cotap pressure.
    if ((fs->pressure < cotap_min_pressure ||
        prev_fs->pressure < cotap_min_pressure) &&
        manager_->device_reports_pressure())
      continue;
    // Compute distance moved.
    if (DistSq(*fs, *prev_fs) > max_speed * max_speed)
      return false;
  }
  return true;
}

bool TapRecord::TapBegan() const {
  if (t5r2_)
    return t5r2_touched_size_ > 0;
  return !touched_.empty();
}

bool TapRecord::TapComplete() const {
  bool ret = false;
  if (t5r2_)
    ret = t5r2_touched_size_ && t5r2_touched_size_ == t5r2_released_size_;
  else
    ret = !touched_.empty() && (touched_.size() == released_.size());
  for (const auto& [tracking_id, finger_state] : touched_) {
    Log("TapRecord::TapComplete: touched_: %d", tracking_id);
  }
  for (short tracking_id : released_) {
    Log("TapRecord::TapComplete: released_: %d", tracking_id);
  }
  return ret;
}

bool TapRecord::MinTapPressureMet() const {
  return t5r2_ || !min_tap_pressure_met_.empty();
}

bool TapRecord::FingersBelowMaxAge() const {
  return fingers_below_max_age_;
}

int TapRecord::TapType() const {
  size_t touched_size =
      t5r2_ ? t5r2_touched_size_ : min_cotap_pressure_met_.size();
  int ret = GESTURES_BUTTON_LEFT;
  if (touched_size > 1)
    ret = GESTURES_BUTTON_RIGHT;
  if (touched_size == 3 &&
      manager_->three_finger_click_enable_.val_ &&
      (!t5r2_ || manager_->t5r2_three_finger_click_enable_.val_))
    ret = GESTURES_BUTTON_MIDDLE;
  return ret;
}

TapToClickManager::TapToClickManager(
    PropRegistry* prop_reg,
    const BoolProperty& three_finger_click_enable,
    const BoolProperty& t5r2_three_finger_click_enable,
    const DoubleProperty& evaluation_timeout,
    const DoubleProperty& tapping_finger_min_separation)
    : state_(TapToClickState::kIdle),
      state_entered_time_(-1.0),
      tap_record_(this),
      tap_drag_last_motion_time_(-1.0),
      tap_drag_finger_was_stationary_(false),
      last_movement_timestamp_(-1.0),
      hwprops_(nullptr),
      metrics_(nullptr),
      three_finger_click_enable_(three_finger_click_enable),
      t5r2_three_finger_click_enable_(t5r2_three_finger_click_enable),
      evaluation_timeout_(evaluation_timeout),
      tapping_finger_min_separation_(tapping_finger_min_separation),
      tap_enable_(prop_reg, "Tap Enable", true),
      tap_paused_(prop_reg, "Tap Paused", false),
      tap_timeout_(prop_reg, "Tap Timeout", 0.2),
      inter_tap_timeout_(prop_reg, "Inter-Tap Timeout", 0.15),
      tap_drag_delay_(prop_reg, "Tap Drag Delay", 0),
      tap_drag_timeout_(prop_reg, "Tap Drag Timeout", 0.3),
      tap_drag_enable_(prop_reg, "Tap Drag Enable", false),
      drag_lock_enable_(prop_reg, "Tap Drag Lock Enable", false),
      tap_drag_stationary_time_(prop_reg, "Tap Drag Stationary Time", 0),
      tap_move_dist_(prop_reg, "Tap Move Distance", 2.0),
      tap_min_pressure_(prop_reg, "Tap Minimum Pressure", 25.0),
      tap_max_movement_(prop_reg, "Tap Maximum Movement", 0.0001),
      tap_max_finger_age_(prop_reg, "Tap Maximum Finger Age", 1.2),
      motion_tap_prevent_timeout_(prop_reg, "Motion Tap Prevent Timeout", 0.05),
      thumb_click_prevention_timeout_(prop_reg,
                                      "Thumb Click Prevention Timeout",
                                      0.15) {}

void TapToClickManager::Initialize(const HardwareProperties* hwprops,
                                  Metrics* metrics) {
  hwprops_ = hwprops;
  metrics_ = metrics;
}

void TapToClickManager::NoteMovement(stime_t timestamp) {
  last_movement_timestamp_ = timestamp;
}

void TapToClickManager::ResetTime() {
  state_entered_time_ = -1.0;
  last_movement_timestamp_ = -1.0;
}

stime_t TapToClickManager::TimeoutForTtcState(TapToClickState state) const {
  switch (state) {
    case TapToClickState::kIdle: return tap_timeout_.val_;
    case TapToClickState::kFirstTapBegan: return tap_timeout_.val_;
    case TapToClickState::kTapComplete: return inter_tap_timeout_.val_;
    case TapToClickState::kSubsequentTapBegan: return tap_timeout_.val_;
    case TapToClickState::kDrag: return tap_timeout_.val_;
    case TapToClickState::kDragRelease: return tap_drag_timeout_.val_;
    case TapToClickState::kDragRetouch: return tap_timeout_.val_;
    default:
      Err("Unknown TapToClickState %u!", static_cast<unsigned>(state));
      return 0.0;
  }
}

void TapToClickManager::SetTapToClickState(TapToClickState state, stime_t now) {
  if (state_ != state) {
    state_ = state;
    state_entered_time_ = now;
  }
}

std::optional<Gesture> TapToClickManager::UpdateTapGesture(
    const HardwareState* hwstate,
    const HardwareStateBuffer& state_buffer,
    const std::set<short>& gs_fingers,
    bool same_fingers,
    bool phys_click_in_progress,
    bool keyboard_recently_used,
    bool prev_gesture_was_scroll,
    stime_t now,
    stime_t* timeout) {
  unsigned down = 0;
  unsigned up = 0;
  UpdateTapState(hwstate, state_buffer, gs_fingers, same_fingers,
                 phys_click_in_progress, keyboard_recently_used,
                 prev_gesture_was_scroll, now, &down, &up, timeout);
  if (down == 0 && up == 0) {
    return std::nullopt;
  }
  Log("UpdateTapGesture: Tap Generated");
  return Gesture(kGestureButtonsChange, state_buffer.Get(1).timestamp, now,
                 down, up, /*is_tap=*/true);
}

void TapToClickManager::UpdateTapState(
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
    stime_t* timeout) {
  if (state_ == TapToClickState::kIdle &&
      (!tap_enable_.val_ || tap_paused_.val_))
    return;

  std::set<short> tap_gs_fingers;

  if (hwstate)
    RemoveMissingIdsFromSet(&tap_dead_fingers_, *hwstate);

  bool cancel_tapping = false;
  if (hwstate) {
    for (int i = 0; i < hwstate->finger_cnt; ++i) {
      if (hwstate->fingers[i].flags &
          (GESTURES_FINGER_NO_TAP | GESTURES_FINGER_MERGE))
        cancel_tapping = true;
    }
    for (short tracking_id : gs_fingers) {
      const FingerState* fs = hwstate->GetFingerState(tracking_id);
      if (!fs) {
        Err("Missing finger state?!");
        continue;
      }
      tap_gs_fingers.insert(tracking_id);
    }
  }
  std::set<short> added_fingers;

  // Fingers removed from the pad entirely.
  std::set<short> removed_fingers;

  // Fingers that were gesturing, but now aren't.
  std::set<short> dead_fingers;

  bool is_timeout = (now - state_entered_time_ > TimeoutForTtcState(state_));

  if (phys_click_in_progress) {
    // Don't allow any current fingers to tap ever.
    for (size_t i = 0; i < hwstate->finger_cnt; i++)
      tap_dead_fingers_.insert(hwstate->fingers[i].tracking_id);
  }

  if (hwstate && (!same_fingers || prev_tap_gs_fingers_ != tap_gs_fingers)) {
    // See if fingers were added.
    for (short tracking_id : tap_gs_fingers) {
      // If the finger was marked as a thumb before, it is not new.
      if (hwstate->timestamp - finger_origin_timestamp(tracking_id) >
               thumb_click_prevention_timeout_.val_)
        continue;

      if (!SetContainsValue(prev_tap_gs_fingers_, tracking_id)) {
        // Gesturing finger wasn't in prev state. It's new.
        const FingerState* fs = hwstate->GetFingerState(tracking_id);
        if (FingerTooCloseToTap(*hwstate, *fs) ||
            FingerTooCloseToTap(state_buffer.Get(1), *fs) ||
            SetContainsValue(tap_dead_fingers_, fs->tracking_id))
          continue;
        added_fingers.insert(tracking_id);
        Log("TTC: Added %d", tracking_id);
      }
    }

    // See if fingers were removed or are now non-gesturing (dead).
    for (short tracking_id : prev_tap_gs_fingers_) {
      if (tap_gs_fingers.find(tracking_id) != tap_gs_fingers.end())
        // Still gesturing; neither removed nor dead.
        continue;
      if (!hwstate->GetFingerState(tracking_id)) {
        // Previously gesturing finger isn't in current state. It's gone.
        removed_fingers.insert(tracking_id);
        Log("TTC: Removed %d", tracking_id);
      } else {
        // Previously gesturing finger is in current state. It's dead.
        dead_fingers.insert(tracking_id);
        Log("TTC: Dead %d", tracking_id);
      }
    }
  }

  prev_tap_gs_fingers_ = tap_gs_fingers;

  // The state machine:

  // If you are updating the code, keep this diagram correct.
  // We have a TapRecord which stores current tap state.
  // Also, if the physical button is down or previous gesture type is scroll,
  // we go to (or stay in) Idle state.

  //     Start
  //       ↓
  //    [Idle**] <----------------------------------------------------------,
  //       ↓ added finger(s)                                                ^
  //  ,>[FirstTapBegan] -<right click: send right click, timeout/movement>->|
  //  |    ↓ released all fingers                                           |
  // ,->[TapComplete*] --<timeout: send click>----------------------------->|
  // ||    | | two finger touching: send left click.                        |
  // |'<---+-'                                                              ^
  // |     ↓ add finger(s)                                                  |
  // ^  [SubsequentTapBegan] --<timeout/move w/o delay: send click>-------->|
  // |     | | | release all fingers: send left click                       |
  // |<----+-+-'                                                            ^
  // |     | `-> start non-left click: send left click; goto FirstTapBegan  |
  // |     ↓ timeout/movement with delay: send button down                  |
  // | ,->[Drag] --<detect 2 finger gesture: send button up>--------------->|
  // | |   ↓ release all fingers                                            ^
  // | |  [DragRelease*]  --<timeout: send button up>---------------------->|
  // ^ ^   ↓ add finger(s)                                                  ^
  // | |  [DragRetouch]  --<remove fingers (left tap): send button up>----->'
  // | |   | | timeout/movement
  // | '-<-+-'
  // |     |  remove all fingers (non-left tap): send button up
  // '<----'
  //
  // * When entering TapComplete or DragRelease, we set a timer, since
  //   we will have no fingers on the pad and want to run possibly before
  //   fingers are put on the pad. Note that we use different timeouts
  //   based on which state we're in (tap_timeout_ or tap_drag_timeout_).
  // ** When entering idle, we reset the TapRecord.

  if (state_ != TapToClickState::kIdle)
    Log("TTC State: %s", TapToClickStateName(state_));
  if (!hwstate)
    Log("TTC: This is a timer callback");
  if (phys_click_in_progress || keyboard_recently_used ||
      prev_gesture_was_scroll || cancel_tapping) {
    Log("TTC: Forced to idle");
    SetTapToClickState(TapToClickState::kIdle, now);
    return;
  }

  switch (state_) {
    case TapToClickState::kIdle:
      tap_record_.Clear();
      if (hwstate &&
          hwstate->timestamp - last_movement_timestamp_ >=
          motion_tap_prevent_timeout_.val_) {
        tap_record_.Update(
            *hwstate, state_buffer.Get(1), added_fingers, removed_fingers,
            dead_fingers);
        if (tap_record_.TapBegan())
          SetTapToClickState(TapToClickState::kFirstTapBegan, now);
      }
      break;
    case TapToClickState::kFirstTapBegan:
      if (is_timeout) {
        SetTapToClickState(TapToClickState::kIdle, now);
        break;
      }
      if (!hwstate) {
        Err("hwstate is null but not a timeout?!");
        break;
      }
      tap_record_.Update(
          *hwstate, state_buffer.Get(1), added_fingers,
          removed_fingers, dead_fingers);
      Log("TTC: Is tap? %d Is moving? %d",
          tap_record_.TapComplete(),
          tap_record_.Moving(*hwstate, tap_move_dist_.val_));
      if (tap_record_.TapComplete()) {
        if (!tap_record_.MinTapPressureMet() ||
            !tap_record_.FingersBelowMaxAge()) {
          SetTapToClickState(TapToClickState::kIdle, now);
        } else if (tap_record_.TapType() == GESTURES_BUTTON_LEFT &&
                   tap_drag_enable_.val_) {
          SetTapToClickState(TapToClickState::kTapComplete, now);
        } else {
          *buttons_down = *buttons_up = tap_record_.TapType();
          SetTapToClickState(TapToClickState::kIdle, now);
        }
      } else if (tap_record_.Moving(*hwstate, tap_move_dist_.val_)) {
        SetTapToClickState(TapToClickState::kIdle, now);
      }
      break;
    case TapToClickState::kTapComplete:
      if (!added_fingers.empty()) {
        tap_record_.Clear();
        tap_record_.Update(
            *hwstate, state_buffer.Get(1), added_fingers, removed_fingers,
            dead_fingers);

        // If more than one finger is touching: Send click
        // and return to FirstTapBegan state.
        if (tap_record_.TapType() != GESTURES_BUTTON_LEFT) {
          *buttons_down = *buttons_up = GESTURES_BUTTON_LEFT;
          SetTapToClickState(TapToClickState::kFirstTapBegan, now);
        } else {
          tap_drag_last_motion_time_ = now;
          tap_drag_finger_was_stationary_ = false;
          SetTapToClickState(TapToClickState::kSubsequentTapBegan, now);
        }
      } else if (is_timeout) {
        *buttons_down = *buttons_up =
            tap_record_.MinTapPressureMet() ? tap_record_.TapType() : 0;
        SetTapToClickState(TapToClickState::kIdle, now);
      }
      break;
    case TapToClickState::kSubsequentTapBegan:
      if (!is_timeout && !hwstate) {
        Err("hwstate is null but not a timeout?!");
        break;
      }
      if (hwstate)
        tap_record_.Update(*hwstate, state_buffer.Get(1), added_fingers,
                           removed_fingers, dead_fingers);

      if (!tap_record_.Motionless(*hwstate, state_buffer.Get(1),
                                  tap_max_movement_.val_)) {
        tap_drag_last_motion_time_ = now;
      }
      if (tap_record_.TapType() == GESTURES_BUTTON_LEFT &&
          now - tap_drag_last_motion_time_ >= tap_drag_stationary_time_.val_) {
        tap_drag_finger_was_stationary_ = true;
      }

      if (is_timeout || tap_record_.Moving(*hwstate, tap_move_dist_.val_)) {
        if (tap_record_.TapType() == GESTURES_BUTTON_LEFT) {
          if (is_timeout) {
            // moving with just one finger. Start dragging.
            *buttons_down = GESTURES_BUTTON_LEFT;
            SetTapToClickState(TapToClickState::kDrag, now);
          } else {
            bool drag_delay_met =
                (now - state_entered_time_ >= tap_drag_delay_.val_);
            if (drag_delay_met && tap_drag_finger_was_stationary_) {
              *buttons_down = GESTURES_BUTTON_LEFT;
              SetTapToClickState(TapToClickState::kDrag, now);
            } else {
              *buttons_down = GESTURES_BUTTON_LEFT;
              *buttons_up = GESTURES_BUTTON_LEFT;
              SetTapToClickState(TapToClickState::kIdle, now);
            }
          }
        } else if (!tap_record_.TapComplete()) {
          // not just one finger. Send button click and go to idle.
          *buttons_down = *buttons_up = GESTURES_BUTTON_LEFT;
          SetTapToClickState(TapToClickState::kIdle, now);
        }
        break;
      }
      if (tap_record_.TapType() != GESTURES_BUTTON_LEFT) {
        // We aren't going to drag, so send left click now and handle current
        // tap afterwards.
        *buttons_down = *buttons_up = GESTURES_BUTTON_LEFT;
        SetTapToClickState(TapToClickState::kFirstTapBegan, now);
      }
      if (tap_record_.TapComplete()) {
        *buttons_down = *buttons_up = GESTURES_BUTTON_LEFT;
        SetTapToClickState(TapToClickState::kTapComplete, now);
        Log("TTC: Subsequent left tap complete");
      }
      break;
    case TapToClickState::kDrag:
      if (hwstate)
        tap_record_.Update(
            *hwstate, state_buffer.Get(1), added_fingers, removed_fingers,
            dead_fingers);
      if (tap_record_.TapComplete()) {
        tap_record_.Clear();
        if (drag_lock_enable_.val_) {
          SetTapToClickState(TapToClickState::kDragRelease, now);
        } else {
          *buttons_up = GESTURES_BUTTON_LEFT;
          SetTapToClickState(TapToClickState::kIdle, now);
        }
      }
      if (tap_record_.TapType() != GESTURES_BUTTON_LEFT &&
          now - state_entered_time_ <= evaluation_timeout_.val_) {
        // We thought we were dragging, but actually we're doing a
        // non-tap-to-click multitouch gesture.
        *buttons_up = GESTURES_BUTTON_LEFT;
        SetTapToClickState(TapToClickState::kIdle, now);
      }
      break;
    case TapToClickState::kDragRelease:
      if (!added_fingers.empty()) {
        tap_record_.Update(
            *hwstate, state_buffer.Get(1), added_fingers, removed_fingers,
            dead_fingers);
        SetTapToClickState(TapToClickState::kDragRetouch, now);
      } else if (is_timeout) {
        *buttons_up = GESTURES_BUTTON_LEFT;
        SetTapToClickState(TapToClickState::kIdle, now);
      }
      break;
    case TapToClickState::kDragRetouch:
      if (hwstate)
        tap_record_.Update(
            *hwstate, state_buffer.Get(1), added_fingers, removed_fingers,
            dead_fingers);
      if (tap_record_.TapComplete()) {
        *buttons_up = GESTURES_BUTTON_LEFT;
        if (tap_record_.TapType() == GESTURES_BUTTON_LEFT)
          SetTapToClickState(TapToClickState::kIdle, now);
        else
          SetTapToClickState(TapToClickState::kTapComplete, now);
        break;
      }
      if (is_timeout) {
        SetTapToClickState(TapToClickState::kDrag, now);
        break;
      }
      if (!hwstate) {
        Err("hwstate is null but not a timeout?!");
        break;
      }
      if (tap_record_.Moving(*hwstate, tap_move_dist_.val_))
        SetTapToClickState(TapToClickState::kDrag, now);
      break;
  }
  if (state_ != TapToClickState::kIdle)
    Log("TTC: New state: %s", TapToClickStateName(state_));
  // Take action based on new state:
  switch (state_) {
    case TapToClickState::kTapComplete:
      *timeout = TimeoutForTtcState(state_);
      break;
    case TapToClickState::kDragRelease:
      *timeout = TimeoutForTtcState(state_);
      break;
    default:  // so gcc doesn't complain about missing enums
      break;
  }
}

bool TapToClickManager::FingerTooCloseToTap(const HardwareState& hwstate,
                                           const FingerState& fs) const {
  const float kMinAllowableSq =
      tapping_finger_min_separation_.val_ * tapping_finger_min_separation_.val_;
  for (size_t i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState* iter_fs = &hwstate.fingers[i];
    if (iter_fs->tracking_id == fs.tracking_id)
      continue;
    float dist_sq = DistSq(fs, *iter_fs);
    if (dist_sq < kMinAllowableSq)
      return true;
  }
  return false;
}

}  // namespace gestures
