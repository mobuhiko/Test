// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * Holds parameters for ContentViewCore.LoadUrl. Parameters should match
 * counterparts in NavigationController::LoadURLParams, including default
 * values.
 */
@JNINamespace("content")
public class LoadUrlParams {
    // Should match NavigationController::LoadUrlType exactly. See comments
    // there for proper usage. Values are initialized in initializeConstants.
    public static int LOAD_TYPE_DEFAULT;
    public static int LOAD_TYPE_BROWSER_INITIATED_HTTP_POST;
    public static int LOAD_TYPE_DATA;

    // Should match NavigationController::UserAgentOverrideOption exactly.
    // See comments there for proper usage. Values are initialized in
    // initializeConstants.
    public static int UA_OVERRIDE_INHERIT;
    public static int UA_OVERRIDE_FALSE;
    public static int UA_OVERRIDE_TRUE;

    // Fields with counterparts in NavigationController::LoadURLParams.
    // Package private so that ContentViewCore.loadUrl can pass them down to
    // native code. Should not be accessed directly anywhere else outside of
    // this class.
    final String mUrl;
    int mLoadUrlType;
    int mTransitionType;
    int mUaOverrideOption;
    String mExtraHeaders;
    byte[] mPostData;
    String mBaseUrlForDataUrl;
    String mVirtualUrlForDataUrl;

    public LoadUrlParams(String url) {
        // Check initializeConstants was called.
        assert LOAD_TYPE_DEFAULT != LOAD_TYPE_BROWSER_INITIATED_HTTP_POST;

        mUrl = url;
        mLoadUrlType = LOAD_TYPE_DEFAULT;
        mTransitionType = ContentViewCore.PAGE_TRANSITION_LINK;
        mUaOverrideOption = UA_OVERRIDE_INHERIT;
        mPostData = null;
        mBaseUrlForDataUrl = null;
        mVirtualUrlForDataUrl = null;
    }

    /**
     * Helper method to create a LoadUrlParams object for data url.
     * @param data Data to be loaded.
     * @param mimeType Mime type of the data.
     * @param isBase64Encoded True if the data is encoded in Base 64 format.
     */
    public static LoadUrlParams createLoadDataParams(
            String data, String mimeType, boolean isBase64Encoded) {
        StringBuilder dataUrl = new StringBuilder("data:");
        dataUrl.append(mimeType);
        if (isBase64Encoded) {
            dataUrl.append(";base64");
        }
        dataUrl.append(",");
        dataUrl.append(data);

        LoadUrlParams params = new LoadUrlParams(dataUrl.toString());
        params.setLoadType(LoadUrlParams.LOAD_TYPE_DATA);
        params.setTransitionType(ContentViewCore.PAGE_TRANSITION_TYPED);
        return params;
    }

    /**
     * Set load type of this load. Defaults to LOAD_TYPE_DEFAULT.
     * @param loadType One of LOAD_TYPE static constants above.
     */
    public void setLoadType(int loadType) {
        mLoadUrlType = loadType;
    }

    /**
     * Set transition type of this load. Defaults to PAGE_TRANSITION_LINK.
     * @param transitionType One of PAGE_TRANSITION static constants in ContentView.
     */
    public void setTransitionType(int transitionType) {
        mTransitionType = transitionType;
    }

    /**
     * Set user agent override option of this load. Defaults to UA_OVERRIDE_INHERIT.
     * @param uaOption One of UA_OVERRIDE static constants above.
     */
    public void setOverrideUserAgent(int uaOption) {
        mUaOverrideOption = uaOption;
    }

    /**
     * Set extra headers for this load.
     * @param extraHeaders Extra headers seperated by "\n".
     */
    public void setExtraHeaders(String extraHeaders) {
        mExtraHeaders = extraHeaders;
    }

    /**
     * Set the post data of this load. This field is ignored unless load type is
     * LOAD_TYPE_BROWSER_INITIATED_HTTP_POST.
     * @param postData Post data for this http post load.
     */
    public void setPostData(byte[] postData) {
        mPostData = postData;
    }

    /**
     * Set the base url for data load. It is used both to resolve relative URLs
     * and when applying JavaScript's same origin policy. It is ignored unless
     * load type is LOAD_TYPE_DATA.
     * @param baseUrl The base url for this data load.
     */
    public void setBaseUrlForDataUrl(String baseUrl) {
        mBaseUrlForDataUrl = baseUrl;
    }

    /**
     * Set the virtual url for data load. It is the url displayed to the user.
     * It is ignored unless load type is LOAD_TYPE_DATA.
     * @param virtualUrl The virtual url for this data load.
     */
    public void setVirtualUrlForDataUrl(String virtualUrl) {
        mVirtualUrlForDataUrl = virtualUrl;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private static void initializeConstants(
            int load_type_default,
            int load_type_browser_initiated_http_post,
            int load_type_data,
            int ua_override_inherit,
            int ua_override_false,
            int ua_override_true) {
        LOAD_TYPE_DEFAULT = load_type_default;
        LOAD_TYPE_BROWSER_INITIATED_HTTP_POST = load_type_browser_initiated_http_post;
        LOAD_TYPE_DATA = load_type_data;
        UA_OVERRIDE_INHERIT = ua_override_inherit;
        UA_OVERRIDE_FALSE = ua_override_false;
        UA_OVERRIDE_TRUE = ua_override_true;
    }
}
