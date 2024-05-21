// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest_helper.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_view_host.h"

namespace content {

BrowserPluginGuestHelper::BrowserPluginGuestHelper(
    BrowserPluginGuest* guest,
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      guest_(guest) {
}

BrowserPluginGuestHelper::~BrowserPluginGuestHelper() {
}

bool BrowserPluginGuestHelper::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginGuestHelper, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateRect, OnUpdateRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HandleInputEvent_ACK, OnHandleInputEventAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor, OnSetCursor)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginGuestHelper::OnUpdateRect(
    const ViewHostMsg_UpdateRect_Params& params) {
  guest_->UpdateRect(render_view_host(), params);
}

void BrowserPluginGuestHelper::OnHandleInputEventAck(
    WebKit::WebInputEvent::Type event_type,
    bool processed) {
  guest_->HandleInputEventAck(render_view_host(), processed);
}

void BrowserPluginGuestHelper::OnTakeFocus(bool reverse) {
  guest_->ViewTakeFocus(reverse);
}

void BrowserPluginGuestHelper::OnShowWidget(int route_id,
                                            const gfx::Rect& initial_pos) {
  guest_->ShowWidget(render_view_host(), route_id, initial_pos);
}

void BrowserPluginGuestHelper::OnSetCursor(const WebCursor& cursor) {
  guest_->SetCursor(cursor);
}

}  // namespace content
