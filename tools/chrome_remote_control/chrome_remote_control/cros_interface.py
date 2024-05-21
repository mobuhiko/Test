# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A wrapper around ssh for common operations on a CrOS-based device"""
import logging
import os
import re
import subprocess
import sys
import time
import tempfile

from chrome_remote_control import util

_next_remote_port = 9224

# TODO(nduca): This whole file is built up around making individual ssh calls
# for each operation. It really could get away with a single ssh session built
# around pexpect, I suspect, if we wanted it to be faster. But, this was
# convenient.

def RunCmd(args, cwd=None):
  """Opens a subprocess to execute a program and returns its return value.

  Args:
    args: A string or a sequence of program arguments. The program to execute is
      the string or the first item in the args sequence.
    cwd: If not None, the subprocess's current directory will be changed to
      |cwd| before it's executed.

  Returns:
    Return code from the command execution.
  """
  logging.debug(' '.join(args) + ' ' + (cwd or ''))
  with open(os.devnull, 'w') as devnull:
    p = subprocess.Popen(args=args, cwd=cwd, stdout=devnull,
                         stderr=devnull, stdin=devnull, shell=False)
    return p.wait()

def GetAllCmdOutput(args, cwd=None):
  """Open a subprocess to execute a program and returns its output.

  Args:
    args: A string or a sequence of program arguments. The program to execute is
      the string or the first item in the args sequence.
    cwd: If not None, the subprocess's current directory will be changed to
      |cwd| before it's executed.

  Returns:
    Captures and returns the command's stdout.
    Prints the command's stderr to logger (which defaults to stdout).
  """
  logging.debug(' '.join(args) + ' ' + (cwd or ''))
  with open(os.devnull, 'w') as devnull:
    p = subprocess.Popen(args=args, cwd=cwd, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, stdin=devnull, shell=False)
    stdout, stderr = p.communicate()
    logging.debug(' > stdout=[%s], stderr=[%s]', stdout, stderr)
    return stdout, stderr

class DeviceSideProcess(object):
  def __init__(self,
               cri,
               device_side_args,
               prevent_output=True,
               extra_ssh_args=None,
               leave_ssh_alive=False,
               env=None,
               login_shell=False):

    # Init members first so that Close will always succeed.
    self._cri = cri
    self._proc = None
    self._devnull = open(os.devnull, 'w')

    if prevent_output:
      out = self._devnull
    else:
      out = sys.stderr

    cri.GetCmdOutput(['rm', '-rf', '/tmp/cros_interface_remote_device_pid'])
    cmd_str = ' '.join(device_side_args)
    if env:
      env_str = ' '.join(['%s=%s' % (k, v) for k, v in env.items()])
      cmd = env_str + ' ' + cmd_str
    else:
      cmd = cmd_str
    contents = """%s&\n""" % cmd
    contents += 'echo $! > /tmp/cros_interface_remote_device_pid\n'
    cri.PushContents(contents, '/tmp/cros_interface_remote_device_bootstrap.sh')

    cmdline = ['/bin/bash']
    if login_shell:
      cmdline.append('-l')
    cmdline.append('/tmp/cros_interface_remote_device_bootstrap.sh')
    proc = subprocess.Popen(
      cri.FormSSHCommandLine(cmdline,
                              extra_ssh_args=extra_ssh_args),
      stdout=out,
      stderr=out,
      stdin=self._devnull,
      shell=False)

    time.sleep(0.1)
    def TryGetResult():
      try:
        self._pid = cri.GetFileContents(
            '/tmp/cros_interface_remote_device_pid').strip()
        return True
      except OSError:
        return False
    try:
      util.WaitFor(TryGetResult, 5)
    except util.TimeoutException:
      raise Exception('Something horrible has happened!')

    # Killing the ssh session leaves the process running. We dont
    # need it anymore, unless we have port-forwards.
    if not leave_ssh_alive:
      proc.kill()
    else:
      self._proc = proc

    self._pid = int(self._pid)
    if not self.IsAlive():
      raise OSError('Process did not come up or did not stay alive verry long!')
    self._cri = cri

  def Close(self, try_sigint_first=False):
    if self.IsAlive():
      # Try to politely shutdown, first.
      if try_sigint_first:
        self._cri.GetAllCmdOutput(
          ['kill', '-INT', str(self._pid)])
        try:
          self.Wait(timeout=0.5)
        except util.TimeoutException:
          pass

      if self.IsAlive():
        self._cri.GetAllCmdOutput(
          ['kill', '-KILL', str(self._pid)])
        try:
          self.Wait(timeout=5)
        except util.TimeoutException:
          pass

      if self.IsAlive():
        raise Exception('Could not shutdown the process.')

    self._cri = None
    if self._proc:
      self._proc.kill()
      self._proc = None

    if self._devnull:
      self._devnull.close()
      self._devnull = None

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Close()
    return

  def Wait(self, timeout=1):
    if not self._pid:
      raise Exception('Closed')
    def IsDone():
      return not self.IsAlive()
    util.WaitFor(IsDone, timeout)
    self._pid = None

  def IsAlive(self):
    if not self._pid:
      return False
    exists = self._cri.FileExistsOnDevice('/proc/%i/cmdline' % self._pid)
    return exists

def HasSSH():
  try:
    RunCmd(['ssh'])
    RunCmd(['scp'])
    return True
  except OSError:
    return False

class LoginException(Exception):
  pass

class KeylessLoginRequiredException(LoginException):
  pass

class CrOSInterface(object): # pylint: disable=R0923
  def __init__(self, hostname, ssh_identity = None):
    self._hostname = hostname
    self._ssh_identity = None

    if ssh_identity:
      self._ssh_identity = os.path.abspath(os.path.expanduser(ssh_identity))

  @property
  def hostname(self):
    return self._hostname

  def FormSSHCommandLine(self, args, extra_ssh_args=None):
    full_args = ['ssh',
                 '-o ConnectTimeout=5',
                 '-o ForwardX11=no',
                 '-o ForwardX11Trusted=no',
                 '-o StrictHostKeyChecking=yes',
                 '-o KbdInteractiveAuthentication=no',
                 '-o PreferredAuthentications=publickey',
                 '-n']
    if self._ssh_identity is not None:
      full_args.extend(['-i', self._ssh_identity])
    if extra_ssh_args:
      full_args.extend(extra_ssh_args)
    full_args.append('root@%s' % self._hostname)
    full_args.extend(args)
    return full_args

  def GetAllCmdOutput(self, args, cwd=None):
    return GetAllCmdOutput(self.FormSSHCommandLine(args), cwd)

  def TryLogin(self):
    stdout, stderr = self.GetAllCmdOutput(['echo', '$USER'])

    if stderr != '':
      if 'Host key verification failed' in stderr:
        raise LoginException(('%s host key verification failed. ' +
                             'SSH to it manually to fix connectivity.') %
            self._hostname)
      if 'Operation timed out' in stderr:
        raise LoginException('Timed out while logging into %s' % self._hostname)
      if 'Permission denied (publickey,keyboard-interactive)' in stderr:
        raise KeylessLoginRequiredException(
          'Need to set up ssh auth for %s' % self._hostname)
      raise LoginException('While logging into %s, got %s' % (
          self._hostname, stderr))
    if stdout != 'root\n':
      raise LoginException(
        'Logged into %s, expected $USER=root, but got %s.' % (
          self._hostname, stdout))

  def FileExistsOnDevice(self, file_name):
    stdout, stderr = self.GetAllCmdOutput([
        'if', 'test', '-a', file_name, ';',
        'then', 'echo', '1', ';',
        'fi'
        ])
    if stderr != '':
      if "Connection timed out" in stderr:
        raise OSError('Machine wasn\'t responding to ssh: %s' %
                      stderr)
      raise OSError('Unepected error: %s' % stderr)
    return stdout == '1\n'

  def PushContents(self, text, remote_filename):
    with tempfile.NamedTemporaryFile() as f:
      f.write(text)
      f.flush()
      args = ['scp',
              '-o ConnectTimeout=5',
              '-o KbdInteractiveAuthentication=no',
              '-o PreferredAuthentications=publickey',
              '-o StrictHostKeyChecking=yes' ]

      if self._ssh_identity:
        args.extend(['-i', self._ssh_identity])

      args.extend([os.path.abspath(f.name),
                   'root@%s:%s' % (self._hostname, remote_filename)])

      stdout, stderr = GetAllCmdOutput(args)
      if stderr != '':
        assert 'No such file or directory' in stderr
        raise OSError

  def GetFileContents(self, filename):
    with tempfile.NamedTemporaryFile() as f:
      args = ['scp',
              '-o ConnectTimeout=5',
              '-o KbdInteractiveAuthentication=no',
              '-o PreferredAuthentications=publickey',
              '-o StrictHostKeyChecking=yes' ]

      if self._ssh_identity:
        args.extend(['-i', self._ssh_identity])

      args.extend(['root@%s:%s' % (self._hostname, filename),
                   os.path.abspath(f.name)])

      stdout, stderr = GetAllCmdOutput(args)

      if stderr != '':
        assert 'No such file or directory' in stderr
        raise OSError

      with open(f.name, 'r') as f2:
        return f2.read()

  def ListProcesses(self):
    stdout, stderr = self.GetAllCmdOutput([
        '/bin/ps', '--no-headers',
        '-A',
        '-o', 'pid,args'])
    assert stderr == ''
    procs = []
    for l in stdout.split('\n'): # pylint: disable=E1103
      if l == '':
        continue
      m = re.match('^\s*(\d+)\s+(.+)', l, re.DOTALL)
      assert m
      procs.append(m.groups())
    return procs

  def KillAllMatching(self, predicate):
    kills = ['kill', '-KILL']
    for p in self.ListProcesses():
      if predicate(p[1]):
        logging.info('Killing %s', repr(p))
        kills.append(p[0])
    if len(kills) > 2:
      self.GetCmdOutput(kills)
    return len(kills) - 2

  def IsServiceRunning(self, service_name):
    stdout, stderr = self.GetAllCmdOutput([
        'status', service_name])
    assert stderr == ''
    return 'running, process' in stdout

  def GetCmdOutput(self, args):
    stdout, stderr = self.GetAllCmdOutput(args)
    assert stderr == ''
    return stdout

  def GetRemotePort(self):
    global _next_remote_port
    port = _next_remote_port
    _next_remote_port += 1
    return port
