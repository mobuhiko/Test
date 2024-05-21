# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds android browsers that can be controlled by chrome_remote_control."""

import logging

from chrome_remote_control import browser
from chrome_remote_control import possible_browser
from chrome_remote_control import cros_browser_backend
from chrome_remote_control import (
  cros_interface as real_cros_interface)

ALL_BROWSER_TYPES = ','.join([
    'cros-chrome',
    ])

class PossibleCrOSBrowser(possible_browser.PossibleBrowser):
  """A launchable android browser instance."""
  def __init__(self, browser_type, options, *args):
    super(PossibleCrOSBrowser, self).__init__(
        browser_type, options)
    self._args = args

  def __repr__(self):
    return 'PossibleCrOSBrowser(browser_type=%s)' % self.browser_type

  def Create(self):
    backend = cros_browser_backend.CrOSBrowserBackend(
        self.browser_type, self._options, *self._args)
    return browser.Browser(backend)

def FindAllAvailableBrowsers(options,
                             cros_interface = real_cros_interface):
  """Finds all the desktop browsers available on this machine."""
  if options.cros_remote == None:
    logging.debug('No --remote specified, will not probe for CrOS.')
    return []

  if not cros_interface.HasSSH():
    logging.debug('ssh not found. Cannot talk to CrOS devices.')
    return []
  cri = cros_interface.CrOSInterface(options.cros_remote,
                                     options.cros_ssh_identity)

  # Check ssh
  try:
    cri.TryLogin()
  except cros_interface.LoginException, ex:
    if isinstance(ex, cros_interface.KeylessLoginRequiredException):
      logging.warn('Could not ssh into %s. Your device must be configured',
                      options.cros_remote)
      logging.warn('to allow passwordless login as root.')
      logging.warn('For a test-build device, pass this to your script:')
      logging.warn('   --identity $(CHROMITE)/ssh_keys/id_testing:')
      logging.warn('')
      logging.warn('For a developer-mode device, the steps are:')
      logging.warn(' - Ensure you have an id_rsa.pub (etc) on this computer')
      logging.warn(' - On the chromebook:')
      logging.warn('   -  Control-Alt-T; shell; sudo -s')
      logging.warn('   -  openssh-server start')
      logging.warn('   -  scp <this machine>:.ssh/id_rsa.pub /tmp/')
      logging.warn('   -  mkdir /root/.ssh')
      logging.warn('   -  chown go-rx /root/.ssh')
      logging.warn('   -  cat /tmp/id_rsa.pub >> /root/.ssh/authorized_keys')
      logging.warn('   -  chown 0600 /root/.ssh/authorized_keys')
      logging.warn('There, that was easy!')
      logging.warn('')
      logging.warn('P.S. Please, make tell your manager how INANE this.')
    else:
      logging.warn(str(ex))
    return []

  if not cri.FileExistsOnDevice('/opt/google/chrome/chrome'):
    logging.warn('Could not find a chrome on ' % cri.hostname)

  return [PossibleCrOSBrowser('cros-chrome', options, False, cri)]
