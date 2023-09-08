// Copyright 2011 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <gtest/gtest.h>

#include "include/activity_log.h"
#include "include/macros.h"
#include "include/prop_registry.h"
#include "include/unittest_util.h"

using std::string;

namespace gestures {

class ActivityLogTest : public ::testing::Test {};

TEST(ActivityLogTest, SimpleTest) {
  PropRegistry prop_reg;
  BoolProperty true_prop(&prop_reg, "true prop", true);
  BoolProperty false_prop(&prop_reg, "false prop", false);
  DoubleProperty double_prop(&prop_reg, "double prop", 77.25);
  IntProperty int_prop(&prop_reg, "int prop", -816);
  StringProperty string_prop(&prop_reg, "string prop", "foobarstr");

  ActivityLog log(&prop_reg);
  EXPECT_TRUE(strstr(log.Encode().c_str(), "true"));
  EXPECT_TRUE(strstr(log.Encode().c_str(), "false"));
  EXPECT_TRUE(strstr(log.Encode().c_str(), "77.25"));
  EXPECT_TRUE(strstr(log.Encode().c_str(), "-816"));
  EXPECT_TRUE(strstr(log.Encode().c_str(), "foobarstr"));

  HardwareProperties hwprops = {
    6011,  // left edge
    6012,  // top edge
    6013,  // right edge
    6014,  // bottom edge
    6015,  // x pixels/TP width
    6016,  // y pixels/TP height
    6017,  // x screen DPI
    6018,  // y screen DPI
    6019,  // orientation minimum
    6020,  // orientation maximum
    6021,  // max fingers
    6022,  // max touch
    1,  // t5r2
    0,  // semi-mt
    1,  // is button pad,
    0,  // has wheel
    0,  // vertical wheel is high resolution
    0,  // is haptic pad
  };

  log.SetHardwareProperties(hwprops);

  const char* expected_strings[] = {
    "6011", "6012", "6013", "6014", "6015", "6016",
    "6017", "6018", "6019", "6020", "6021", "6022"
  };
  string hwprops_log = log.Encode();
  for (size_t i = 0; i < arraysize(expected_strings); i++)
    EXPECT_TRUE(strstr(hwprops_log.c_str(), expected_strings[i]));

  EXPECT_EQ(0, log.size());
  EXPECT_GT(log.MaxSize(), 10);

  FingerState fs = { 0.0, 0.0, 0.0, 0.0, 9.0, 0.0, 3.0, 4.0, 22, 0 };
  HardwareState hs = make_hwstate(1.0, 0, 1, 1, &fs);
  log.LogHardwareState(hs);
  EXPECT_EQ(1, log.size());
  EXPECT_TRUE(strstr(log.Encode().c_str(), "22"));
  ActivityLog::Entry* entry = log.GetEntry(0);
  EXPECT_TRUE(std::holds_alternative<HardwareState>(entry->details));

  log.LogTimerCallback(234.5);
  EXPECT_EQ(2, log.size());
  EXPECT_TRUE(strstr(log.Encode().c_str(), "234.5"));
  entry = log.GetEntry(1);
  EXPECT_TRUE(std::holds_alternative<ActivityLog::TimerCallbackEntry>
                (entry->details));

  log.LogCallbackRequest(90210);
  EXPECT_EQ(3, log.size());
  EXPECT_TRUE(strstr(log.Encode().c_str(), "90210"));
  entry = log.GetEntry(2);
  EXPECT_TRUE(std::holds_alternative<ActivityLog::CallbackRequestEntry>
                (entry->details));

  Gesture null;
  Gesture move(kGestureMove, 1.0, 2.0, 773, 4.0);
  Gesture scroll(kGestureScroll, 1.0, 2.0, 312, 4.0);
  Gesture buttons(kGestureButtonsChange, 1.0, 847, 3, 4, false);
  Gesture contact_initiated;
  contact_initiated.type = kGestureTypeContactInitiated;
  Gesture mousewheel(kGestureMouseWheel, 1.0, 2.0, 30.0, 40.0, 3, 4);
  Gesture pinch(kGesturePinch, 1.0, 2.0, 3.0, 4.0);
  Gesture fling(kGestureFling, 1.0, 2.0, 42.0, 24.0, 1);
  Gesture swipe(kGestureSwipe, 1.0, 2.0, 128.0, 4.0);
  Gesture swipelift(kGestureSwipeLift, 1.0, 2.0);
  Gesture swipe4f(kGestureFourFingerSwipe, 1.0, 2.0, 256.0, 4.0);
  Gesture swipe4flift(kGestureFourFingerSwipeLift, 1.0, 2.0);
  Gesture metrics(kGestureMetrics, 1.0, 2.0,
                  kGestureMetricsTypeMouseMovement, 3.0, 4.0);

  Gesture* gs[] = {
    &null, &move, &scroll, &buttons, &contact_initiated,
    &mousewheel, &pinch, &fling, &swipe, &swipelift,
    &swipe4f, &swipe4flift, &metrics
  };
  const char* test_strs[] = {
    "null", "773", "312", "847", "nitiated",
    "30", "3", "42", "128", "null",
    "256", "null", "1"
  };

  ASSERT_EQ(arraysize(gs), arraysize(test_strs));
  for (size_t i = 0; i < arraysize(gs); ++i) {
    log.LogGesture(*gs[i]);
    EXPECT_TRUE(strstr(log.Encode().c_str(), test_strs[i]))
      << "i=" << i;
    entry = log.GetEntry(log.size() - 1);
    EXPECT_TRUE(std::holds_alternative<Gesture>(entry->details))
      << "i=" << i;
  }

  log.Clear();
  EXPECT_EQ(0, log.size());
}

TEST(ActivityLogTest, WrapAroundTest) {
  ActivityLog log(nullptr);
  // overfill the buffer
  const size_t fill_size = (ActivityLog::kBufferSize * 3) / 2;
  for (size_t i = 0; i < fill_size; i++)
    log.LogCallbackRequest(static_cast<stime_t>(i));
  const string::size_type prefix_length = 100;
  string first_prefix = log.Encode().substr(0, prefix_length);
  log.LogCallbackRequest(static_cast<stime_t>(fill_size));
  string second_prefix = log.Encode().substr(0, prefix_length);
  EXPECT_NE(first_prefix, second_prefix);
}

TEST(ActivityLogTest, VersionTest) {
  ActivityLog log(nullptr);
  string thelog = log.Encode();
  EXPECT_TRUE(thelog.find(VCSID) != string::npos);
}

TEST(ActivityLogTest, EncodePropChangeBoolTest) {
  ActivityLog log(nullptr);
  Json::Value ret;

  ActivityLog::PropChangeEntry bool_prop;
  bool_prop.name = "boolean";
  bool_prop.value = static_cast<GesturesPropBool>(true);
  ret = log.EncodePropChange(bool_prop);
  EXPECT_EQ(ret[ActivityLog::kKeyType],
            Json::Value(ActivityLog::kKeyPropChange));
  EXPECT_EQ(ret[ActivityLog::kKeyPropChangeName],
            Json::Value(bool_prop.name));
  EXPECT_EQ(static_cast<GesturesPropBool>(
            ret[ActivityLog::kKeyPropChangeValue].asBool()),
            std::get<GesturesPropBool>(bool_prop.value));
  EXPECT_EQ(ret[ActivityLog::kKeyPropChangeType],
            ActivityLog::kValuePropChangeTypeBool);
}

TEST(ActivityLogTest, EncodePropChangeDoubleTest) {
  ActivityLog log(nullptr);
  Json::Value ret;

  ActivityLog::PropChangeEntry double_prop;
  double_prop.name = "double";
  double_prop.value = 42.0;
  ret = log.EncodePropChange(double_prop);
  EXPECT_EQ(ret[ActivityLog::kKeyType],
            Json::Value(ActivityLog::kKeyPropChange));
  EXPECT_EQ(ret[ActivityLog::kKeyPropChangeName],
            Json::Value(double_prop.name));
  EXPECT_EQ(ret[ActivityLog::kKeyPropChangeValue].asDouble(),
            std::get<double>(double_prop.value));
  EXPECT_EQ(ret[ActivityLog::kKeyPropChangeType],
            ActivityLog::kValuePropChangeTypeDouble);
}

TEST(ActivityLogTest, EncodePropChangeIntTest) {
  ActivityLog log(nullptr);
  Json::Value ret;

  ActivityLog::PropChangeEntry int_prop;
  int_prop.name = "int";
  int_prop.value = 42;
  ret = log.EncodePropChange(int_prop);
  EXPECT_EQ(ret[ActivityLog::kKeyType],
            Json::Value(ActivityLog::kKeyPropChange));
  EXPECT_EQ(ret[ActivityLog::kKeyPropChangeName],
            Json::Value(int_prop.name));
  EXPECT_EQ(ret[ActivityLog::kKeyPropChangeValue].asInt(),
            std::get<int>(int_prop.value));
  EXPECT_EQ(ret[ActivityLog::kKeyPropChangeType],
            ActivityLog::kValuePropChangeTypeInt);
}

TEST(ActivityLogTest, EncodePropChangeShortTest) {
  ActivityLog log(nullptr);
  Json::Value ret;

  ActivityLog::PropChangeEntry short_prop;
  short_prop.name = "short";
  short_prop.value = static_cast<short>(42);
  ret = log.EncodePropChange(short_prop);
  EXPECT_EQ(ret[ActivityLog::kKeyType],
            Json::Value(ActivityLog::kKeyPropChange));
  EXPECT_EQ(ret[ActivityLog::kKeyPropChangeName],
            Json::Value(short_prop.name));
  EXPECT_EQ(static_cast<short>(
            ret[ActivityLog::kKeyPropChangeValue].asInt()),
            std::get<short>(short_prop.value));
  EXPECT_EQ(ret[ActivityLog::kKeyPropChangeType],
            ActivityLog::kValuePropChangeTypeShort);
}

}  // namespace gestures
