// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.telephony.TelephonyManager;
import android.util.Log;

import org.chromium.base.ActivityStatus;

/**
 * Used by the NetworkChangeNotifier to listens to platform changes in connectivity.
 * Note that use of this class requires that the app have the platform
 * ACCESS_NETWORK_STATE permission.
 */
public class NetworkChangeNotifierAutoDetect extends BroadcastReceiver
        implements ActivityStatus.Listener {

    /** Queries the ConnectivityManager for information about the current connection. */
    static class ConnectivityManagerDelegate {
        private ConnectivityManager mConnectivityManager;

        ConnectivityManagerDelegate(Context context) {
            if (context != null) {
                mConnectivityManager = (ConnectivityManager)
                       context.getSystemService(Context.CONNECTIVITY_SERVICE);
            }
        }

        boolean activeNetworkExists() {
            return mConnectivityManager != null &&
                    mConnectivityManager.getActiveNetworkInfo() != null;
        }

        int getNetworkType() {
            return mConnectivityManager.getActiveNetworkInfo().getType();
        }

        int getNetworkSubtype() {
            return mConnectivityManager.getActiveNetworkInfo().getSubtype();
        }
    }

    private static final String TAG = "NetworkChangeNotifierAutoDetect";

    private final NetworkConnectivityIntentFilter mIntentFilter =
            new NetworkConnectivityIntentFilter();

    private final Observer mObserver;

    private final Context mContext;
    private ConnectivityManagerDelegate mConnectivityManagerDelegate;
    private boolean mRegistered;
    private int mConnectionType;

    /**
     * Observer notified on the UI thread whenever a new connection type was detected.
     */
    public static interface Observer {
        public void onConnectionTypeChanged(int newConnectionType);
    }

    public NetworkChangeNotifierAutoDetect(Observer observer, Context context) {
        mObserver = observer;
        mContext = context;
        mConnectivityManagerDelegate = new ConnectivityManagerDelegate(context);
        mConnectionType = currentConnectionType(context);

        ActivityStatus status = ActivityStatus.getInstance();
        if (!status.isPaused()) {
          registerReceiver();
        }
        status.registerListener(this);
    }

    /**
     * Allows overriding the ConnectivityManagerDelegate for tests.
     */
    void setConnectivityManagerDelegateForTests(ConnectivityManagerDelegate delegate) {
        mConnectivityManagerDelegate = delegate;
    }

    public void destroy() {
        unregisterReceiver();
    }

    /**
     * Register a BroadcastReceiver in the given context.
     */
    private void registerReceiver() {
        if (!mRegistered) {
          mRegistered = true;
          mContext.registerReceiver(this, mIntentFilter);
        }
    }

    /**
     * Unregister the BroadcastReceiver in the given context.
     */
    private void unregisterReceiver() {
        if (mRegistered) {
           mRegistered = false;
           mContext.unregisterReceiver(this);
        }
    }

    private int currentConnectionType(Context context) {
        // Track exactly what type of connection we have.
        if (!mConnectivityManagerDelegate.activeNetworkExists()) {
            return NetworkChangeNotifier.CONNECTION_NONE;
        }

        switch (mConnectivityManagerDelegate.getNetworkType()) {
            case ConnectivityManager.TYPE_ETHERNET:
                return NetworkChangeNotifier.CONNECTION_ETHERNET;
            case ConnectivityManager.TYPE_WIFI:
                return NetworkChangeNotifier.CONNECTION_WIFI;
            case ConnectivityManager.TYPE_WIMAX:
                return NetworkChangeNotifier.CONNECTION_4G;
            case ConnectivityManager.TYPE_MOBILE:
                // Use information from TelephonyManager to classify the connection.
                switch (mConnectivityManagerDelegate.getNetworkSubtype()) {
                    case TelephonyManager.NETWORK_TYPE_GPRS:
                    case TelephonyManager.NETWORK_TYPE_EDGE:
                    case TelephonyManager.NETWORK_TYPE_CDMA:
                    case TelephonyManager.NETWORK_TYPE_1xRTT:
                    case TelephonyManager.NETWORK_TYPE_IDEN:
                        return NetworkChangeNotifier.CONNECTION_2G;
                    case TelephonyManager.NETWORK_TYPE_UMTS:
                    case TelephonyManager.NETWORK_TYPE_EVDO_0:
                    case TelephonyManager.NETWORK_TYPE_EVDO_A:
                    case TelephonyManager.NETWORK_TYPE_HSDPA:
                    case TelephonyManager.NETWORK_TYPE_HSUPA:
                    case TelephonyManager.NETWORK_TYPE_HSPA:
                    case TelephonyManager.NETWORK_TYPE_EVDO_B:
                    case TelephonyManager.NETWORK_TYPE_EHRPD:
                    case TelephonyManager.NETWORK_TYPE_HSPAP:
                        return NetworkChangeNotifier.CONNECTION_3G;
                    case TelephonyManager.NETWORK_TYPE_LTE:
                        return NetworkChangeNotifier.CONNECTION_4G;
                    default:
                        return NetworkChangeNotifier.CONNECTION_UNKNOWN;
                }
            default:
                return NetworkChangeNotifier.CONNECTION_UNKNOWN;
        }
    }

    // BroadcastReceiver
    @Override
    public void onReceive(Context context, Intent intent) {
        boolean noConnection =
                intent.getBooleanExtra(ConnectivityManager.EXTRA_NO_CONNECTIVITY, false);
        int newConnectionType = noConnection ?
                NetworkChangeNotifier.CONNECTION_NONE : currentConnectionType(context);

        if (newConnectionType != mConnectionType) {
            mConnectionType = newConnectionType;
            Log.d(TAG, "Network connectivity changed, type is: " + mConnectionType);
            mObserver.onConnectionTypeChanged(newConnectionType);
        }
    }

    // AcitivityStatus.Listener
    @Override
    public void onActivityStatusChanged(boolean isPaused) {
        if (isPaused) {
            unregisterReceiver();
        } else {
            registerReceiver();
        }
    }

    private static class NetworkConnectivityIntentFilter extends IntentFilter {
        NetworkConnectivityIntentFilter() {
                addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        }
    }
}
