// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCPrioritizedTexture.h"

#include "CCPrioritizedTextureManager.h"
#include "CCPriorityCalculator.h"
#include "CCProxy.h"
#include <algorithm>

using namespace std;

namespace cc {

CCPrioritizedTexture::CCPrioritizedTexture(CCPrioritizedTextureManager* manager, IntSize size, GC3Denum format)
    : m_size(size)
    , m_format(format)
    , m_bytes(0)
    , m_priority(CCPriorityCalculator::lowestPriority())
    , m_isAbovePriorityCutoff(false)
    , m_isSelfManaged(false)
    , m_backing(0)
    , m_manager(0)
{
    // m_manager is set in registerTexture() so validity can be checked.
    ASSERT(format || size.isEmpty());
    if (format)
        m_bytes = CCTexture::memorySizeBytes(size, format);
    if (manager)
        manager->registerTexture(this);
}

CCPrioritizedTexture::~CCPrioritizedTexture()
{
    if (m_manager)
        m_manager->unregisterTexture(this);
}

void CCPrioritizedTexture::setTextureManager(CCPrioritizedTextureManager* manager)
{
    if (m_manager == manager)
        return;
    if (m_manager)
        m_manager->unregisterTexture(this);
    if (manager)
        manager->registerTexture(this);
}

void CCPrioritizedTexture::setDimensions(IntSize size, GC3Denum format)
{
    if (m_format != format || m_size != size) {
        m_isAbovePriorityCutoff = false;
        m_format = format;
        m_size = size;
        m_bytes = CCTexture::memorySizeBytes(size, format);
        ASSERT(m_manager || !m_backing);
        if (m_manager)
            m_manager->returnBackingTexture(this);
    }
}

bool CCPrioritizedTexture::requestLate()
{
    if (!m_manager)
        return false;
    return m_manager->requestLate(this);
}

bool CCPrioritizedTexture::backingResourceWasEvicted() const
{
    return m_backing ? m_backing->resourceHasBeenDeleted() : false;
}

void CCPrioritizedTexture::acquireBackingTexture(CCResourceProvider* resourceProvider)
{
    ASSERT(m_isAbovePriorityCutoff);
    if (m_isAbovePriorityCutoff)
        m_manager->acquireBackingTextureIfNeeded(this, resourceProvider);
}

CCResourceProvider::ResourceId CCPrioritizedTexture::resourceId() const
{
    if (m_backing)
        return m_backing->id();
    return 0;
}

void CCPrioritizedTexture::upload(CCResourceProvider* resourceProvider,
                                  const uint8_t* image, const IntRect& imageRect,
                                  const IntRect& sourceRect, const IntSize& destOffset)
{
    ASSERT(m_isAbovePriorityCutoff);
    if (m_isAbovePriorityCutoff)
        acquireBackingTexture(resourceProvider);
    ASSERT(m_backing);
    resourceProvider->upload(resourceId(), image, imageRect, sourceRect, destOffset);
}

void CCPrioritizedTexture::link(Backing* backing)
{
    ASSERT(backing);
    ASSERT(!backing->m_owner);
    ASSERT(!m_backing);

    m_backing = backing;
    m_backing->m_owner = this;
}

void CCPrioritizedTexture::unlink()
{
    ASSERT(m_backing);
    ASSERT(m_backing->m_owner == this);

    m_backing->m_owner = 0;
    m_backing = 0;
}

void CCPrioritizedTexture::setToSelfManagedMemoryPlaceholder(size_t bytes)
{
    setDimensions(IntSize(), GraphicsContext3D::RGBA);
    setIsSelfManaged(true);
    m_bytes = bytes;
}

CCPrioritizedTexture::Backing::Backing(unsigned id, CCResourceProvider* resourceProvider, IntSize size, GC3Denum format)
    : CCTexture(id, size, format)
    , m_owner(0)
    , m_priorityAtLastPriorityUpdate(CCPriorityCalculator::lowestPriority())
    , m_ownerExistedAtLastPriorityUpdate(false)
    , m_wasAbovePriorityCutoffAtLastPriorityUpdate(false)
    , m_resourceHasBeenDeleted(false)
#ifndef NDEBUG
    , m_resourceProvider(resourceProvider)
#endif
{
}

CCPrioritizedTexture::Backing::~Backing()
{
    ASSERT(!m_owner);
    ASSERT(m_resourceHasBeenDeleted);
}

void CCPrioritizedTexture::Backing::deleteResource(CCResourceProvider* resourceProvider)
{
    ASSERT(CCProxy::isImplThread());
    ASSERT(!m_resourceHasBeenDeleted);
#ifndef NDEBUG
    ASSERT(resourceProvider == m_resourceProvider);
#endif

    resourceProvider->deleteResource(id());
    setId(0);
    m_resourceHasBeenDeleted = true;
}

bool CCPrioritizedTexture::Backing::resourceHasBeenDeleted() const
{
    ASSERT(CCProxy::isImplThread());
    return m_resourceHasBeenDeleted;
}

void CCPrioritizedTexture::Backing::updatePriority()
{
    ASSERT(CCProxy::isImplThread() && CCProxy::isMainThreadBlocked());
    if (m_owner) {
        m_ownerExistedAtLastPriorityUpdate = true;
        m_priorityAtLastPriorityUpdate = m_owner->requestPriority();
        m_wasAbovePriorityCutoffAtLastPriorityUpdate = m_owner->isAbovePriorityCutoff();
    } else {
        m_ownerExistedAtLastPriorityUpdate = false;
        m_priorityAtLastPriorityUpdate = CCPriorityCalculator::lowestPriority();
        m_wasAbovePriorityCutoffAtLastPriorityUpdate = false;
    }
}

} // namespace cc
