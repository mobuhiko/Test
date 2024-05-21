# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from chrome_remote_control import multi_page_benchmark
from chrome_remote_control import util

class DidNotScrollException(multi_page_benchmark.MeasurementFailure):
  def __init__(self):
    super(DidNotScrollException, self).__init__('Page did not scroll')

def CalcScrollResults(rendering_stats_deltas):
  num_frames_sent_to_screen = rendering_stats_deltas['numFramesSentToScreen']

  mean_frame_time_seconds = (
    rendering_stats_deltas['totalTimeInSeconds'] /
      float(num_frames_sent_to_screen))

  dropped_percent = (
    rendering_stats_deltas['droppedFrameCount'] /
    float(num_frames_sent_to_screen))

  return {
      'mean_frame_time_ms': round(mean_frame_time_seconds * 1000, 3),
      'dropped_percent': round(dropped_percent * 100, 1)
      }

class ScrollingBenchmark(multi_page_benchmark.MultiPageBenchmark):
  def __init__(self):
    super(ScrollingBenchmark, self).__init__()

  def AddOptions(self, parser):
    parser.add_option('--no-gpu-benchmarking-extension', action='store_true',
        dest='no_gpu_benchmarking_extension',
        help='Disable the chrome.gpuBenchmarking extension.')
    parser.add_option('--report-all-results', dest='report_all_results',
                      action='store_true',
                      help='Reports all data collected, not just FPS')

  @staticmethod
  def ScrollPageFully(tab):
    scroll_js_path = os.path.join(os.path.dirname(__file__), 'scroll.js')
    scroll_js = open(scroll_js_path, 'r').read()

    # Run scroll test.
    tab.runtime.Execute(scroll_js)
    tab.runtime.Execute("""
      window.__renderingStatsDeltas = null;
      new __ScrollTest(function(rendering_stats_deltas) {
        window.__renderingStatsDeltas = rendering_stats_deltas;
      });
    """)

    # Poll for scroll benchmark completion.
    util.WaitFor(lambda: tab.runtime.Evaluate(
        'window.__renderingStatsDeltas'), 60)

    rendering_stats_deltas = tab.runtime.Evaluate(
      'window.__renderingStatsDeltas')

    if not (rendering_stats_deltas['numFramesSentToScreen'] > 0):
      raise DidNotScrollException()
    return rendering_stats_deltas

  def CustomizeBrowserOptions(self, options):
    if not options.no_gpu_benchmarking_extension:
      options.extra_browser_args.append('--enable-gpu-benchmarking')

  def MeasurePage(self, _, tab):
    rendering_stats_deltas = self.ScrollPageFully(tab)
    scroll_results = CalcScrollResults(rendering_stats_deltas)
    if self.options.report_all_results:
      all_results = {}
      all_results.update(rendering_stats_deltas)
      all_results.update(scroll_results)
      return all_results
    return scroll_results



def Main():
  return multi_page_benchmark.Main(ScrollingBenchmark())
