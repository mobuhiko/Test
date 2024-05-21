// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/testshell/chrome_main_delegate_testshell_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "chrome/android/testshell/tab_manager.h"

static base::android::RegistrationMethod kRegistrationMethods[] = {
    { "TabManager", chrome::RegisterTabManager },
};

ChromeMainDelegateTestShellAndroid::ChromeMainDelegateTestShellAndroid() {
}

ChromeMainDelegateTestShellAndroid::~ChromeMainDelegateTestShellAndroid() {
}

bool ChromeMainDelegateTestShellAndroid::RegisterApplicationNativeMethods(
    JNIEnv* env) {
  if (!ChromeMainDelegateAndroid::RegisterApplicationNativeMethods(env))
    return false;

  return base::android::RegisterNativeMethods(env,
                                              kRegistrationMethods,
                                              arraysize(kRegistrationMethods));
}
