# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from chrome_remote_control import multi_page_benchmark
from gpu_tools import scrolling_benchmark

class TextureUploadBenchmark(scrolling_benchmark.ScrollingBenchmark):
  def MeasurePage(self, _, tab):
    rendering_stats_deltas = self.ScrollPageFully(tab)
    return {
      'texture_upload_count': rendering_stats_deltas['textureUploadCount']
    }

def Main():
  return multi_page_benchmark.Main(TextureUploadBenchmark())
