// Copyright 2014 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/string_util.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

#include "include/macros.h"

namespace gestures {

namespace {

const char kWhitespaceASCII[] = {
  0x09,    // CHARACTER TABULATION
  0x0A,    // LINE FEED (LF)
  0x0B,    // LINE TABULATION
  0x0C,    // FORM FEED (FF)
  0x0D,    // CARRIAGE RETURN (CR)
  0x20,    // SPACE
  0
};

// Backend for StringPrintF/StringAppendF. This does not finalize
// the va_list, the caller is expected to do that.
PRINTF_FORMAT(2, 0)
static void StringAppendVT(std::string* dst,
                           const char* format,
                           va_list ap) {
  // First try with a small fixed size buffer.
  // This buffer size should be kept in sync with StringUtilTest.GrowBoundary
  // and StringUtilTest.StringPrintfBounds.
  char stack_buf[1024];

  va_list ap_copy;
  va_copy(ap_copy, ap);

  errno = 0;
  int result = vsnprintf(stack_buf, arraysize(stack_buf), format, ap_copy);
  va_end(ap_copy);

  if (result >= 0 && result < static_cast<int>(arraysize(stack_buf))) {
    // It fit.
    dst->append(stack_buf, result);
    return;
  }

  // Repeatedly increase buffer size until it fits.
  int mem_length = arraysize(stack_buf);
  while (true) {
    if (result < 0) {
      if (errno != 0 && errno != EOVERFLOW)
        return;
      // Try doubling the buffer size.
      mem_length *= 2;
    } else {
      // We need exactly "result + 1" characters.
      mem_length = result + 1;
    }

    if (mem_length > 32 * 1024 * 1024) {
      // That should be plenty, don't try anything larger.  This protects
      // against huge allocations when using vsnprintfT implementations that
      // return -1 for reasons other than overflow without setting errno.
      return;
    }

    std::vector<char> mem_buf(mem_length);

    // NOTE: You can only use a va_list once.  Since we're in a while loop, we
    // need to make a new copy each time so we don't use up the original.
    va_copy(ap_copy, ap);
    result = vsnprintf(&mem_buf[0], mem_length, format, ap_copy);
    va_end(ap_copy);

    if ((result >= 0) && (result < mem_length)) {
      // It fit.
      dst->append(&mem_buf[0], result);
      return;
    }
  }
}

template<typename STR>
STR TrimStringT(const STR& input, const typename STR::value_type trim_chars[]) {
  if (input.empty()) {
    return "";
  }

  // Find the edges of leading/trailing whitespace.
  const typename STR::size_type first_good_char =
      input.find_first_not_of(trim_chars);
  const typename STR::size_type last_good_char =
      input.find_last_not_of(trim_chars);

  if (first_good_char == STR::npos || last_good_char == STR::npos) {
    return "";
  }

  // Trim the whitespace.
  return input.substr(first_good_char, last_good_char - first_good_char + 1);
}

}  // namespace

void StringAppendV(std::string* dst, const char* format, va_list ap) {
  StringAppendVT(dst, format, ap);
}

std::string StringPrintf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::string result;
  StringAppendV(&result, format, ap);
  va_end(ap);
  return result;
}

std::string TrimWhitespaceASCII(const std::string& input) {
  return TrimStringT(input, kWhitespaceASCII);
}

}  // namespace gestures
