// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/app_non_client_frame_view_aura.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

using aura::Window;

namespace {

Window* GetChildWindowNamed(Window* window, const char* name) {
  for (size_t i = 0; i < window->children().size(); ++i) {
    Window* child = window->children()[i];
    if (child->name() == name)
      return child;
  }
  return NULL;
}

bool HasChildWindowNamed(Window* window, const char* name) {
  return GetChildWindowNamed(window, name) != NULL;
}

void MaximizeWindow(aura::Window* window) {
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
}

void MinimizeWindow(aura::Window* window) {
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
}

}  // namespace

class AppNonClientFrameViewAuraTest : public InProcessBrowserTest {
 public:
  AppNonClientFrameViewAuraTest() : InProcessBrowserTest(), app_browser_(NULL) {
  }
  virtual ~AppNonClientFrameViewAuraTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    Browser::CreateParams params = Browser::CreateParams::CreateForApp(
        Browser::TYPE_POPUP,
        std::string("Test"),
        gfx::Rect(),
        browser()->profile());
    params.initial_show_state = ui::SHOW_STATE_MAXIMIZED;
    params.app_type = Browser::APP_TYPE_HOST;
    app_browser_ = new Browser(params);
    chrome::AddBlankTab(app_browser_, true);
    app_browser_->window()->Show();
  }

  AppNonClientFrameViewAura* GetAppFrameView() const {
    BrowserView* browser_view =
        static_cast<BrowserView*>(app_browser_->window());
    BrowserFrame* frame = browser_view->frame();
    return static_cast<AppNonClientFrameViewAura*>(frame->GetFrameView());
  }

  aura::RootWindow* GetRootWindow() const {
    BrowserView* browser_view =
        static_cast<BrowserView*>(app_browser_->window());
    views::Widget* widget = browser_view->GetWidget();
    aura::Window* window =
        static_cast<aura::Window*>(widget->GetNativeWindow());
    return window->GetRootWindow();
  }

  Browser* app_browser() const { return app_browser_; }

 private:
  Browser *app_browser_;
};

// Ensure that we can click the close button when the controls are shown.
// In particular make sure that we can click it on the top pixel of the button.
IN_PROC_BROWSER_TEST_F(AppNonClientFrameViewAuraTest, ClickClose) {
  aura::RootWindow* root_window = GetRootWindow();
  aura::test::EventGenerator eg(root_window, gfx::Point(0, 1));

  // Click close button.
  eg.MoveMouseTo(root_window->bounds().width() - 1, 0);
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(app_browser()));
  eg.ClickLeftButton();
  signal.Wait();
  EXPECT_EQ(1,
            static_cast<int>(browser::GetBrowserCount(browser()->profile())));
}

// Ensure that closing a maximized app with Ctrl-W does not crash the
// application.  crbug.com/147635
IN_PROC_BROWSER_TEST_F(AppNonClientFrameViewAuraTest, KeyboardClose) {
  aura::RootWindow* root_window = GetRootWindow();
  aura::test::EventGenerator eg(root_window);

  // Base browser and app browser.
  EXPECT_EQ(2u, browser::GetBrowserCount(browser()->profile()));

  // Send Control-W.
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(app_browser()));
  eg.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  eg.ReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  signal.Wait();

  // App browser is closed.
  EXPECT_EQ(1u, browser::GetBrowserCount(browser()->profile()));
}

// Ensure that snapping left with Alt-[ closes the control window.
IN_PROC_BROWSER_TEST_F(AppNonClientFrameViewAuraTest, SnapLeftClosesControls) {
  aura::RootWindow* root_window = GetRootWindow();
  aura::test::EventGenerator eg(root_window);
  aura::Window* native_window = app_browser()->window()->GetNativeWindow();

  // Control window exists.
  EXPECT_TRUE(HasChildWindowNamed(
      native_window, AppNonClientFrameViewAura::kControlWindowName));

  // Send Alt-[
  eg.PressKey(ui::VKEY_OEM_4, ui::EF_ALT_DOWN);
  eg.ReleaseKey(ui::VKEY_OEM_4, ui::EF_ALT_DOWN);
  content::RunAllPendingInMessageLoop();

  // Control window is gone.
  EXPECT_FALSE(HasChildWindowNamed(
      native_window, AppNonClientFrameViewAura::kControlWindowName));
}

// Ensure that the controls are at the proper locations.
IN_PROC_BROWSER_TEST_F(AppNonClientFrameViewAuraTest, ControlsAtRightSide) {
  const gfx::Rect work_area = gfx::Screen::GetPrimaryDisplay().work_area();

  aura::RootWindow* root_window = GetRootWindow();
  aura::test::EventGenerator eg(root_window);
  aura::Window* native_window = app_browser()->window()->GetNativeWindow();

  // Control window exists.
  aura::Window* window = GetChildWindowNamed(
      native_window, AppNonClientFrameViewAura::kControlWindowName);

  ASSERT_TRUE(window);
  gfx::Rect rect = window->bounds();
  EXPECT_EQ(work_area.right(), rect.right());
  EXPECT_EQ(work_area.y(), rect.y());

  MinimizeWindow(native_window);
  content::RunAllPendingInMessageLoop();
  window = GetChildWindowNamed(
      native_window, AppNonClientFrameViewAura::kControlWindowName);
  EXPECT_FALSE(window);
  MaximizeWindow(native_window);
  content::RunAllPendingInMessageLoop();

  // Control window exists.
  aura::Window* window_after = GetChildWindowNamed(
      native_window, AppNonClientFrameViewAura::kControlWindowName);
  ASSERT_TRUE(window_after);
  gfx::Rect rect_after = window_after->bounds();
  EXPECT_EQ(work_area.right(), rect_after.right());
  EXPECT_EQ(work_area.y(), rect_after.y());
}
