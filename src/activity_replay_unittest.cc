// Copyright 2012 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "include/activity_replay.h"
#include "include/command_line.h"
#include "include/file_util.h"
#include "include/finger_metrics.h"
#include "include/gestures.h"
#include "include/logging_filter_interpreter.h"
#include "include/string_util.h"

using std::string;

namespace gestures {

namespace {

template <typename STR>
void SplitStringT(const STR& str,
                  const typename STR::value_type s,
                  bool trim_whitespace,
                  std::vector<STR>* r) {
  r->clear();
  size_t last = 0;
  size_t c = str.size();
  for (size_t i = 0; i <= c; ++i) {
    if (i == c || str[i] == s) {
      STR tmp(str, last, i - last);
      if (trim_whitespace)
        tmp = TrimWhitespaceASCII(tmp);
      // Avoid converting an empty or all-whitespace source string into a vector
      // of one empty string.
      if (i != c || !r->empty() || !tmp.empty())
        r->push_back(tmp);
      last = i + 1;
    }
  }
}

// |str| should not be in a multi-byte encoding like Shift-JIS or GBK in which
// the trailing byte of a multi-byte character can be in the ASCII range.
// UTF-8, and other single/multi-byte ASCII-compatible encodings are OK.
// Note: |c| must be in the ASCII range.
void SplitString(const std::string& str,
                 char c,
                 std::vector<std::string>* r) {
  SplitStringT(str, c, true, r);
}

}  // namespace

class ActivityReplayTest : public ::testing::Test {};

// This test reads a log file and replays it. This test should be enabled for a
// hands-on debugging session.

TEST(ActivityReplayTest, DISABLED_SimpleTest) {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  GestureInterpreter* c_interpreter = NewGestureInterpreter();
  c_interpreter->Initialize();

  Interpreter* interpreter = c_interpreter->interpreter();
  PropRegistry* prop_reg = c_interpreter->prop_reg();
  {
    MetricsProperties mprops(prop_reg);

    string log_contents;
    ASSERT_TRUE(ReadFileToString(cl->GetSwitchValueASCII("in").c_str(),
                                 &log_contents));

    ActivityReplay replay(prop_reg);
    std::vector<string> honor_props;
    if (cl->GetSwitchValueASCII("only_honor")[0])
      SplitString(cl->GetSwitchValueASCII("only_honor"),
                        ',', &honor_props);
    std::set<string> honor_props_set(honor_props.begin(), honor_props.end());
    replay.Parse(log_contents, honor_props_set);
    replay.Replay(interpreter, &mprops);

    // Dump the new log
    const string kOutSwitchName = "outfile";
    if (cl->HasSwitch(kOutSwitchName))
      static_cast<LoggingFilterInterpreter*>(interpreter)->Dump(
          cl->GetSwitchValueASCII(kOutSwitchName).c_str());
  }

  DeleteGestureInterpreter(c_interpreter);
}

}  // namespace gestures
