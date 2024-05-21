// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_SHEET_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/memory/scoped_nsobject.h"

// This class manages multiple tab modal sheets for a single parent window. Each
// tab can have a single sheet and only the active tab's sheet will be visible.
// A tab in this case is the |parentView| passed to |-showSheet:forParentView:|.
@interface ConstrainedWindowSheetController : NSObject <NSAnimationDelegate> {
 @private
  scoped_nsobject<NSMutableArray> sheets_;
  scoped_nsobject<NSWindow> parentWindow_;
  scoped_nsobject<NSView> activeView_;
}

// Returns a sheet controller for |parentWindow|. If a sheet controller does not
// exist yet then one will be created.
+ (ConstrainedWindowSheetController*)
    controllerForParentWindow:(NSWindow*)parentWindow;

// Find a controller that's managing the given sheet. If no such controller
// exists then nil is returned.
+ (ConstrainedWindowSheetController*)controllerForSheet:(NSWindow*)sheet;

// Shows the given sheet over |parentView|. If |parentView| is not the active
// view then the sheet is not shown until the |parentView| becomes active.
- (void)showSheet:(NSWindow*)sheet
    forParentView:(NSView*)parentView;

// Closes the given sheet. If the parent view of the sheet is currently active
// then an asynchronous animation will be run and the sheet will be closed
// at the end of the animation.
- (void)closeSheet:(NSWindow*)sheet;

// Make |parentView| the current active view. If |parentView| has an attached
// sheet then the sheet is made visible.
- (void)parentViewDidBecomeActive:(NSView*)parentView;

// Gets the number of sheets attached to the controller's window.
- (int)sheetCount;

@end

@interface ConstrainedWindowSheetController (TestAPI)

// Testing only API. End any pending animation for the given sheet.
- (void)endAnimationForSheet:(NSWindow*)sheet;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_SHEET_CONTROLLER_H_
