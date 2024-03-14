// Copyright 2014 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_STRING_UTIL_H_
#define GESTURES_STRING_UTIL_H_

#include <string>
#include <vector>

#include "include/compiler_specific.h"

namespace gestures {

// Return a C++ string given printf-like input.
std::string StringPrintf(const char* format, ...)
      PRINTF_FORMAT(1, 2);

// Lower-level routine that takes a va_list and appends to a specified
// string.  All other routines are just convenience wrappers around it.
void StringAppendV(std::string* dst, const char* format, va_list ap)
    PRINTF_FORMAT(2, 0);

// Trims whitespace from the start and end of the input string.  This function
// is for ASCII strings and only looks for ASCII whitespace.
std::string TrimWhitespaceASCII(const std::string& input);

}  // namespace gestures

#endif  // GESTURES_STRING_UTIL_H_
