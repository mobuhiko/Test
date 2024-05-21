// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCSingleThreadProxy.h"

#include "CCDrawQuad.h"
#include "CCGraphicsContext.h"
#include "CCLayerTreeHost.h"
#include "CCTextureUpdateController.h"
#include "CCTimer.h"
#include "TraceEvent.h"
#include <wtf/CurrentTime.h>

using namespace WTF;

namespace cc {

PassOwnPtr<CCProxy> CCSingleThreadProxy::create(CCLayerTreeHost* layerTreeHost)
{
    return adoptPtr(new CCSingleThreadProxy(layerTreeHost));
}

CCSingleThreadProxy::CCSingleThreadProxy(CCLayerTreeHost* layerTreeHost)
    : m_layerTreeHost(layerTreeHost)
    , m_contextLost(false)
    , m_rendererInitialized(false)
    , m_nextFrameIsNewlyCommittedFrame(false)
{
    TRACE_EVENT0("cc", "CCSingleThreadProxy::CCSingleThreadProxy");
    ASSERT(CCProxy::isMainThread());
}

void CCSingleThreadProxy::start()
{
    DebugScopedSetImplThread impl;
    m_layerTreeHostImpl = m_layerTreeHost->createLayerTreeHostImpl(this);
}

CCSingleThreadProxy::~CCSingleThreadProxy()
{
    TRACE_EVENT0("cc", "CCSingleThreadProxy::~CCSingleThreadProxy");
    ASSERT(CCProxy::isMainThread());
    ASSERT(!m_layerTreeHostImpl && !m_layerTreeHost); // make sure stop() got called.
}

bool CCSingleThreadProxy::compositeAndReadback(void *pixels, const IntRect& rect)
{
    TRACE_EVENT0("cc", "CCSingleThreadProxy::compositeAndReadback");
    ASSERT(CCProxy::isMainThread());

    if (!commitAndComposite())
        return false;

    m_layerTreeHostImpl->readback(pixels, rect);

    if (m_layerTreeHostImpl->isContextLost())
        return false;

    m_layerTreeHostImpl->swapBuffers();
    didSwapFrame();

    return true;
}

void CCSingleThreadProxy::startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double duration)
{
    m_layerTreeHostImpl->startPageScaleAnimation(targetPosition, useAnchor, scale, monotonicallyIncreasingTime(), duration);
}

void CCSingleThreadProxy::finishAllRendering()
{
    ASSERT(CCProxy::isMainThread());
    {
        DebugScopedSetImplThread impl;
        m_layerTreeHostImpl->finishAllRendering();
    }
}

bool CCSingleThreadProxy::isStarted() const
{
    ASSERT(CCProxy::isMainThread());
    return m_layerTreeHostImpl;
}

bool CCSingleThreadProxy::initializeContext()
{
    ASSERT(CCProxy::isMainThread());
    OwnPtr<CCGraphicsContext> context = m_layerTreeHost->createContext();
    if (!context)
        return false;
    m_contextBeforeInitialization = context.release();
    return true;
}

void CCSingleThreadProxy::setSurfaceReady()
{
    // Scheduling is controlled by the embedder in the single thread case, so nothing to do.
}

void CCSingleThreadProxy::setVisible(bool visible)
{
    DebugScopedSetImplThread impl;
    m_layerTreeHostImpl->setVisible(visible);
}

bool CCSingleThreadProxy::initializeRenderer()
{
    ASSERT(CCProxy::isMainThread());
    ASSERT(m_contextBeforeInitialization);
    {
        DebugScopedSetImplThread impl;
        bool ok = m_layerTreeHostImpl->initializeRenderer(m_contextBeforeInitialization.release());
        if (ok) {
            m_rendererInitialized = true;
            m_RendererCapabilitiesForMainThread = m_layerTreeHostImpl->rendererCapabilities();
        }

        return ok;
    }
}

bool CCSingleThreadProxy::recreateContext()
{
    TRACE_EVENT0("cc", "CCSingleThreadProxy::recreateContext");
    ASSERT(CCProxy::isMainThread());
    ASSERT(m_contextLost);

    OwnPtr<CCGraphicsContext> context = m_layerTreeHost->createContext();
    if (!context)
        return false;

    bool initialized;
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;
        DebugScopedSetImplThread impl;
        if (!m_layerTreeHostImpl->contentsTexturesPurged())
            m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->resourceProvider());
        initialized = m_layerTreeHostImpl->initializeRenderer(context.release());
        if (initialized) {
            m_RendererCapabilitiesForMainThread = m_layerTreeHostImpl->rendererCapabilities();
        }
    }

    if (initialized)
        m_contextLost = false;

    return initialized;
}

void CCSingleThreadProxy::implSideRenderingStats(CCRenderingStats& stats)
{
    m_layerTreeHostImpl->renderingStats(stats);
}

const RendererCapabilities& CCSingleThreadProxy::rendererCapabilities() const
{
    ASSERT(m_rendererInitialized);
    // Note: this gets called during the commit by the "impl" thread
    return m_RendererCapabilitiesForMainThread;
}

void CCSingleThreadProxy::loseContext()
{
    ASSERT(CCProxy::isMainThread());
    m_layerTreeHost->didLoseContext();
    m_contextLost = true;
}

void CCSingleThreadProxy::setNeedsAnimate()
{
    // CCThread-only feature
    ASSERT_NOT_REACHED();
}

void CCSingleThreadProxy::doCommit(PassOwnPtr<CCTextureUpdateQueue> queue)
{
    ASSERT(CCProxy::isMainThread());
    // Commit immediately
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;
        DebugScopedSetImplThread impl;

        m_layerTreeHostImpl->beginCommit();

        m_layerTreeHost->beginCommitOnImplThread(m_layerTreeHostImpl.get());

        OwnPtr<CCTextureUpdateController> updateController =
            CCTextureUpdateController::create(
                NULL,
                CCProxy::mainThread(),
                queue,
                m_layerTreeHostImpl->resourceProvider(),
                m_layerTreeHostImpl->resourceProvider()->textureUploader());
        updateController->finalize();

        m_layerTreeHost->finishCommitOnImplThread(m_layerTreeHostImpl.get());

        m_layerTreeHostImpl->commitComplete();

#if !ASSERT_DISABLED
        // In the single-threaded case, the scroll deltas should never be
        // touched on the impl layer tree.
        OwnPtr<CCScrollAndScaleSet> scrollInfo = m_layerTreeHostImpl->processScrollDeltas();
        ASSERT(!scrollInfo->scrolls.size());
#endif
    }
    m_layerTreeHost->commitComplete();
    m_nextFrameIsNewlyCommittedFrame = true;
}

void CCSingleThreadProxy::setNeedsCommit()
{
    ASSERT(CCProxy::isMainThread());
    m_layerTreeHost->scheduleComposite();
}

void CCSingleThreadProxy::setNeedsRedraw()
{
    // FIXME: Once we move render_widget scheduling into this class, we can
    // treat redraw requests more efficiently than commitAndRedraw requests.
    m_layerTreeHostImpl->setFullRootLayerDamage();
    setNeedsCommit();
}

bool CCSingleThreadProxy::commitRequested() const
{
    return false;
}

void CCSingleThreadProxy::didAddAnimation()
{
}

size_t CCSingleThreadProxy::maxPartialTextureUpdates() const
{
    return std::numeric_limits<size_t>::max();
}

void CCSingleThreadProxy::stop()
{
    TRACE_EVENT0("cc", "CCSingleThreadProxy::stop");
    ASSERT(CCProxy::isMainThread());
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;
        DebugScopedSetImplThread impl;

        if (!m_layerTreeHostImpl->contentsTexturesPurged())
            m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->resourceProvider());
        m_layerTreeHostImpl.clear();
    }
    m_layerTreeHost = 0;
}

void CCSingleThreadProxy::setNeedsRedrawOnImplThread()
{
    m_layerTreeHost->scheduleComposite();
}

void CCSingleThreadProxy::setNeedsCommitOnImplThread()
{
    m_layerTreeHost->scheduleComposite();
}

void CCSingleThreadProxy::postAnimationEventsToMainThreadOnImplThread(PassOwnPtr<CCAnimationEventsVector> events, double wallClockTime)
{
    ASSERT(CCProxy::isImplThread());
    DebugScopedSetMainThread main;
    m_layerTreeHost->setAnimationEvents(events, wallClockTime);
}

void CCSingleThreadProxy::releaseContentsTexturesOnImplThread()
{
    ASSERT(isImplThread());
    m_layerTreeHost->reduceContentsTexturesMemoryOnImplThread(0, m_layerTreeHostImpl->resourceProvider());
}

// Called by the legacy scheduling path (e.g. where render_widget does the scheduling)
void CCSingleThreadProxy::compositeImmediately()
{
    if (commitAndComposite()) {
        m_layerTreeHostImpl->swapBuffers();
        didSwapFrame();
    }
}

void CCSingleThreadProxy::forceSerializeOnSwapBuffers()
{
    {
        DebugScopedSetImplThread impl;
        if (m_rendererInitialized)
            m_layerTreeHostImpl->renderer()->doNoOp();
    }
}

void CCSingleThreadProxy::onSwapBuffersCompleteOnImplThread()
{
    ASSERT_NOT_REACHED();
}

bool CCSingleThreadProxy::commitAndComposite()
{
    ASSERT(CCProxy::isMainThread());

    if (!m_layerTreeHost->initializeRendererIfNeeded())
        return false;

    // Unlink any texture backings that were deleted
    CCPrioritizedTextureManager::BackingVector evictedContentsTexturesBackings;
    {
        DebugScopedSetImplThread implThread;
        m_layerTreeHost->getEvictedContentTexturesBackings(evictedContentsTexturesBackings);
    }
    m_layerTreeHost->unlinkEvictedContentTexturesBackings(evictedContentsTexturesBackings);
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implAndMainBlocked;
        m_layerTreeHost->deleteEvictedContentTexturesBackings();
    }

    OwnPtr<CCTextureUpdateQueue> queue = adoptPtr(new CCTextureUpdateQueue);
    m_layerTreeHost->updateLayers(*(queue.get()), m_layerTreeHostImpl->memoryAllocationLimitBytes());

    if (m_layerTreeHostImpl->contentsTexturesPurged())
        m_layerTreeHostImpl->resetContentsTexturesPurged();

    m_layerTreeHost->willCommit();
    doCommit(queue.release());
    bool result = doComposite();
    m_layerTreeHost->didBeginFrame();
    return result;
}

bool CCSingleThreadProxy::doComposite()
{
    ASSERT(!m_contextLost);
    {
        DebugScopedSetImplThread impl;

        if (!m_layerTreeHostImpl->visible())
            return false;

        double monotonicTime = monotonicallyIncreasingTime();
        double wallClockTime = currentTime();
        m_layerTreeHostImpl->animate(monotonicTime, wallClockTime);

        // We guard prepareToDraw() with canDraw() because it always returns a valid frame, so can only
        // be used when such a frame is possible. Since drawLayers() depends on the result of
        // prepareToDraw(), it is guarded on canDraw() as well.
        if (!m_layerTreeHostImpl->canDraw())
            return false;

        CCLayerTreeHostImpl::FrameData frame;
        m_layerTreeHostImpl->prepareToDraw(frame);
        m_layerTreeHostImpl->drawLayers(frame);
        m_layerTreeHostImpl->didDrawAllLayers(frame);
    }

    if (m_layerTreeHostImpl->isContextLost()) {
        m_contextLost = true;
        m_layerTreeHost->didLoseContext();
        return false;
    }

    return true;
}

void CCSingleThreadProxy::didSwapFrame()
{
    if (m_nextFrameIsNewlyCommittedFrame) {
        m_nextFrameIsNewlyCommittedFrame = false;
        m_layerTreeHost->didCommitAndDrawFrame();
    }
}

}
