// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_root_window_host_linux.h"

#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "base/message_pump_aurax11.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/user_action_client.h"
#include "ui/aura/desktop/desktop_activation_client.h"
#include "ui/aura/desktop/desktop_dispatcher_client.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/compound_event_filter.h"
#include "ui/aura/shared/input_method_event_filter.h"
#include "ui/base/touch/touch_factory.h"
#include "ui/base/x/x11_util.h"
#include "ui/views/widget/desktop_capture_client.h"
#include "ui/views/widget/desktop_layout_manager.h"
#include "ui/views/widget/x11_desktop_handler.h"
#include "ui/views/widget/x11_window_event_filter.h"

namespace views {

namespace {

// Standard Linux mouse buttons for going back and forward.
const int kBackMouseButton = 8;
const int kForwardMouseButton = 9;

// Constants that are part of EWMH.
const int k_NET_WM_STATE_ADD = 1;
const int k_NET_WM_STATE_REMOVE = 0;

const char* kAtomsToCache[] = {
  "WM_DELETE_WINDOW",
  "WM_S0",
  "_NET_WM_PID",
  "_NET_WM_PING",
  "_NET_WM_STATE",
  "_NET_WM_STATE_HIDDEN",
  "_NET_WM_STATE_MAXIMIZED_HORZ",
  "_NET_WM_STATE_MAXIMIZED_VERT",
  NULL
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, public:

DesktopRootWindowHostLinux::DesktopRootWindowHostLinux(
    internal::NativeWidgetDelegate* native_widget_delegate,
    const gfx::Rect& initial_bounds)
    : xdisplay_(base::MessagePumpAuraX11::GetDefaultXDisplay()),
      xwindow_(0),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      atom_cache_(xdisplay_, kAtomsToCache),
      window_mapped_(false),
      focus_when_shown_(false),
      has_capture_(false),
      cursor_loader_(),
      current_cursor_(ui::kCursorNull),
      cursor_shown_(true),
      native_widget_delegate_(native_widget_delegate) {
}

DesktopRootWindowHostLinux::~DesktopRootWindowHostLinux() {
  base::MessagePumpAuraX11::Current()->RemoveDispatcherForWindow(xwindow_);
  XDestroyWindow(xdisplay_, xwindow_);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, private:

void DesktopRootWindowHostLinux::InitX11Window(
    const Widget::InitParams& params) {
  unsigned long attribute_mask = CWBackPixmap;
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = None;

  if (params.type == Widget::InitParams::TYPE_MENU) {
    swa.override_redirect = True;
    attribute_mask |= CWOverrideRedirect;
  }

  xwindow_ = XCreateWindow(
      xdisplay_, x_root_window_,
      params.bounds.x(), params.bounds.y(),
      params.bounds.width(), params.bounds.height(),
      0,               // border width
      CopyFromParent,  // depth
      InputOutput,
      CopyFromParent,  // visual
      attribute_mask,
      &swa);
  base::MessagePumpAuraX11::Current()->AddDispatcherForWindow(this, xwindow_);

  // TODO(erg): Maybe need to set a ViewProp here like in RWHL::RWHL().

  long event_mask = ButtonPressMask | ButtonReleaseMask | FocusChangeMask |
                    KeyPressMask | KeyReleaseMask |
                    EnterWindowMask | LeaveWindowMask |
                    ExposureMask | VisibilityChangeMask |
                    StructureNotifyMask | PropertyChangeMask |
                    PointerMotionMask;
  XSelectInput(xdisplay_, xwindow_, event_mask);
  XFlush(xdisplay_);

  invisible_cursor_ = ui::CreateInvisibleCursor();

  // TODO(erg): We currently only request window deletion events. We also
  // should listen for activation events and anything else that GTK+ listens
  // for, and do something useful.
  ::Atom protocols[2];
  protocols[0] = atom_cache_.GetAtom("WM_DELETE_WINDOW");
  protocols[1] = atom_cache_.GetAtom("_NET_WM_PING");
  XSetWMProtocols(xdisplay_, xwindow_, protocols, 2);

  // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
  // the desktop environment.
  XSetWMProperties(xdisplay_, xwindow_, NULL, NULL, NULL, 0, NULL, NULL, NULL);

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  pid_t pid = getpid();
  XChangeProperty(xdisplay_,
                  xwindow_,
                  atom_cache_.GetAtom("_NET_WM_PID"),
                  XA_CARDINAL,
                  32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&pid), 1);
}

// TODO(erg): This method should basically be everything I need form
// RootWindowHostLinux::RootWindowHostLinux().
void DesktopRootWindowHostLinux::InitRootWindow(
    const Widget::InitParams& params) {
  bounds_ = params.bounds;

  aura::RootWindow::CreateParams rw_params(bounds_);
  rw_params.host = this;
  root_window_.reset(new aura::RootWindow(rw_params));
  root_window_->Init();
  root_window_->AddChild(content_window_);
  root_window_->SetLayoutManager(new DesktopLayoutManager(root_window_.get()));
  root_window_host_delegate_ = root_window_.get();

  // If we're given a parent, we need to mark ourselves as transient to another
  // window. Otherwise activation gets screwy.
  gfx::NativeView parent = params.GetParent();
  if (!params.child && params.GetParent())
    parent->AddTransientChild(content_window_);

  native_widget_delegate_->OnNativeWidgetCreated();

  capture_client_.reset(new DesktopCaptureClient);
  aura::client::SetCaptureClient(root_window_.get(), capture_client_.get());

  root_window_->set_focus_manager(
      X11DesktopHandler::get()->get_focus_manager());

  aura::DesktopActivationClient* activation_client =
      X11DesktopHandler::get()->get_activation_client();
  aura::client::SetActivationClient(
      root_window_.get(), activation_client);

  dispatcher_client_.reset(new aura::DesktopDispatcherClient);
  aura::client::SetDispatcherClient(root_window_.get(),
                                    dispatcher_client_.get());

  // The cursor client is a curious thing; it proxies some, but not all, calls
  // to our SetCursor() method. We require all calls to go through a route that
  // uses a CursorLoader, which includes all the ones in views:: internal.
  //
  // TODO(erg): This is a code smell. I suspect that I'm working around the
  // CursorClient's interface being plain wrong.
  aura::client::SetCursorClient(root_window_.get(), this);

  // No event filter for aura::Env. Create CompoundEvnetFilter per RootWindow.
  root_window_event_filter_ = new aura::shared::CompoundEventFilter;
  // Pass ownership of the filter to the root_window.
  root_window_->SetEventFilter(root_window_event_filter_);

  input_method_filter_.reset(new aura::shared::InputMethodEventFilter());
  input_method_filter_->SetInputMethodPropertyInRootWindow(root_window_.get());
  root_window_event_filter_->AddFilter(input_method_filter_.get());

  // TODO(erg): Unify this code once the other consumer goes away.
  x11_window_event_filter_.reset(
      new X11WindowEventFilter(root_window_.get(), activation_client));
  x11_window_event_filter_->SetUseHostWindowBorders(false);
  root_window_event_filter_->AddFilter(x11_window_event_filter_.get());
}

bool DesktopRootWindowHostLinux::IsWindowManagerPresent() {
  // Per ICCCM 2.8, "Manager Selections", window managers should take ownership
  // of WM_Sn selections (where n is a screen number).
  return XGetSelectionOwner(
      xdisplay_, atom_cache_.GetAtom("WM_S0")) != None;
}

void DesktopRootWindowHostLinux::SetWMSpecState(bool enabled,
                                                ::Atom state1,
                                                ::Atom state2) {
  XEvent xclient;
  memset(&xclient, 0, sizeof(xclient));
  xclient.type = ClientMessage;
  xclient.xclient.window = xwindow_;
  xclient.xclient.message_type = atom_cache_.GetAtom("_NET_WM_STATE");
  xclient.xclient.format = 32;
  xclient.xclient.data.l[0] =
      enabled ? k_NET_WM_STATE_ADD : k_NET_WM_STATE_REMOVE;
  xclient.xclient.data.l[1] = state1;
  xclient.xclient.data.l[2] = state2;
  xclient.xclient.data.l[3] = 1;
  xclient.xclient.data.l[4] = 0;

  XSendEvent(xdisplay_, x_root_window_, False,
             SubstructureRedirectMask | SubstructureNotifyMask,
             &xclient);
}

bool DesktopRootWindowHostLinux::HasWMSpecProperty(const char* property) const {
  return window_properties_.find(atom_cache_.GetAtom(property)) !=
      window_properties_.end();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, DesktopRootWindowHost implementation:

void DesktopRootWindowHostLinux::Init(aura::Window* content_window,
                                      const Widget::InitParams& params) {
  content_window_ = content_window;

  // TODO(erg): Check whether we *should* be building a RootWindowHost here, or
  // whether we should be proxying requests to another DRWHL.

  // In some situations, views tries to make a zero sized window, and that
  // makes us crash. Make sure we have valid sizes.
  Widget::InitParams sanitized_params = params;
  if (sanitized_params.bounds.width() == 0)
    sanitized_params.bounds.set_width(100);
  if (sanitized_params.bounds.height() == 0)
    sanitized_params.bounds.set_height(100);

  InitX11Window(sanitized_params);
  InitRootWindow(sanitized_params);

  // This needs to be the intersection of:
  // - NativeWidgetAura::InitNativeWidget()
  // - DesktopNativeWidgetHelperAura::PreInitialize()
}

void DesktopRootWindowHostLinux::Close() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::CloseNow() {
  NOTIMPLEMENTED();
}

aura::RootWindowHost* DesktopRootWindowHostLinux::AsRootWindowHost() {
  return this;
}

void DesktopRootWindowHostLinux::ShowWindowWithState(
    ui::WindowShowState show_state) {
  if (show_state != ui::SHOW_STATE_DEFAULT &&
      show_state != ui::SHOW_STATE_NORMAL) {
    // Only forwarding to Show().
    NOTIMPLEMENTED();
  }

  Show();
}

void DesktopRootWindowHostLinux::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

bool DesktopRootWindowHostLinux::IsVisible() const {
  return window_mapped_;
}

void DesktopRootWindowHostLinux::SetSize(const gfx::Size& size) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::CenterWindow(const gfx::Size& size) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  *bounds = bounds_;

  // TODO(erg): This needs a better implementation. For now, we're just pass
  // back the normal state until we keep track of this.
  *show_state = ui::SHOW_STATE_NORMAL;
}

gfx::Rect DesktopRootWindowHostLinux::GetWindowBoundsInScreen() const {
  return bounds_;
}

gfx::Rect DesktopRootWindowHostLinux::GetClientAreaBoundsInScreen() const {
  // TODO(erg): The NativeWidgetAura version returns |bounds_|, claiming its
  // needed for View::ConvertPointToScreen() to work
  // correctly. DesktopRootWindowHostWin::GetClientAreaBoundsInScreen() just
  // asks windows what it thinks the client rect is.
  //
  // Attempts to calculate the rect by asking the NonClientFrameView what it
  // thought its GetBoundsForClientView() were broke combobox drop down
  // placement.
  return bounds_;
}

gfx::Rect DesktopRootWindowHostLinux::GetRestoredBounds() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Rect DesktopRootWindowHostLinux::GetWorkAreaBoundsInScreen() const {
  std::vector<int> value;
  if (ui::GetIntArrayProperty(x_root_window_, "_NET_WORKAREA", &value) &&
      value.size() >= 4) {
    return gfx::Rect(value[0], value[1], value[2], value[3]);
  }

  // TODO(erg): As a fallback, we should return the bounds for the current
  // monitor. However, that's pretty difficult and requires futzing with XRR.
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void DesktopRootWindowHostLinux::SetShape(gfx::NativeRegion native_region) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::Activate() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::Deactivate() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

bool DesktopRootWindowHostLinux::IsActive() const {
  // TODO(erg):
  //NOTIMPLEMENTED();
  return true;
}

void DesktopRootWindowHostLinux::Maximize() {
  SetWMSpecState(true,
                 atom_cache_.GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                 atom_cache_.GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

void DesktopRootWindowHostLinux::Minimize() {
  XIconifyWindow(xdisplay_, xwindow_, 0);
}

void DesktopRootWindowHostLinux::Restore() {
  SetWMSpecState(false,
                 atom_cache_.GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                 atom_cache_.GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

bool DesktopRootWindowHostLinux::IsMaximized() const {
  return (HasWMSpecProperty("_NET_WM_STATE_MAXIMIZED_VERT") ||
          HasWMSpecProperty("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

bool DesktopRootWindowHostLinux::IsMinimized() const {
  return HasWMSpecProperty("_NET_WM_STATE_HIDDEN");
}

void DesktopRootWindowHostLinux::SetCursorInternal(gfx::NativeCursor cursor) {
  XDefineCursor(xdisplay_, xwindow_, cursor.platform());
}

bool DesktopRootWindowHostLinux::HasCapture() const {
  return has_capture_;
}

void DesktopRootWindowHostLinux::SetAlwaysOnTop(bool always_on_top) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

InputMethod* DesktopRootWindowHostLinux::CreateInputMethod() {
  // TODO(erg):
  NOTIMPLEMENTED();
  return NULL;
}

internal::InputMethodDelegate*
    DesktopRootWindowHostLinux::GetInputMethodDelegate() {
  // TODO(erg):
  NOTIMPLEMENTED();
  return NULL;
}

void DesktopRootWindowHostLinux::SetWindowTitle(const string16& title) {
  XStoreName(xdisplay_, xwindow_, UTF16ToUTF8(title).c_str());
}

void DesktopRootWindowHostLinux::ClearNativeFocus() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

Widget::MoveLoopResult DesktopRootWindowHostLinux::RunMoveLoop(
    const gfx::Point& drag_offset) {
  // TODO(erg):
  NOTIMPLEMENTED();
  return Widget::MOVE_LOOP_CANCELED;
}

void DesktopRootWindowHostLinux::EndMoveLoop() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

bool DesktopRootWindowHostLinux::ShouldUseNativeFrame() {
  return false;
}

void DesktopRootWindowHostLinux::FrameTypeChanged() {
}

NonClientFrameView* DesktopRootWindowHostLinux::CreateNonClientFrameView() {
  return NULL;
}

void DesktopRootWindowHostLinux::SetFullscreen(bool fullscreen) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

bool DesktopRootWindowHostLinux::IsFullscreen() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return false;
}

void DesktopRootWindowHostLinux::SetOpacity(unsigned char opacity) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::SetWindowIcons(
    const gfx::ImageSkia& window_icon, const gfx::ImageSkia& app_icon) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::SetAccessibleName(const string16& name) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::SetAccessibleRole(
    ui::AccessibilityTypes::Role role) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::SetAccessibleState(
    ui::AccessibilityTypes::State state) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::InitModalType(ui::ModalType modal_type) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::FlashFrame(bool flash_frame) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, aura::RootWindowHost implementation:

aura::RootWindow* DesktopRootWindowHostLinux::GetRootWindow() {
  return root_window_.get();
}

gfx::AcceleratedWidget DesktopRootWindowHostLinux::GetAcceleratedWidget() {
  return xwindow_;
}

void DesktopRootWindowHostLinux::Show() {
  if (!window_mapped_) {
    // Before we map the window, set size hints. Otherwise, some window managers
    // will ignore toplevel XMoveWindow commands.
    XSizeHints size_hints;
    size_hints.flags = PPosition;
    size_hints.x = bounds_.x();
    size_hints.y = bounds_.y();
    XSetWMNormalHints(xdisplay_, xwindow_, &size_hints);

    XMapWindow(xdisplay_, xwindow_);

    // We now block until our window is mapped. Some X11 APIs will crash and
    // burn if passed |xwindow_| before the window is mapped, and XMapWindow is
    // asynchronous.
    base::MessagePumpAuraX11::Current()->BlockUntilWindowMapped(xwindow_);
    window_mapped_ = true;
  }
}

void DesktopRootWindowHostLinux::Hide() {
  if (window_mapped_) {
    XWithdrawWindow(xdisplay_, xwindow_, 0);
    window_mapped_ = false;
  }
}

void DesktopRootWindowHostLinux::ToggleFullScreen() {
  NOTIMPLEMENTED();
}

gfx::Rect DesktopRootWindowHostLinux::GetBounds() const {
  return bounds_;
}

void DesktopRootWindowHostLinux::SetBounds(const gfx::Rect& bounds) {
  bool size_changed = bounds.size() != bounds_.size();

  if (bounds != bounds_) {
    XMoveResizeWindow(xdisplay_, xwindow_, bounds.x(), bounds.y(),
                      bounds.width(), bounds.height());
    bounds_ = bounds;
  }

  if (size_changed)
    root_window_host_delegate_->OnHostResized(bounds_.size());
  else
    root_window_host_delegate_->OnHostPaint();
}

gfx::Point DesktopRootWindowHostLinux::GetLocationOnNativeScreen() const {
  return bounds_.origin();
}

void DesktopRootWindowHostLinux::SetCapture() {
  // TODO(erg): I don't entirely understand the concept of capture in views.
  // As described in the comment on View::OnMouseCaptureLost, it seems like
  // it's what view started the current mouse press/drag. But that doesn't
  // really square with the comments in RootWindowHostLinux.
  //
  // Maybe the following is correct due to X's implicit grabs? Just keeping
  // track if we've been told that we're whatever capture is and returning it
  // when asked fixes the case where buttons pressed don't receive button
  // release events.
  has_capture_ = true;
}

void DesktopRootWindowHostLinux::ReleaseCapture() {
  has_capture_ = false;
}

void DesktopRootWindowHostLinux::SetCursor(gfx::NativeCursor cursor) {
  cursor_loader_.SetPlatformCursor(&cursor);

  if (cursor == current_cursor_)
    return;
  current_cursor_ = cursor;

  if (cursor_shown_)
    SetCursorInternal(cursor);
}

void DesktopRootWindowHostLinux::ShowCursor(bool show) {
  if (show == cursor_shown_)
    return;
  cursor_shown_ = show;
  SetCursorInternal(show ? current_cursor_ : invisible_cursor_);
}

bool DesktopRootWindowHostLinux::QueryMouseLocation(
    gfx::Point* location_return) {
  ::Window root_return, child_return;
  int root_x_return, root_y_return, win_x_return, win_y_return;
  unsigned int mask_return;
  XQueryPointer(xdisplay_,
                xwindow_,
                &root_return,
                &child_return,
                &root_x_return, &root_y_return,
                &win_x_return, &win_y_return,
                &mask_return);
  *location_return = gfx::Point(
      std::max(0, std::min(bounds_.width(), win_x_return)),
      std::max(0, std::min(bounds_.height(), win_y_return)));
  return (win_x_return >= 0 && win_x_return < bounds_.width() &&
          win_y_return >= 0 && win_y_return < bounds_.height());
}

bool DesktopRootWindowHostLinux::ConfineCursorToRootWindow() {
  NOTIMPLEMENTED();
  return false;
}

void DesktopRootWindowHostLinux::UnConfineCursor() {
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::SetFocusWhenShown(bool focus_when_shown) {
  static const char* k_NET_WM_USER_TIME = "_NET_WM_USER_TIME";
  focus_when_shown_ = focus_when_shown;
  if (IsWindowManagerPresent() && !focus_when_shown_) {
    ui::SetIntProperty(xwindow_,
                       k_NET_WM_USER_TIME,
                       k_NET_WM_USER_TIME,
                       0);
  }
}

bool DesktopRootWindowHostLinux::GrabSnapshot(
      const gfx::Rect& snapshot_bounds,
      std::vector<unsigned char>* png_representation) {
  NOTIMPLEMENTED();
  return false;
}

void DesktopRootWindowHostLinux::PostNativeEvent(
    const base::NativeEvent& native_event) {
  DCHECK(xwindow_);
  DCHECK(xdisplay_);
  XEvent xevent = *native_event;
  xevent.xany.display = xdisplay_;
  xevent.xany.window = xwindow_;

  switch (xevent.type) {
    case EnterNotify:
    case LeaveNotify:
    case MotionNotify:
    case KeyPress:
    case KeyRelease:
    case ButtonPress:
    case ButtonRelease: {
      // The fields used below are in the same place for all of events
      // above. Using xmotion from XEvent's unions to avoid repeating
      // the code.
      xevent.xmotion.root = x_root_window_;
      xevent.xmotion.time = CurrentTime;

      gfx::Point point(xevent.xmotion.x, xevent.xmotion.y);
      root_window_->ConvertPointToNativeScreen(&point);
      xevent.xmotion.x_root = point.x();
      xevent.xmotion.y_root = point.y();
    }
    default:
      break;
  }
  XSendEvent(xdisplay_, xwindow_, False, 0, &xevent);
}

void DesktopRootWindowHostLinux::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
}

void DesktopRootWindowHostLinux::PrepareForShutdown() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, aura::CursorClient implementation:

bool DesktopRootWindowHostLinux::IsCursorVisible() const {
  return cursor_shown_;
}

void DesktopRootWindowHostLinux::SetDeviceScaleFactor(
    float device_scale_factor) {
  cursor_loader_.UnloadAll();
  cursor_loader_.set_device_scale_factor(device_scale_factor);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, MessageLoop::Dispatcher implementation:

bool DesktopRootWindowHostLinux::Dispatch(const base::NativeEvent& event) {
  XEvent* xev = event;

  // May want to factor CheckXEventForConsistency(xev); into a common location
  // since it is called here.
  switch (xev->type) {
    case Expose:
      // TODO(erg): Can we only redraw the affected areas?
      root_window_host_delegate_->OnHostPaint();
      break;
    case KeyPress: {
      ui::KeyEvent keydown_event(xev, false);
      root_window_host_delegate_->OnHostKeyEvent(&keydown_event);
      break;
    }
    case KeyRelease: {
      ui::KeyEvent keyup_event(xev, false);
      root_window_host_delegate_->OnHostKeyEvent(&keyup_event);
      break;
    }
    case ButtonPress: {
      if (static_cast<int>(xev->xbutton.button) == kBackMouseButton ||
          static_cast<int>(xev->xbutton.button) == kForwardMouseButton) {
        aura::client::UserActionClient* gesture_client =
            aura::client::GetUserActionClient(root_window_.get());
        if (gesture_client) {
          gesture_client->OnUserAction(
              static_cast<int>(xev->xbutton.button) == kBackMouseButton ?
              aura::client::UserActionClient::BACK :
              aura::client::UserActionClient::FORWARD);
        }
        break;
      }
    }  // fallthrough
    case ButtonRelease: {
      ui::MouseEvent mouseev(xev);
      root_window_host_delegate_->OnHostMouseEvent(&mouseev);
      break;
    }
    case FocusOut:
      if (xev->xfocus.mode != NotifyGrab)
        root_window_host_delegate_->OnHostLostCapture();
      break;
    case ConfigureNotify: {
      DCHECK_EQ(xwindow_, xev->xconfigure.window);
      DCHECK_EQ(xwindow_, xev->xconfigure.event);
      // It's possible that the X window may be resized by some other means than
      // from within aura (e.g. the X window manager can change the size). Make
      // sure the root window size is maintained properly.
      gfx::Rect bounds(xev->xconfigure.x, xev->xconfigure.y,
                       xev->xconfigure.width, xev->xconfigure.height);
      bool size_changed = bounds_.size() != bounds.size();
      bool origin_changed = bounds_.origin() != bounds.origin();
      bounds_ = bounds;
      if (size_changed)
        root_window_host_delegate_->OnHostResized(bounds.size());
      if (origin_changed)
        root_window_host_delegate_->OnHostMoved(bounds_.origin());
      break;
    }
    case GenericEvent: {
      ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
      if (!factory->ShouldProcessXI2Event(xev))
        break;

      ui::EventType type = ui::EventTypeFromNative(xev);
      XEvent last_event;
      int num_coalesced = 0;

      switch (type) {
        // case ui::ET_TOUCH_MOVED:
        //   num_coalesced = CoalescePendingMotionEvents(xev, &last_event);
        //   if (num_coalesced > 0)
        //     xev = &last_event;
        //   // fallthrough
        // case ui::ET_TOUCH_PRESSED:
        // case ui::ET_TOUCH_RELEASED: {
        //   ui::TouchEvent touchev(xev);
        //   root_window_host_delegate_->OnHostTouchEvent(&touchev);
        //   break;
        // }
        case ui::ET_MOUSE_MOVED:
        case ui::ET_MOUSE_DRAGGED:
        case ui::ET_MOUSE_PRESSED:
        case ui::ET_MOUSE_RELEASED:
        case ui::ET_MOUSE_ENTERED:
        case ui::ET_MOUSE_EXITED: {
          if (type == ui::ET_MOUSE_MOVED || type == ui::ET_MOUSE_DRAGGED) {
            // If this is a motion event, we want to coalesce all pending motion
            // events that are at the top of the queue.
            // num_coalesced = CoalescePendingMotionEvents(xev, &last_event);
            // if (num_coalesced > 0)
            //   xev = &last_event;
          } else if (type == ui::ET_MOUSE_PRESSED) {
            XIDeviceEvent* xievent =
                static_cast<XIDeviceEvent*>(xev->xcookie.data);
            int button = xievent->detail;
            if (button == kBackMouseButton || button == kForwardMouseButton) {
              aura::client::UserActionClient* gesture_client =
                  aura::client::GetUserActionClient(
                      root_window_host_delegate_->AsRootWindow());
              if (gesture_client) {
                bool reverse_direction =
                    ui::IsTouchpadEvent(xev) && ui::IsNaturalScrollEnabled();
                gesture_client->OnUserAction(
                    (button == kBackMouseButton && !reverse_direction) ||
                    (button == kForwardMouseButton && reverse_direction) ?
                    aura::client::UserActionClient::BACK :
                    aura::client::UserActionClient::FORWARD);
              }
              break;
            }
          }
          ui::MouseEvent mouseev(xev);
          root_window_host_delegate_->OnHostMouseEvent(&mouseev);
          break;
        }
        case ui::ET_MOUSEWHEEL: {
          ui::MouseWheelEvent mouseev(xev);
          root_window_host_delegate_->OnHostMouseEvent(&mouseev);
          break;
        }
        case ui::ET_SCROLL_FLING_START:
        case ui::ET_SCROLL_FLING_CANCEL:
        case ui::ET_SCROLL: {
          ui::ScrollEvent scrollev(xev);
          root_window_host_delegate_->OnHostScrollEvent(&scrollev);
          break;
        }
        case ui::ET_UNKNOWN:
          break;
        default:
          NOTREACHED();
      }

      // If we coalesced an event we need to free its cookie.
      if (num_coalesced > 0)
        XFreeEventData(xev->xgeneric.display, &last_event.xcookie);
      break;
    }
    case MapNotify: {
      // If there's no window manager running, we need to assign the X input
      // focus to our host window.
      if (!IsWindowManagerPresent() && focus_when_shown_)
        XSetInputFocus(xdisplay_, xwindow_, RevertToNone, CurrentTime);
      break;
    }
    case ClientMessage: {
      Atom message_type = static_cast<Atom>(xev->xclient.data.l[0]);
      if (message_type == atom_cache_.GetAtom("WM_DELETE_WINDOW")) {
        // We have received a close message from the window manager.
        root_window_->OnRootWindowHostCloseRequested();
      } else if (message_type == atom_cache_.GetAtom("_NET_WM_PING")) {
        XEvent reply_event = *xev;
        reply_event.xclient.window = x_root_window_;

        XSendEvent(xdisplay_,
                   reply_event.xclient.window,
                   False,
                   SubstructureRedirectMask | SubstructureNotifyMask,
                   &reply_event);
      }
      break;
    }
    case MappingNotify: {
      switch (xev->xmapping.request) {
        case MappingModifier:
        case MappingKeyboard:
          XRefreshKeyboardMapping(&xev->xmapping);
          root_window_->OnKeyboardMappingChanged();
          break;
        case MappingPointer:
          ui::UpdateButtonMap();
          break;
        default:
          NOTIMPLEMENTED() << " Unknown request: " << xev->xmapping.request;
          break;
      }
      break;
    }
    case MotionNotify: {
      // Discard all but the most recent motion event that targets the same
      // window with unchanged state.
      XEvent last_event;
      while (XPending(xev->xany.display)) {
        XEvent next_event;
        XPeekEvent(xev->xany.display, &next_event);
        if (next_event.type == MotionNotify &&
            next_event.xmotion.window == xev->xmotion.window &&
            next_event.xmotion.subwindow == xev->xmotion.subwindow &&
            next_event.xmotion.state == xev->xmotion.state) {
          XNextEvent(xev->xany.display, &last_event);
          xev = &last_event;
        } else {
          break;
        }
      }

      ui::MouseEvent mouseev(xev);
      root_window_host_delegate_->OnHostMouseEvent(&mouseev);
      break;
    }
    case PropertyNotify: {
      // Get our new window property state if the WM has told us its changed.
      ::Atom state = atom_cache_.GetAtom("_NET_WM_STATE");

      std::vector< ::Atom> atom_list;
      if (xev->xproperty.atom == state &&
          ui::GetAtomArrayProperty(xwindow_, "_NET_WM_STATE", &atom_list)) {
        window_properties_.clear();
        std::copy(atom_list.begin(), atom_list.end(),
                  inserter(window_properties_, window_properties_.begin()));

        // Now that we have different window properties, we may need to
        // relayout the window. (The windows code doesn't need this because
        // their window change is synchronous.)
        native_widget_delegate_->AsWidget()->GetRootView()->Layout();
      }
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHost, public:

// static
DesktopRootWindowHost* DesktopRootWindowHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    const gfx::Rect& initial_bounds) {
  return new DesktopRootWindowHostLinux(native_widget_delegate, initial_bounds);
}

}  // namespace views
