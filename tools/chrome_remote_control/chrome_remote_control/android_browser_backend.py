# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import tempfile
import json

from chrome_remote_control import adb_commands
from chrome_remote_control import browser_backend

class AndroidBrowserBackend(browser_backend.BrowserBackend):
  """The backend for controlling a browser instance running on Android.
  """
  def __init__(self, options, adb, package, is_content_shell,
               cmdline_file, activity, devtools_remote_port):
    super(AndroidBrowserBackend, self).__init__(is_content_shell)
    # Initialize fields so that an explosion during init doesn't break in Close.
    self._options = options
    self._adb = adb
    self._package = package
    self._cmdline_file = cmdline_file
    self._activity = activity
    self._port = 9222
    self._devtools_remote_port = devtools_remote_port

    # Beginnings of a basic command line.
    if is_content_shell:
      pseudo_exec_name = 'content_shell'
    else:
      pseudo_exec_name = 'chrome'
    args = [pseudo_exec_name,
            '--disable-fre', '--no-first-run']

    # Kill old browser.
    self._adb.KillAll(self._package)
    self._adb.KillAll('forwarder')
    self._adb.Forward('tcp:9222', self._devtools_remote_port)

    # Chrome Android doesn't listen to --user-data-dir.
    # TODO: symlink the app's Default, files and cache dir
    # to somewhere safe.
    if not is_content_shell and not options.dont_override_profile:
      # Set up the temp dir
      # self._tmpdir = '/sdcard/chrome_remote_control_data'
      # self._adb.RunShellCommand('rm -r %s' %  self._tmpdir)
      # args.append('--user-data-dir=%s' % self._tmpdir)
      pass

    # Set up the command line.
    args.extend(options.extra_browser_args)
    with tempfile.NamedTemporaryFile() as f:
      f.write(' '.join(args))
      f.flush()
      self._adb.Push(f.name, cmdline_file)

    # Force devtools protocol on, if not already done.
    if not is_content_shell:
      # Make sure we can find the apps' prefs file
      app_data_dir = '/data/data/%s' % self._package
      prefs_file = (app_data_dir +
                    '/app_chrome/Default/Preferences')
      if not self._adb.FileExistsOnDevice(prefs_file):
        logging.critical(
            'android_browser_backend: Could not find preferences file ' +
            '%s for %s' % (prefs_file, self._package))
        raise browser_backend.BrowserGoneException('Missing preferences file.')

      with tempfile.NamedTemporaryFile() as raw_f:
        self._adb.Pull(prefs_file, raw_f.name)
        with open(raw_f.name, 'r') as f:
          txt_in = f.read()
          preferences = json.loads(txt_in)
        changed = False
        if 'devtools' not in preferences:
          preferences['devtools'] = {}
          changed = True
        if 'remote_enabled' not in preferences['devtools']:
          preferences['devtools']['remote_enabled'] = True
          changed = True
        if preferences['devtools']['remote_enabled'] != True:
          preferences['devtools']['remote_enabled'] = True
          changed = True
        if changed:
          logging.warning('Manually enabled devtools protocol on %s' %
                          self._package)
          with open(raw_f.name, 'w') as f:
            txt = json.dumps(preferences, indent=2)
            f.write(txt)
          self._adb.Push(raw_f.name, prefs_file)

    # Start it up!
    self._adb.StartActivity(self._package,
                            self._activity,
                            True,
                            None,
                            'chrome://newtab/')
    try:
      self._WaitForBrowserToComeUp()
    except:
      import traceback
      traceback.print_exc()
      self.Close()
      raise

  def __del__(self):
    self.Close()

  def Close(self):
    self._adb.KillAll(self._package)

  def IsBrowserRunning(self):
    pids = self._adb.ExtractPid(self._package)
    return len(pids) != 0

  def CreateForwarder(self, host_port):
    return adb_commands.Forwarder(self._adb, host_port)


