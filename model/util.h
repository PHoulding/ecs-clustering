/// \file util.h
/// \author Keefer Rourke <krourke@uoguelph.ca>
/// \brief Generic utilties that make it easier to read and manipulate values.
///
/// Copyright (c) 2020 by Keefer Rourke <krourke@uoguelph.ca>
/// Permission to use, copy, modify, and/or distribute this software for any
/// purpose with or without fee is hereby granted, provided that the above
/// copyright notice and this permission notice appear in all copies.
///
/// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
/// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
/// AND FITNESS. IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
/// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
/// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
/// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
/// PERFORMANCE OF THIS SOFTWARE.

#ifndef __util_h
#define __util_h

#include <inttypes.h>
#include <iostream>
#include <vector>

//#include "ns3/core-module.h"

/// User defined literal for metric lengths.
constexpr int32_t operator"" _meters(const unsigned long long meters) {
  return static_cast<int32_t>(meters);
}
constexpr double operator"" _meters(const long double meters) { return meters; }

/// User defined literal for unit measures in seconds.
constexpr double operator"" _seconds(const long double seconds) { return seconds; }

/// User defined literal for minute measures, auto converted to seconds.
constexpr double operator"" _minutes(const long double minutes) { return minutes * 60; }

/// User defined literal for meters/second velocities.
constexpr double operator"" _mps(const long double mps) { return mps; }

/// User defined literal for byte counts.
constexpr int32_t operator"" _b(const unsigned long long bytes) {
  return static_cast<int32_t>(bytes);
}

/// User defined literal for percentages.
constexpr double operator"" _percent(const long double num) { return num / 100.0; }

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
  os << "[";
  for (size_t i = 0; i < vec.size(); i++) {
    os << vec[i];
    if (i < vec.size() - 1) {
      os << ", ";
    }
  }
  os << "]";
  return os;
}

#endif
