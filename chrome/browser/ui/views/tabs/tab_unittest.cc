// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"

#include "chrome/browser/ui/tabs/tab_strip_selection_model.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using views::Widget;

class FakeTabController : public TabController {
 public:
  FakeTabController() {}
  virtual ~FakeTabController() {}

  virtual const TabStripSelectionModel& GetSelectionModel() OVERRIDE {
    return selection_model_;
  }
  virtual bool SupportsMultipleSelection() OVERRIDE { return false; }
  virtual void SelectTab(BaseTab* tab) OVERRIDE {}
  virtual void ExtendSelectionTo(BaseTab* tab) OVERRIDE {}
  virtual void ToggleSelected(BaseTab* tab) OVERRIDE {}
  virtual void AddSelectionFromAnchorTo(BaseTab* tab) OVERRIDE {}
  virtual void CloseTab(BaseTab* tab, CloseTabSource source) OVERRIDE {}
  virtual void ShowContextMenuForTab(BaseTab* tab,
                                     const gfx::Point& p) OVERRIDE {}
  virtual bool IsActiveTab(const BaseTab* tab) const OVERRIDE { return false; }
  virtual bool IsTabSelected(const BaseTab* tab) const OVERRIDE {
    return false;
  }
  virtual bool IsTabPinned(const BaseTab* tab) const OVERRIDE { return false; }
  virtual void MaybeStartDrag(
      BaseTab* tab,
      const ui::LocatedEvent& event,
      const TabStripSelectionModel& original_selection) OVERRIDE {}
  virtual void ContinueDrag(views::View* view,
                            const gfx::Point& location) OVERRIDE {}
  virtual bool EndDrag(EndDragReason reason) OVERRIDE { return false; }
  virtual BaseTab* GetTabAt(BaseTab* tab,
                            const gfx::Point& tab_in_tab_coordinates) OVERRIDE {
    return NULL;
  }
  virtual void ClickActiveTab(const BaseTab* tab) const OVERRIDE {}
  virtual void OnMouseEventInTab(views::View* source,
                                 const ui::MouseEvent& event) OVERRIDE {}
  virtual bool ShouldPaintTab(const BaseTab* tab, gfx::Rect* clip) OVERRIDE {
    return true;
  }
  virtual bool IsInstantExtendedAPIEnabled() OVERRIDE {
    return false;
  }

 private:
  TabStripSelectionModel selection_model_;

  DISALLOW_COPY_AND_ASSIGN(FakeTabController);
};

typedef views::ViewsTestBase TabTest;

TEST_F(TabTest, HitTestTopPixel) {
  Widget widget;
  Widget::InitParams params;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds.SetRect(10, 20, 300, 400);
  widget.Init(params);

  FakeTabController tab_controller;
  Tab tab(&tab_controller);
  widget.GetContentsView()->AddChildView(&tab);
  tab.SetBoundsRect(gfx::Rect(gfx::Point(0, 0), Tab::GetStandardSize()));

  // Tabs have some shadow in the top, so by default we don't hit the tab there.
  int middle_x = tab.width() / 2;
  EXPECT_FALSE(tab.HitTestPoint(gfx::Point(middle_x, 0)));

  // Tabs are slanted, so a click halfway down the left edge won't hit it.
  int middle_y = tab.height() / 2;
  EXPECT_FALSE(tab.HitTestPoint(gfx::Point(0, middle_y)));

  // If the window is maximized, however, we want clicks in the top edge to
  // select the tab.
  widget.Maximize();
  EXPECT_TRUE(tab.HitTestPoint(gfx::Point(middle_x, 0)));

  // But clicks in the area above the slanted sides should still miss.
  EXPECT_FALSE(tab.HitTestPoint(gfx::Point(0, 0)));
  EXPECT_FALSE(tab.HitTestPoint(gfx::Point(tab.width() - 1, 0)));
}
