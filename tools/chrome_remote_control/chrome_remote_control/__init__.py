# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A library for chrome-based tests.

"""
from chrome_remote_control.browser_finder import FindBrowser
from chrome_remote_control.browser_finder import GetAllAvailableBrowserTypes
from chrome_remote_control.browser_options import BrowserOptions
from chrome_remote_control.browser import Browser
from chrome_remote_control.tab import Tab
from chrome_remote_control.util import TimeoutException, WaitFor

def CreateBrowser(browser_type):
  """Shorthand way to create a browser of a given type

  However, note that the preferred way to create a browser is:
     options = BrowserOptions()
     _, leftover_args, = options.CreateParser().parse_args()
     browser_to_create = FindBrowser(options)
     return browser_to_create.Create()

  as it creates more opportunities for customization and
  error handling."""
  browser_to_create = FindBrowser(BrowserOptions(browser_type))
  if not browser_to_create:
    raise Exception('No browser of type %s found' % browser_type)
  return browser_to_create.Create()
