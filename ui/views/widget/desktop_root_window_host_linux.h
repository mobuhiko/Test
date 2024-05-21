// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_ROOT_WINDOW_HOST_LINUX_H_
#define UI_VIEWS_WIDGET_DESKTOP_ROOT_WINDOW_HOST_LINUX_H_

#include <X11/Xlib.h>

// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include "base/basictypes.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/root_window_host.h"
#include "ui/gfx/rect.h"
#include "ui/base/cursor/cursor_loader_x11.h"
#include "ui/base/x/x11_atom_cache.h"
#include "ui/views/widget/desktop_root_window_host.h"

namespace aura {
class DesktopActivationClient;
class DesktopDispatcherClient;
class FocusManager;
namespace shared {
class CompoundEventFilter;
class InputMethodEventFilter;
}
}

namespace views {
class DesktopCaptureClient;
class X11WindowEventFilter;

class DesktopRootWindowHostLinux : public DesktopRootWindowHost,
                                   public aura::RootWindowHost,
                                   public aura::client::CursorClient,
                                   public MessageLoop::Dispatcher {
 public:
  DesktopRootWindowHostLinux(
      internal::NativeWidgetDelegate* native_widget_delegate,
      const gfx::Rect& initial_bounds);
  virtual ~DesktopRootWindowHostLinux();

 private:
  // Initializes our X11 surface to draw on. This method performs all
  // initialization related to talking to the X11 server.
  void InitX11Window(const Widget::InitParams& params);

  // Creates an aura::RootWindow to contain the |content_window|, along with
  // all aura client objects that direct behavior.
  void InitRootWindow(const Widget::InitParams& params);

  // Returns true if there's an X window manager present... in most cases.  Some
  // window managers (notably, ion3) don't implement enough of ICCCM for us to
  // detect that they're there.
  bool IsWindowManagerPresent();

  // Sends a message to the x11 window manager, enabling or disabling the
  // states |state1| and |state2|.
  void SetWMSpecState(bool enabled, ::Atom state1, ::Atom state2);

  // Checks if the window manager has set a specific state.
  bool HasWMSpecProperty(const char* property) const;

  // Sets the cursor on |xwindow_| to |cursor|.  Does not check or update
  // |current_cursor_|.
  void SetCursorInternal(gfx::NativeCursor cursor);

  // Overridden from DesktopRootWindowHost:
  virtual void Init(aura::Window* content_window,
                    const Widget::InitParams& params) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void CloseNow() OVERRIDE;
  virtual aura::RootWindowHost* AsRootWindowHost() OVERRIDE;
  virtual void ShowWindowWithState(ui::WindowShowState show_state) OVERRIDE;
  virtual void ShowMaximizedWithBounds(
      const gfx::Rect& restored_bounds) OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void CenterWindow(const gfx::Size& size) OVERRIDE;
  virtual void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsInScreen() const OVERRIDE;
  virtual gfx::Rect GetClientAreaBoundsInScreen() const OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual gfx::Rect GetWorkAreaBoundsInScreen() const OVERRIDE;
  virtual void SetShape(gfx::NativeRegion native_region) OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual bool HasCapture() const OVERRIDE;
  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE;
  virtual InputMethod* CreateInputMethod() OVERRIDE;
  virtual internal::InputMethodDelegate* GetInputMethodDelegate() OVERRIDE;
  virtual void SetWindowTitle(const string16& title) OVERRIDE;
  virtual void ClearNativeFocus() OVERRIDE;
  virtual Widget::MoveLoopResult RunMoveLoop(
      const gfx::Point& drag_offset) OVERRIDE;
  virtual void EndMoveLoop() OVERRIDE;
  virtual void SetVisibilityChangedAnimationsEnabled(bool value) OVERRIDE;
  virtual bool ShouldUseNativeFrame() OVERRIDE;
  virtual void FrameTypeChanged() OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView() OVERRIDE;
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual void SetOpacity(unsigned char opacity) OVERRIDE;
  virtual void SetWindowIcons(const gfx::ImageSkia& window_icon,
                              const gfx::ImageSkia& app_icon) OVERRIDE;
  virtual void SetAccessibleName(const string16& name) OVERRIDE;
  virtual void SetAccessibleRole(ui::AccessibilityTypes::Role role) OVERRIDE;
  virtual void SetAccessibleState(ui::AccessibilityTypes::State state) OVERRIDE;
  virtual void InitModalType(ui::ModalType modal_type) OVERRIDE;
  virtual void FlashFrame(bool flash_frame) OVERRIDE;

  // Overridden from aura::RootWindowHost:
  virtual aura::RootWindow* GetRootWindow() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ToggleFullScreen() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual void ShowCursor(bool show) OVERRIDE;
  virtual bool QueryMouseLocation(gfx::Point* location_return) OVERRIDE;
  virtual bool ConfineCursorToRootWindow() OVERRIDE;
  virtual void UnConfineCursor() OVERRIDE;
  virtual void MoveCursorTo(const gfx::Point& location) OVERRIDE;
  virtual void SetFocusWhenShown(bool focus_when_shown) OVERRIDE;
  virtual bool GrabSnapshot(
      const gfx::Rect& snapshot_bounds,
      std::vector<unsigned char>* png_representation) OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void PrepareForShutdown() OVERRIDE;

  // Overridden from aura::CursorClient:
  // Note: other methods are just set on aura::RootWindowHost:
  virtual bool IsCursorVisible() const OVERRIDE;
  virtual void SetDeviceScaleFactor(float device_scale_factor) OVERRIDE;

  // Overridden from Dispatcher:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  // X11 things
  // The display and the native X window hosting the root window.
  Display* xdisplay_;
  ::Window xwindow_;

  // The native root window.
  ::Window x_root_window_;

  ui::X11AtomCache atom_cache_;

  // Is the window mapped to the screen?
  bool window_mapped_;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_;

  // True if the window should be focused when the window is shown.
  bool focus_when_shown_;

  // Whether we've been told that we hold the capture bit.
  bool has_capture_;

  // The window manager state bits.
  std::set< ::Atom> window_properties_;

  // aura:: objects that we own.
  scoped_ptr<aura::RootWindow> root_window_;
  scoped_ptr<DesktopCaptureClient> capture_client_;
  scoped_ptr<aura::DesktopActivationClient> activation_client_;
  scoped_ptr<aura::DesktopDispatcherClient> dispatcher_client_;

  // Translates custom bitmaps provided by the webpage into X11 cursors.
  ui::CursorLoaderX11 cursor_loader_;

  // Current Aura cursor.
  gfx::NativeCursor current_cursor_;

  // Is the cursor currently shown?
  bool cursor_shown_;

  // The invisible cursor.
  ::Cursor invisible_cursor_;

  // Toplevel event filter which dispatches to other event filters.
  aura::shared::CompoundEventFilter* root_window_event_filter_;

  // An event filter that pre-handles all key events to send them to an IME.
  scoped_ptr<aura::shared::InputMethodEventFilter> input_method_filter_;
  scoped_ptr<X11WindowEventFilter> x11_window_event_filter_;

  // TODO(beng): Consider providing an interface to DesktopNativeWidgetAura
  //             instead of providing this route back to Widget.
  internal::NativeWidgetDelegate* native_widget_delegate_;

  aura::RootWindowHostDelegate* root_window_host_delegate_;
  aura::Window* content_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopRootWindowHostLinux);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_ROOT_WINDOW_HOST_LINUX_H_
