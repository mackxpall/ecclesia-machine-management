/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This library defines a fake clock. It is not a full mock object; instead it
// is a simulation of a clock that does not move forward except when explicitly
// instructed to.

#ifndef ECCLESIA_LIB_TIME_CLOCK_FAKE_H_
#define ECCLESIA_LIB_TIME_CLOCK_FAKE_H_

#include <functional>
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

class FakeClock : public Clock {
 public:
  // Construct a fake clock. It can be initialized to a specific time, or if
  // not specified it will default to (the real) Now.
  FakeClock() : FakeClock(absl::Now()) {}
  FakeClock(std::function<void(absl::Duration)> sleep) :
    time_(absl::Now()), sleep_(sleep) {}
  FakeClock(absl::Time now, std::function<void(absl::Duration)> sleep) :
    time_(now), sleep_(sleep) {}
  explicit FakeClock(absl::Time now) : time_(now) {}

  absl::Time Now() const override { return time_; }

  void Sleep(absl::Duration d) override {
    if (sleep_.has_value()) {
      // Call the provided sleep function
      (*sleep_)(d);
    } else {
      // Otherwise use default clock-like behavior
      SleepFor(d);
    }
  }

  void SleepFor(absl::Duration d) { AdvanceTime(d); }

  // Move time forward by duration. This cannot be used to move time back.
  void AdvanceTime(absl::Duration duration) { time_ += duration; }

 private:
  // The current time this clock holds, can be provided at construction
  absl::Time time_;

  // An optional sleep function that can be provided during construction
  // will be used if provided otherwise SleepFor is used
  const absl::optional<std::function<void(absl::Duration)>> sleep_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_TIME_CLOCK_FAKE_H_
