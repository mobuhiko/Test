// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// A Widget observer class used in the tests below to observe bubbles closing.
class TestWidgetObserver : public WidgetObserver {
 public:
  explicit TestWidgetObserver(Widget* widget);
  virtual ~TestWidgetObserver();

  // WidgetObserver overrides:
  virtual void OnWidgetClosing(Widget* widget) OVERRIDE;

  bool widget_closed() const { return widget_ == NULL; }

 private:
  Widget* widget_;
};

TestWidgetObserver::TestWidgetObserver(Widget* widget)
    : widget_(widget) {
  widget_->AddObserver(this);
}

TestWidgetObserver::~TestWidgetObserver() {
  if (widget_)
    widget_->RemoveObserver(this);
}

void TestWidgetObserver::OnWidgetClosing(Widget* widget) {
  DCHECK_EQ(widget_, widget);
  widget_ = NULL;
}

}  // namespace

typedef ViewsTestBase BubbleDelegateTest;

TEST_F(BubbleDelegateTest, CreateDelegate) {
  BubbleDelegateView* bubble_delegate =
      new BubbleDelegateView(NULL, BubbleBorder::NONE);
  bubble_delegate->set_color(SK_ColorGREEN);
  Widget* bubble_widget(
      BubbleDelegateView::CreateBubble(bubble_delegate));
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  TestWidgetObserver bubble_observer(bubble_widget);
  EXPECT_FALSE(bubble_observer.widget_closed());

  BubbleBorder* border =
      bubble_delegate->GetBubbleFrameView()->bubble_border();
  EXPECT_EQ(bubble_delegate->arrow_location(), border->arrow_location());
  EXPECT_EQ(bubble_delegate->color(), border->background_color());

  bubble_widget->CloseNow();
  RunPendingMessages();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(BubbleDelegateTest, CloseAnchorWidget) {
  // Create the anchor widget.
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  scoped_ptr<Widget> anchor_widget(new Widget);
  anchor_widget->Init(params);
  anchor_widget->Show();

  BubbleDelegateView* bubble_delegate = new BubbleDelegateView(
      anchor_widget->GetContentsView(), BubbleBorder::NONE);
  // Preventing close on deactivate should not prevent closing with the anchor.
  bubble_delegate->set_close_on_deactivate(false);
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());
  TestWidgetObserver bubble_observer(bubble_widget);
  EXPECT_FALSE(bubble_observer.widget_closed());

  bubble_widget->Show();
  RunPendingMessages();
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());
  EXPECT_FALSE(bubble_observer.widget_closed());

#if defined(USE_AURA)
  // TODO(msw): Remove activation hack to prevent bookkeeping errors in:
  //            aura::test::TestActivationClient::OnWindowDestroyed().
  scoped_ptr<Widget> smoke_and_mirrors_widget(new Widget);
  smoke_and_mirrors_widget->Init(params);
  smoke_and_mirrors_widget->Show();
  EXPECT_FALSE(bubble_observer.widget_closed());
#endif

  // Ensure that closing the anchor widget also closes the bubble itself.
  anchor_widget->CloseNow();
  RunPendingMessages();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(BubbleDelegateTest, ResetAnchorWidget) {
  // Create the anchor and parent widgets.
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  scoped_ptr<Widget> anchor_widget(new Widget);
  anchor_widget->Init(params);
  anchor_widget->Show();
  scoped_ptr<Widget> parent_widget(new Widget);
  parent_widget->Init(params);
  parent_widget->Show();

  // Make sure the bubble widget is parented to a widget other than the anchor
  // widget so that closing the anchor widget does not close the bubble widget.
  BubbleDelegateView* bubble_delegate = new BubbleDelegateView(
      anchor_widget->GetContentsView(), BubbleBorder::NONE);
  bubble_delegate->set_parent_window(parent_widget->GetNativeView());
  // Preventing close on deactivate should not prevent closing with the parent.
  bubble_delegate->set_close_on_deactivate(false);
  Widget* bubble_widget = BubbleDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());
  TestWidgetObserver bubble_observer(bubble_widget);
  EXPECT_FALSE(bubble_observer.widget_closed());

  // Showing and hiding the bubble widget should have no effect on its anchor.
  bubble_widget->Show();
  RunPendingMessages();
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());
  bubble_widget->Hide();
  RunPendingMessages();
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());

  // Ensure that closing the anchor widget clears the bubble's reference to that
  // anchor widget, but the bubble itself does not close.
  anchor_widget->CloseNow();
  RunPendingMessages();
  EXPECT_NE(anchor_widget.get(), bubble_delegate->anchor_widget());
  EXPECT_FALSE(bubble_observer.widget_closed());

#if defined(USE_AURA)
  // TODO(msw): Remove activation hack to prevent bookkeeping errors in:
  //            aura::test::TestActivationClient::OnWindowDestroyed().
  scoped_ptr<Widget> smoke_and_mirrors_widget(new Widget);
  smoke_and_mirrors_widget->Init(params);
  smoke_and_mirrors_widget->Show();
  EXPECT_FALSE(bubble_observer.widget_closed());
#endif

  // Ensure that closing the parent widget also closes the bubble itself.
  parent_widget->CloseNow();
  RunPendingMessages();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

}  // namespace views
