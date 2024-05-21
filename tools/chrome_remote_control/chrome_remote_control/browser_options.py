# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import optparse
import os
import sys
import shlex
import logging

from chrome_remote_control import browser_finder

class BrowserOptions(optparse.Values):
  """Options to be used for discovering and launching a browser."""

  def __init__(self, browser_type=None):
    optparse.Values.__init__(self)

    self.browser_type = browser_type
    self.browser_executable = None
    self.chrome_root = None
    self.android_device = None
    self.cros_ssh_identity = None

    self.dont_override_profile = False
    self.extra_browser_args = []
    self.show_stdout = False

    self.cros_remote = None

    self.verbosity = 0

  def Copy(self):
    other = BrowserOptions()
    other.__dict__.update(self.__dict__)
    return other

  def CreateParser(self, *args, **kwargs):
    parser = optparse.OptionParser(*args, **kwargs)

    # Selection group
    group = optparse.OptionGroup(parser, 'Which browser to use')
    group.add_option('--browser',
        dest='browser_type',
        default=None,
        help='Browser type to run, '
             'in order of priority. Supported values: list,%s' %
             browser_finder.ALL_BROWSER_TYPES)
    group.add_option('--browser-executable',
        dest='browser_executable',
        help='The exact browser to run.')
    group.add_option('--chrome-root',
        dest='chrome_root',
        help='Where to look for chrome builds.'
             'Defaults to searching parent dirs by default.')
    group.add_option('--device',
        dest='android_device',
        help='The android device ID to use'
             'If not specified, only 0 or 1 connected devcies are supported.')
    group.add_option(
        '--remote',
        dest='cros_remote',
        default=os.getenv('REMOTE'),
        help='The IP address of a remote ChromeOS device to use. ' +
             'Defaults to $REMOTE from environment variable if set.')
    group.add_option('--identity',
        dest='cros_ssh_identity',
        default=None,
        help='The identity file to use when ssh\'ing into the ChromeOS device')
    parser.add_option_group(group)

    # Browser options
    group = optparse.OptionGroup(parser, 'Browser options')
    group.add_option('--dont-override-profile', action='store_true',
        dest='dont_override_profile',
        help='Uses the regular user profile instead of a clean one')
    group.add_option('--extra-browser-args',
        dest='extra_browser_args_as_string',
        help='Additional arguments to pass to the browser when it starts')
    group.add_option('--show-stdout',
        action='store_true',
        help='When possible, will display the stdout of the process')
    parser.add_option_group(group)

    # Debugging options
    group = optparse.OptionGroup(parser, 'When things go wrong')
    group.add_option(
      '-v', '--verbose', action='count', dest='verbosity',
      help='Increase verbosity level (repeat as needed)')
    parser.add_option_group(group)

    real_parse = parser.parse_args
    def ParseArgs(args=None):
      defaults = parser.get_default_values()
      for k, v in defaults.__dict__.items():
        if k in self.__dict__ and self.__dict__[k] != None:
          continue
        self.__dict__[k] = v
      ret = real_parse(args, self) # pylint: disable=E1121

      if self.verbosity >= 2:
        logging.basicConfig(level=logging.DEBUG)
      elif self.verbosity:
        logging.basicConfig(level=logging.INFO)
      else:
        logging.basicConfig(level=logging.WARNING)

      if self.browser_executable and not self.browser_type:
        self.browser_type = 'exact'
      if not self.browser_executable and not self.browser_type:
        sys.stderr.write('Must provide --browser=<type>. ' +
                         'Use --browser=list for valid options.\n')
        sys.exit(1)
      if self.browser_type == 'list':
        types = browser_finder.GetAllAvailableBrowserTypes(self)
        sys.stderr.write('Available browsers:\n')
        sys.stdout.write('  %s\n' % '\n  '.join(types))
        sys.exit(1)
      if self.extra_browser_args_as_string: # pylint: disable=E1101
        tmp = shlex.split(
          self.extra_browser_args_as_string) # pylint: disable=E1101
        self.extra_browser_args.extend(tmp)
        delattr(self, 'extra_browser_args_as_string')
      return ret
    parser.parse_args = ParseArgs
    return parser

# This global variable can be set to a BrowserOptions object by the test harness
# to allow multiple unit tests to use a specific browser, in face of multiple
# options.
options_for_unittests = None
