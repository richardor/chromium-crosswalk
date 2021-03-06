// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_LOGGER_H_
#define NET_LOG_NET_LOG_LOGGER_H_

#include <stdio.h>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/log/net_log.h"

namespace base {
class DictionaryValue;
class FilePath;
class Value;
}

namespace net {

class URLRequestContext;

// NetLogLogger watches the NetLog event stream, and sends all entries to
// a file specified on creation.
//
// The text file will contain a single JSON object.
class NET_EXPORT NetLogLogger : public NetLog::ThreadSafeObserver {
 public:
  NetLogLogger();
  ~NetLogLogger() override;

  // Sets the log level to log at. Must be called before StartObserving.
  void set_log_level(NetLog::LogLevel log_level);

  // Starts observing |net_log| and writes output to |file|.  Must not already
  // be watching a NetLog.  Separate from constructor to enforce thread safety.
  //
  // |file| must be a non-NULL empty file that's open for writing.
  //
  // |constants| is an optional legend for decoding constant values used in the
  // log.  It should generally be a modified version of GetNetConstants().  If
  // not present, the output of GetNetConstants() will be used.
  //
  // |url_request_context| is an optional URLRequestContext that will be used to
  // pre-populate the log with information about in-progress events.
  // If the context is non-NULL, this must be called on the context's thread.
  void StartObserving(NetLog* net_log,
                      base::ScopedFILE file,
                      base::Value* constants,
                      net::URLRequestContext* url_request_context);

  // Stops observing net_log().  Must already be watching.  Must be called
  // before destruction of the NetLogLogger and the NetLog.
  //
  // |url_request_context| is an optional argument used to added additional
  // network stack state to the log.  If the context is non-NULL, this must be
  // called on the context's thread.
  void StopObserving(net::URLRequestContext* url_request_context);

  // net::NetLog::ThreadSafeObserver implementation:
  void OnAddEntry(const NetLog::Entry& entry) override;

 private:
  base::ScopedFILE file_;

  // The LogLevel to log at.
  NetLog::LogLevel log_level_;

  // True if OnAddEntry() has been called at least once.
  bool added_events_;

  DISALLOW_COPY_AND_ASSIGN(NetLogLogger);
};

}  // namespace net

#endif  // NET_LOG_NET_LOG_LOGGER_H_
