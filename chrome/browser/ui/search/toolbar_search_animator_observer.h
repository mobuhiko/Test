// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_TOOLBAR_SEARCH_ANIMATOR_OBSERVER_H_
#define CHROME_BROWSER_UI_SEARCH_TOOLBAR_SEARCH_ANIMATOR_OBSERVER_H_

namespace content {
class WebContents;
}

namespace chrome {
namespace search {

// This class defines the observer interface for |ToolbarSearchAnimator|.
class ToolbarSearchAnimatorObserver {
 public:
  // Called from ui::AnimationDelegate::AnimationProgressed for fading in
  // toolbar gradient background.
  virtual void OnToolbarBackgroundAnimatorProgressed() = 0;

  // Called when toolbar gradient background animation is canceled and jumps to
  // the end state.
  // If animation is canceled because the active tab is deactivated or detached
  // or closing, |web_contents| contains the tab's contents.
  // Otherwise, if animation is canceled because of mode change, |web_contents|
  // is NULL.
  virtual void OnToolbarBackgroundAnimatorCanceled(
      content::WebContents* web_contents) = 0;

  // Called when toolbar separator visibility has changed.
  virtual void OnToolbarSeparatorChanged() = 0;

 protected:
  virtual ~ToolbarSearchAnimatorObserver() {}
};

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_TOOLBAR_SEARCH_ANIMATOR_OBSERVER_H_
