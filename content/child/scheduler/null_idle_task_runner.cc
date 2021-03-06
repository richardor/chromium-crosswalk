// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/scheduler/null_idle_task_runner.h"

namespace content {

NullIdleTaskRunner::NullIdleTaskRunner()
    : SingleThreadIdleTaskRunner(nullptr,
                                 nullptr,
                                 base::Callback<void(base::TimeTicks*)>(),
                                 "null.taskrunner") {
}

NullIdleTaskRunner::~NullIdleTaskRunner() {
}

void NullIdleTaskRunner::PostIdleTask(
    const tracked_objects::Location& from_here,
    const IdleTask& idle_task) {
}

void NullIdleTaskRunner::PostNonNestableIdleTask(
    const tracked_objects::Location& from_here,
    const IdleTask& idle_task) {
}

void NullIdleTaskRunner::PostIdleTaskAfterWakeup(
  const tracked_objects::Location& from_here,
  const IdleTask& idle_task) {
}

}  // namespace content
