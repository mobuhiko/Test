// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/browser_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "content/browser/android/android_browser_process.h"
#include "content/browser/android/content_settings.h"
#include "content/browser/android/content_video_view.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/content_view_statics.h"
#include "content/browser/android/download_controller.h"
#include "content/browser/android/load_url_params.h"
#include "content/browser/android/sandboxed_process_launcher.h"
#include "content/browser/android/surface_texture_peer_browser_impl.h"
#include "content/browser/android/touch_point.h"
#include "content/browser/android/web_contents_observer_android.h"
#include "content/browser/geolocation/location_api_adapter_android.h"
#include "content/browser/renderer_host/ime_adapter_android.h"
#include "content/browser/renderer_host/java/java_bound_object.h"

using content::SurfaceTexturePeerBrowserImpl;

namespace {
base::android::RegistrationMethod kContentRegisteredMethods[] = {
  { "AndroidLocationApiAdapter",
    AndroidLocationApiAdapter::RegisterGeolocationService },
  { "AndroidBrowserProcess", content::RegisterAndroidBrowserProcess },
  { "BrowserProcessSurfaceTexture",
    SurfaceTexturePeerBrowserImpl::RegisterBrowserProcessSurfaceTexture },
  { "ContentSettings", content::ContentSettings::RegisterContentSettings },
  { "ContentVideoView", content::ContentVideoView::RegisterContentVideoView },
  { "ContentViewCore", content::RegisterContentViewCore },
  { "DownloadController",
    content::DownloadController::RegisterDownloadController },
  { "RegisterImeAdapter", content::RegisterImeAdapter },
  { "JavaBoundObject", JavaBoundObject::RegisterJavaBoundObject },
  { "LoadUrlParams", content::RegisterLoadUrlParams },
  { "SandboxedProcessLauncher", content::RegisterSandboxedProcessLauncher },
  { "TouchPoint", content::RegisterTouchPoint },
  { "WebContentsObserverAndroid", content::RegisterWebContentsObserverAndroid },
  { "WebViewStatics", content::RegisterWebViewStatics },
};

}  // namespace

namespace content {
namespace android {

bool RegisterBrowserJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content
