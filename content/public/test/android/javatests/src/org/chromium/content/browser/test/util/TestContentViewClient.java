// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;


import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;

/**
 * The default ContentViewClient used by ContentView tests.
 * <p>
 * Tests that need to supply their own ContentViewClient should do that
 * by extending this one.
 */
public class TestContentViewClient extends ContentViewClient {

    private OnEvaluateJavaScriptResultHelper mOnEvaluateJavaScriptResultHelper;

    public TestContentViewClient() {
        mOnEvaluateJavaScriptResultHelper = new OnEvaluateJavaScriptResultHelper();
    }

    public OnEvaluateJavaScriptResultHelper getOnEvaluateJavaScriptResultHelper() {
        return mOnEvaluateJavaScriptResultHelper;
    }

    /**
     * ATTENTION!: When overriding the following methods, be sure to call
     * the corresponding methods in the super class. Otherwise
     * {@link CallbackHelper#waitForCallback()} methods will
     * stop working!
     */
    @Override
    public void onEvaluateJavaScriptResult(int id, String jsonResult) {
        super.onEvaluateJavaScriptResult(id, jsonResult);
        mOnEvaluateJavaScriptResultHelper.notifyCalled(id, jsonResult);
    }
}
