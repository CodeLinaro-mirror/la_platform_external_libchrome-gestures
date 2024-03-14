// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "include/string_util.h"

#include <string>
#include <vector>

namespace gestures {

class StringUtilTest : public ::testing::Test {};

TEST(StringUtilTest, StringPrintfTest) {
  const char *pstr =
    "0123456789012345678901234567890123456789012345678901234567890123456789";
  std::string str = StringPrintf(
    " %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s ",
    pstr, pstr, pstr, pstr, pstr,
    pstr, pstr, pstr, pstr, pstr,
    pstr, pstr, pstr, pstr, pstr
  );
  int expected_length = (70*15)+15+1;
  EXPECT_EQ(str.size(), expected_length);
}

TEST(StringUtilTest, TrimWhitespaceASCIITest) {
  EXPECT_EQ(TrimWhitespaceASCII(""), "");
  EXPECT_EQ(TrimWhitespaceASCII(" x    "), "x");
  EXPECT_EQ(TrimWhitespaceASCII("badger"), "badger");
  EXPECT_EQ(TrimWhitespaceASCII("badger  "), "badger");
  EXPECT_EQ(TrimWhitespaceASCII("  badger"), "badger");
  EXPECT_EQ(TrimWhitespaceASCII("  \t \n\r "), "");
  EXPECT_EQ(TrimWhitespaceASCII("   Bees and ponies     "), "Bees and ponies");
}

}  // namespace gestures
