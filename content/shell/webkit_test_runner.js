// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testRunner = testRunner || {};
var accessibilityController = accessibilityController || {};
var gamepadController = gamepadController || {};
var eventSender = eventSender || {};
var textInputController = textInputController || {};

(function() {
  native function GetWorkerThreadCount();
  native function NotifyDone();
  native function OverridePreference();
  native function SetDumpAsText();
  native function SetDumpChildFramesAsText();
  native function SetPrinting();
  native function SetShouldStayOnPageAfterHandlingBeforeUnload();
  native function SetWaitUntilDone();

  native function NotImplemented();

  var DefaultHandler = function(name) {
    var handler = {
      get: function(receiver, property) {
        NotImplemented(name, property);
        return function() {}
      }
    }
    return Proxy.create(handler);
  }

  var TestRunner = function() {
    Object.defineProperty(this,
                          "workerThreadCount",
                          {value: GetWorkerThreadCount});
    Object.defineProperty(this,
                          "overridePreference",
                          {value: OverridePreference});
    Object.defineProperty(this, "notifyDone", {value: NotifyDone});
    Object.defineProperty(this, "dumpAsText", {value: SetDumpAsText});
    Object.defineProperty(this,
                          "dumpChildFramesAsText",
                          {value: SetDumpChildFramesAsText});
    Object.defineProperty(this, "setPrinting", {value: SetPrinting});
    Object.defineProperty(
        this,
        "setShouldStayOnPageAfterHandlingBeforeUnload",
        {value: SetShouldStayOnPageAfterHandlingBeforeUnload});
    Object.defineProperty(this, "waitUntilDone", {value: SetWaitUntilDone});
  }
  TestRunner.prototype = DefaultHandler("testRunner");
  testRunner = new TestRunner();

  var AccessibilityController = function() {}
  AccessibilityController.prototype = DefaultHandler("accessibilityController");
  accessibilityController = new AccessibilityController();

  var GamepadController = function() {}
  GamepadController.prototype = DefaultHandler("gamepadController");
  gamepadController = new GamepadController();

  var EventSender = function() {}
  EventSender.prototype = DefaultHandler("eventSender");
  eventSender = new EventSender();

  var TextInputController = function() {}
  TextInputController.prototype = DefaultHandler("textInputController");
  textInputController = new TextInputController();
})();
