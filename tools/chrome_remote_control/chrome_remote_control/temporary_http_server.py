# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import socket
import subprocess
import sys
import urlparse

class TemporaryHTTPServer(object):
  def __init__(self, browser_backend, path):
    self._server = None
    self._devnull = None
    self._path = path
    self._forwarder = None

    tmp = socket.socket()
    tmp.bind(('', 0))
    port = tmp.getsockname()[1]
    tmp.close()
    self._host_port = port

    assert os.path.exists(path)
    assert os.path.isdir(path)

    self._devnull = open(os.devnull, 'w')
    self._server = subprocess.Popen(
        [sys.executable, '-m', 'SimpleHTTPServer', str(self._host_port)],
        cwd=self._path,
        stdout=self._devnull, stderr=self._devnull)

    self._forwarder = browser_backend.CreateForwarder(self._host_port)

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Close()

  def __del__(self):
    self.Close()

  def Close(self):
    if self._forwarder:
      self._forwarder.Close()
      self._forwarder = None
    if self._server:
      self._server.kill()
      self._server = None
    if self._devnull:
      self._devnull.close()
      self._devnull = None

  @property
  def path(self):
    return self._path

  @property
  def url(self):
    return self._forwarder.url

  def UrlOf(self, path):
    return urlparse.urljoin(self.url, path)
