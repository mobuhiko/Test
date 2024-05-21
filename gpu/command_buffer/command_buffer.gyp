# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'command_buffer.gypi',
  ],
  'targets': [
    {
      'target_name': 'gles2_utils',
      'type': '<(component)',
      'variables': {
        'gles2_utils_target': 1,
      },
      'all_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos',
        ],
      },
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'export_dependent_settings': [
        '../../base/base.gyp:base',
      ],
    },
  ],
}

