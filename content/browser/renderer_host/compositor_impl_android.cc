// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_impl_android.h"

#include <android/native_window_jni.h>

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorSupport.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorOutputSurface.h"

namespace {

static bool g_initialized = false;

// Adapts a pure WebGraphicsContext3D into a WebCompositorOutputSurface.
class WebGraphicsContextToOutputSurfaceAdapter :
    public WebKit::WebCompositorOutputSurface {
public:
    explicit WebGraphicsContextToOutputSurfaceAdapter(
        WebKit::WebGraphicsContext3D* context)
        : m_context3D(context)
        , m_client(0)
    {
    }

    virtual bool bindToClient(
        WebKit::WebCompositorOutputSurfaceClient* client) OVERRIDE
    {
        DCHECK(client);
        if (!m_context3D->makeContextCurrent())
            return false;
        m_client = client;
        return true;
    }

    virtual const Capabilities& capabilities() const OVERRIDE
    {
        return m_capabilities;
    }

    virtual WebKit::WebGraphicsContext3D* context3D() const OVERRIDE
    {
        return m_context3D.get();
    }

    virtual void sendFrameToParentCompositor(
        const WebKit::WebCompositorFrame&) OVERRIDE
    {
    }

private:
    scoped_ptr<WebKit::WebGraphicsContext3D> m_context3D;
    Capabilities m_capabilities;
    WebKit::WebCompositorOutputSurfaceClient* m_client;
};

} // anonymous namespace

namespace content {

// static
Compositor* Compositor::Create() {
  return new CompositorImpl();
}

// static
void Compositor::Initialize() {
  g_initialized = true;
  WebKit::Platform::current()->compositorSupport()->initialize(NULL);
}

// static
bool CompositorImpl::IsInitialized() {
  return g_initialized;
}

CompositorImpl::CompositorImpl()
    : window_(NULL),
      surface_id_(0) {
  root_layer_.reset(
      WebKit::Platform::current()->compositorSupport()->createLayer());
}

CompositorImpl::~CompositorImpl() {
}

void CompositorImpl::OnSurfaceUpdated(
    const SurfacePresentedCallback& callback) {
  if (host_.get())
    host_->composite();
  // TODO(sievers): Let RWHV do this
  uint32 sync_point =
      ImageTransportFactoryAndroid::GetInstance()->InsertSyncPoint();
  callback.Run(sync_point);
}

void CompositorImpl::SetRootLayer(WebKit::WebLayer* root_layer) {
  root_layer_->removeAllChildren();
  root_layer_->addChild(root_layer);
}

void CompositorImpl::SetWindowSurface(ANativeWindow* window) {
  GpuSurfaceTracker* tracker = GpuSurfaceTracker::Get();

  if (window_) {
    tracker->RemoveSurface(surface_id_);
    ANativeWindow_release(window_);
    window_ = NULL;
    surface_id_ = 0;
    size_ = gfx::Size();
  }

  if (window) {
    window_ = window;
    ANativeWindow_acquire(window);
    surface_id_ = tracker->AddSurfaceForNativeWidget(window);
    tracker->SetSurfaceHandle(
        surface_id_,
        gfx::GLSurfaceHandle(gfx::kDummyPluginWindow, false));

    WebKit::WebLayerTreeView::Settings settings;
    settings.refreshRate = 60.0;
    WebKit::WebCompositorSupport* compositor_support =
        WebKit::Platform::current()->compositorSupport();
    host_.reset(
        compositor_support->createLayerTreeView(this, *root_layer_, settings));
    host_->setVisible(true);
    host_->setSurfaceReady();
  }
}

void CompositorImpl::SetWindowBounds(const gfx::Size& size) {
  if (size_ == size)
    return;

  size_ = size;
  host_->setViewportSize(size);
  root_layer_->setBounds(size);
}

void CompositorImpl::updateAnimations(double frameBeginTime) {
}

void CompositorImpl::layout() {
}

void CompositorImpl::applyScrollAndScale(const WebKit::WebSize& scrollDelta,
                                     float scaleFactor) {
}

WebKit::WebCompositorOutputSurface* CompositorImpl::createOutputSurface() {
  WebKit::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  GpuChannelHostFactory* factory = BrowserGpuChannelHostFactory::instance();
  GURL url("chrome://gpu/Compositor::createContext3D");
  base::WeakPtr<WebGraphicsContext3DSwapBuffersClient> swap_client;
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      new WebGraphicsContext3DCommandBufferImpl(
          surface_id_,
          url,
          factory,
          swap_client));
  if (!context->Initialize(
      attrs,
      false,
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE)) {
    LOG(ERROR) << "Failed to create 3D context for compositor.";
    return NULL;
  }

  return new WebGraphicsContextToOutputSurfaceAdapter(context.release());
}

void CompositorImpl::didRecreateOutputSurface(bool success) {
}

void CompositorImpl::didCommit() {
}

void CompositorImpl::didCommitAndDrawFrame() {
}

void CompositorImpl::didCompleteSwapBuffers() {
}

void CompositorImpl::scheduleComposite() {
}

} // namespace content
