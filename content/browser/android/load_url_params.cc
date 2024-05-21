// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/load_url_params.h"

#include <jni.h>

#include "content/public/browser/navigation_controller.h"
#include "jni/LoadUrlParams_jni.h"

namespace {

using content::NavigationController;

void RegisterConstants(JNIEnv* env) {
  Java_LoadUrlParams_initializeConstants(env,
      NavigationController::LOAD_TYPE_DEFAULT,
      NavigationController::LOAD_TYPE_BROWSER_INITIATED_HTTP_POST,
      NavigationController::LOAD_TYPE_DATA,
      NavigationController::UA_OVERRIDE_INHERIT,
      NavigationController::UA_OVERRIDE_FALSE,
      NavigationController::UA_OVERRIDE_TRUE);
}

}  // namespace

namespace content {

bool RegisterLoadUrlParams(JNIEnv* env) {
  if (!RegisterNativesImpl(env))
    return false;
  RegisterConstants(env);
  return true;
}

}  // namespace content
