# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core.platform import tracing_category_filter
from telemetry.core.platform import tracing_options
from telemetry.page import action_runner
from telemetry.timeline.model import TimelineModel
from telemetry.value import trace
from telemetry.web_perf import timeline_interaction_record as tir_module

from measurements import smooth_gesture_util


RUN_SMOOTH_ACTIONS = 'RunSmoothAllActions'


class TimelineController(object):
  def __init__(self):
    super(TimelineController, self).__init__()
    self.trace_categories = None
    self._model = None
    self._renderer_process = None
    self._smooth_records = []
    self._interaction = None

  def SetUp(self, page, tab):
    """Starts gathering timeline data.

    """
    # Resets these member variables incase this object is reused.
    self._model = None
    self._renderer_process = None
    if not tab.browser.platform.tracing_controller.IsChromeTracingSupported():
      raise Exception('Not supported')
    category_filter = tracing_category_filter.TracingCategoryFilter(
        filter_string=self.trace_categories)
    for delay in page.GetSyntheticDelayCategories():
      category_filter.AddSyntheticDelay(delay)
    options = tracing_options.TracingOptions()
    options.enable_chrome_trace = True
    tab.browser.platform.tracing_controller.Start(options, category_filter)

  def Start(self, tab):
    # Start the smooth marker for all actions.
    runner = action_runner.ActionRunner(tab)
    self._interaction = runner.BeginInteraction(
        RUN_SMOOTH_ACTIONS)

  def Stop(self, tab, results):
    # End the smooth marker for all actions.
    self._interaction.End()
    # Stop tracing.
    timeline_data = tab.browser.platform.tracing_controller.Stop()
    results.AddValue(trace.TraceValue(
        results.current_page, timeline_data))
    self._model = TimelineModel(timeline_data)
    self._renderer_process = self._model.GetRendererProcessFromTabId(tab.id)
    renderer_thread = self.model.GetRendererThreadFromTabId(tab.id)

    run_smooth_actions_record = None
    self._smooth_records = []
    for event in renderer_thread.async_slices:
      if not tir_module.IsTimelineInteractionRecord(event.name):
        continue
      r = tir_module.TimelineInteractionRecord.FromAsyncEvent(event)
      if r.label == RUN_SMOOTH_ACTIONS:
        assert run_smooth_actions_record is None, (
          'TimelineController cannot issue more than 1 %s record' %
          RUN_SMOOTH_ACTIONS)
        run_smooth_actions_record = r
      else:
        self._smooth_records.append(
          smooth_gesture_util.GetAdjustedInteractionIfContainGesture(
            self.model, r))

    # If there is no other smooth records, we make measurements on time range
    # marked by timeline_controller itself.
    # TODO(nednguyen): when crbug.com/239179 is marked fixed, makes sure that
    # page sets are responsible for issueing the markers themselves.
    if len(self._smooth_records) == 0 and run_smooth_actions_record:
      self._smooth_records = [run_smooth_actions_record]


  def CleanUp(self, tab):
    if tab.browser.platform.tracing_controller.is_tracing_running:
      tab.browser.platform.tracing_controller.Stop()

  @property
  def model(self):
    return self._model

  @property
  def renderer_process(self):
    return self._renderer_process

  @property
  def smooth_records(self):
    return self._smooth_records
