# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds browsers that can be controlled by chrome_remote_control."""

import logging

from chrome_remote_control import android_browser_finder
from chrome_remote_control import cros_browser_finder
from chrome_remote_control import desktop_browser_finder

ALL_BROWSER_TYPES = (
    desktop_browser_finder.ALL_BROWSER_TYPES + ',' +
    android_browser_finder.ALL_BROWSER_TYPES + ',' +
    cros_browser_finder.ALL_BROWSER_TYPES)

class BrowserTypeRequiredException(Exception):
  pass

def FindBrowser(options):
  """Finds the best PossibleBrowser object to run given the provided
  BrowserOptions object. The returned possiblity object can then be used to
  connect to and control the located browser.
  """
  if options.browser_type == 'exact' and options.browser_executable == None:
    raise Exception('browser_type=exact requires browser_executable be set.')

  if options.browser_type != 'exact' and options.browser_executable != None:
    raise Exception('browser_executable requires browser_executable=exact.')

  if options.browser_type == None:
    raise BrowserTypeRequiredException('browser_type must be specified')

  browsers = []
  browsers.extend(desktop_browser_finder.FindAllAvailableBrowsers(options))
  browsers.extend(android_browser_finder.FindAllAvailableBrowsers(options))
  browsers.extend(cros_browser_finder.FindAllAvailableBrowsers(options))

  if options.browser_type == 'any':
    types = ALL_BROWSER_TYPES.split(',')
    def compare_browsers_on_type_priority(x, y):
      x_idx = types.index(x.browser_type)
      y_idx = types.index(y.browser_type)
      return x_idx - y_idx
    browsers.sort(compare_browsers_on_type_priority)
    if len(browsers) >= 1:
      return browsers[0]
    else:
      return None

  matching_browsers = [b for b in browsers
                       if b.browser_type == options.browser_type]

  if len(matching_browsers) == 1:
    return matching_browsers[0]
  elif len(matching_browsers) > 1:
    logging.warning('Multiple browsers of the same type found: %s' % (
                    repr(matching_browsers)))
    return matching_browsers[0]
  else:
    return None

def GetAllAvailableBrowserTypes(options):
  """Returns an array of browser types supported on this system."""
  browsers = []
  browsers.extend(desktop_browser_finder.FindAllAvailableBrowsers(options))
  browsers.extend(android_browser_finder.FindAllAvailableBrowsers(options))
  browsers.extend(cros_browser_finder.FindAllAvailableBrowsers(options))

  type_list = set([browser.browser_type for browser in browsers])
  type_list = list(type_list)
  type_list.sort()
  return type_list
