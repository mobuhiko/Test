// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Instrumentation;
import android.content.Context;
import android.test.ActivityInstrumentationTestCase2;

import junit.framework.Assert;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.content.browser.ContentSettings;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.ui.gfx.ActivityNativeWindow;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/**
 * A base class for android_webview tests.
 */
public class AndroidWebViewTestBase
        extends ActivityInstrumentationTestCase2<AndroidWebViewTestRunnerActivity> {
    protected static int WAIT_TIMEOUT_SECONDS = 15;
    protected static final boolean NORMAL_VIEW = false;
    protected static final boolean INCOGNITO_VIEW = true;

    public AndroidWebViewTestBase() {
        super(AndroidWebViewTestRunnerActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        final Context context = getActivity();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                ContentViewCore.initChromiumBrowserProcess(
                        context, ContentView.MAX_RENDERERS_SINGLE_PROCESS);
            }
        });
    }

    /**
     * Runs a {@link Callable} on the main thread, blocking until it is
     * complete, and returns the result. Calls
     * {@link Instrumentation#waitForIdleSync()} first to help avoid certain
     * race conditions.
     *
     * @param <R> Type of result to return
     */
    public <R> R runTestOnUiThreadAndGetResult(Callable<R> callable)
            throws Throwable {
        FutureTask<R> task = new FutureTask<R>(callable);
        getInstrumentation().waitForIdleSync();
        getInstrumentation().runOnMainSync(task);
        try {
            return task.get();
        } catch (ExecutionException e) {
            // Unwrap the cause of the exception and re-throw it.
            throw e.getCause();
        }
    }

    protected void enableJavaScriptOnUiThread(final ContentViewCore contentViewCore) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                contentViewCore.getContentSettings().setJavaScriptEnabled(true);
            }
        });
    }

    /**
     * Loads url on the UI thread and blocks until onPageFinished is called.
     */
    protected void loadUrlSync(final ContentViewCore contentViewCore,
                               CallbackHelper onPageFinishedHelper,
                               final String url) throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(contentViewCore, url);
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
    }

    /**
     * Loads url on the UI thread but does not block.
     */
    protected void loadUrlAsync(final ContentViewCore contentViewCore,
                                final String url) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                contentViewCore.loadUrl(new LoadUrlParams(url));
            }
        });
    }

    /**
     * Loads data on the UI thread and blocks until onPageFinished is called.
     */
    protected void loadDataSync(final ContentViewCore contentViewCore,
                                CallbackHelper onPageFinishedHelper,
                                final String data, final String mimeType,
                                final boolean isBase64Encoded) throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataAsync(contentViewCore, data, mimeType, isBase64Encoded);
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
    }

    /**
     * Loads data on the UI thread but does not block.
     */
    protected void loadDataAsync(final ContentViewCore contentViewCore, final String data,
                                 final String mimeType, final boolean isBase64Encoded)
            throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                contentViewCore.loadUrl(LoadUrlParams.createLoadDataParams(
                        data, mimeType, isBase64Encoded));
            }
        });
    }

    protected AwTestContainerView createAwTestContainerView(final boolean incognito,
            final AwContentsClient contentsClient) {
        return createAwTestContainerView(incognito, new AwTestContainerView(getActivity()),
                contentsClient);
    }

    protected AwTestContainerView createAwTestContainerView(final boolean incognito,
            final AwTestContainerView testContainerView,
            final AwContentsClient contentsClient) {
        ContentViewCore contentViewCore = new ContentViewCore(
                getActivity(), ContentViewCore.PERSONALITY_VIEW);
        testContainerView.initialize(contentViewCore,
                new AwContents(testContainerView, testContainerView.getInternalAccessDelegate(),
                contentViewCore, contentsClient, new ActivityNativeWindow(getActivity()),
                incognito, false));
        getActivity().addView(testContainerView);
        return testContainerView;
    }

    protected AwTestContainerView createAwTestContainerViewOnMainSync(
            final AwContentsClient client) throws Exception {
        return createAwTestContainerViewOnMainSync(NORMAL_VIEW, client);
    }

    protected AwTestContainerView createAwTestContainerViewOnMainSync(
            final boolean incognito,
            final AwContentsClient client) throws Exception {
        final AtomicReference<AwTestContainerView> testContainerView =
                new AtomicReference<AwTestContainerView>();
        final Context context = getActivity();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                testContainerView.set(createAwTestContainerView(incognito, client));
            }
        });
        return testContainerView.get();
    }

    protected void destroyAwContentsOnMainSync(final AwContents contents) {
        if (contents == null) return;
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                contents.destroy();
            }
        });
    }

    protected String getTitleOnUiThread(final ContentViewCore contentViewCore) throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<String>() {
            @Override
            public String call() throws Exception {
                return contentViewCore.getTitle();
            }
        });
    }

    protected ContentSettings getContentSettingsOnUiThread(
            final ContentViewCore contentViewCore) throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<ContentSettings>() {
            @Override
            public ContentSettings call() throws Exception {
                return contentViewCore.getContentSettings();
            }
        });
    }

    /**
     * Executes the given snippet of JavaScript code within the given ContentView. Returns the
     * result of its execution in JSON format.
     */
    protected String executeJavaScriptAndWaitForResult(final ContentViewCore core,
            TestAwContentsClient viewClient, final String code) throws Throwable {
        final AtomicInteger requestId = new AtomicInteger();
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper
                onEvaluateJavaScriptResultHelper = viewClient.getOnEvaluateJavaScriptResultHelper();
        int currentCallCount = onEvaluateJavaScriptResultHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                requestId.set(core.evaluateJavaScript(code));
            }
        });
        onEvaluateJavaScriptResultHelper.waitForCallback(currentCallCount);
        Assert.assertEquals("Response ID mismatch when evaluating JavaScript.",
                requestId.get(), onEvaluateJavaScriptResultHelper.getId());
        return onEvaluateJavaScriptResultHelper.getJsonResult();
    }
}
