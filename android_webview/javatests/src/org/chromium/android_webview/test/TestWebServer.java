// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.util.Base64;
import android.util.Log;
import android.util.Pair;

import org.apache.http.HttpException;
import org.apache.http.HttpRequest;
import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.HttpVersion;
import org.apache.http.RequestLine;
import org.apache.http.StatusLine;
import org.apache.http.entity.ByteArrayEntity;
import org.apache.http.impl.DefaultHttpServerConnection;
import org.apache.http.impl.cookie.DateUtils;
import org.apache.http.message.BasicHttpResponse;
import org.apache.http.params.BasicHttpParams;
import org.apache.http.params.CoreProtocolPNames;
import org.apache.http.params.HttpParams;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.URI;
import java.net.URL;
import java.net.URLConnection;
import java.security.KeyManagementException;
import java.security.KeyStore;
import java.security.NoSuchAlgorithmException;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.List;
import java.util.Map;

import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.KeyManager;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSession;
import javax.net.ssl.X509TrustManager;

/**
 * Simple http test server for testing.
 *
 * Based heavily on the CTSWebServer in Android.
 */
public class TestWebServer {
    private static final String TAG = "TestWebServer";
    private static final int SERVER_PORT = 4444;
    private static final int SSL_SERVER_PORT = 4445;

    public static final String SHUTDOWN_PREFIX = "/shutdown";

    private static TestWebServer sInstance;
    private static Hashtable<Integer, String> sReasons;

    private ServerThread mServerThread;
    private String mServerUri;
    private boolean mSsl;

    private static class Response {
        final byte[] mResponseData;
        final List<Pair<String, String>> mResponseHeaders;

        Response(byte[] resposneData, List<Pair<String, String>> responseHeaders) {
            mResponseData = resposneData;
            mResponseHeaders = responseHeaders == null ?
                    new ArrayList<Pair<String, String>>() : responseHeaders;
        }
    }

    private Map<String, Response> mResponseMap = new HashMap<String, Response>();

    /**
     * Create and start a local HTTP server instance.
     * @param ssl True if the server should be using secure sockets.
     * @throws Exception
     */
    public TestWebServer(boolean ssl) throws Exception {
        if (sInstance != null) {
            // attempt to start a new instance while one is still running
            // shut down the old instance first
            sInstance.shutdown();
        }
        sInstance = this;
        mSsl = ssl;
        if (mSsl) {
            mServerUri = "https://localhost:" + SSL_SERVER_PORT;
        } else {
            mServerUri = "http://localhost:" + SERVER_PORT;
        }
        mServerThread = new ServerThread(this, mSsl);
        mServerThread.start();
    }

    /**
     * Terminate the http server.
     */
    public void shutdown() {
        try {
            // Avoid a deadlock between two threads where one is trying to call
            // close() and the other one is calling accept() by sending a GET
            // request for shutdown and having the server's one thread
            // sequentially call accept() and close().
            URL url = new URL(mServerUri + SHUTDOWN_PREFIX);
            URLConnection connection = openConnection(url);
            connection.connect();

            // Read the input from the stream to send the request.
            InputStream is = connection.getInputStream();
            is.close();

            // Block until the server thread is done shutting down.
            mServerThread.join();

        } catch (MalformedURLException e) {
            throw new IllegalStateException(e);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        } catch (IOException e) {
            throw new RuntimeException(e);
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException(e);
        } catch (KeyManagementException e) {
            throw new IllegalStateException(e);
        }

        sInstance = null;
    }

    /**
     * Sets a response to be returned when a particular request path is passed
     * in (with the option to specify additional headers).
     *
     * @param requestPath The path to respond to.
     * @param responseString The response body that will be returned.
     * @param responseHeaders Any additional headers that should be returned along with the
     *                        response (null is acceptable).
     * @return The full URL including the path that should be requested to get the expected
     *         response.
     */
    public String setResponse(
            String requestPath, String responseString,
            List<Pair<String, String>> responseHeaders) {
        mResponseMap.put(requestPath, new Response(responseString.getBytes(), responseHeaders));
        return mServerUri + requestPath;
    }

    /**
     * Sets a base64 encoded response to be returned when a particular request path is passed
     * in (with the option to specify additional headers).
     *
     * @param requestPath The path to respond to.
     * @param base64EncodedResponse The response body that is base64 encoded. The actual server
     *                              response will the decoded binary form.
     * @param responseHeaders Any additional headers that should be returned along with the
     *                        response (null is acceptable).
     * @return The full URL including the path that should be requested to get the expected
     *         response.
     */
    public String setResponseBase64(
            String requestPath, String base64EncodedResponse,
            List<Pair<String, String>> responseHeaders) {
        mResponseMap.put(requestPath,
                new Response(Base64.decode(base64EncodedResponse, Base64.DEFAULT),
                responseHeaders));
        return mServerUri + requestPath;
    }

    private URLConnection openConnection(URL url)
            throws IOException, NoSuchAlgorithmException, KeyManagementException {
        if (mSsl) {
            // Install hostname verifiers and trust managers that don't do
            // anything in order to get around the client not trusting
            // the test server due to a lack of certificates.

            HttpsURLConnection connection = (HttpsURLConnection) url.openConnection();
            connection.setHostnameVerifier(new TestHostnameVerifier());

            SSLContext context = SSLContext.getInstance("TLS");
            TestTrustManager trustManager = new TestTrustManager();
            context.init(null, new TestTrustManager[] {trustManager}, null);
            connection.setSSLSocketFactory(context.getSocketFactory());

            return connection;
        } else {
            return url.openConnection();
        }
    }

    /**
     * {@link X509TrustManager} that trusts everybody. This is used so that
     * the client calling {@link TestWebServer#shutdown()} can issue a request
     * for shutdown by blindly trusting the {@link TestWebServer}'s
     * credentials.
     */
    private static class TestTrustManager implements X509TrustManager {
        @Override
        public void checkClientTrusted(X509Certificate[] chain, String authType) {
            // Trust the TestWebServer...
        }

        @Override
        public void checkServerTrusted(X509Certificate[] chain, String authType) {
            // Trust the TestWebServer...
        }

        @Override
        public X509Certificate[] getAcceptedIssuers() {
            return null;
        }
    }

    /**
     * {@link HostnameVerifier} that verifies everybody. This permits
     * the client to trust the web server and call
     * {@link TestWebServer#shutdown()}.
     */
    private static class TestHostnameVerifier implements HostnameVerifier {
        @Override
        public boolean verify(String hostname, SSLSession session) {
            return true;
        }
    }

    /**
     * Generate a response to the given request.
     * @throws InterruptedException
     */
    private HttpResponse getResponse(HttpRequest request) throws InterruptedException {
        RequestLine requestLine = request.getRequestLine();
        HttpResponse httpResponse = null;
        Log.i(TAG, requestLine.getMethod() + ": " + requestLine.getUri());
        String uriString = requestLine.getUri();
        URI uri = URI.create(uriString);
        String path = uri.getPath();

        Response response = mResponseMap.get(path);
        if (path.equals(SHUTDOWN_PREFIX)) {
            httpResponse = createResponse(HttpStatus.SC_OK);
        } else if (response == null) {
            httpResponse = createResponse(HttpStatus.SC_NOT_FOUND);
        } else {
            httpResponse = createResponse(HttpStatus.SC_OK);
            httpResponse.setEntity(createEntity(response.mResponseData));
            for (Pair<String, String> header : response.mResponseHeaders) {
                httpResponse.addHeader(header.first, header.second);
            }
        }
        StatusLine sl = httpResponse.getStatusLine();
        Log.i(TAG, sl.getStatusCode() + "(" + sl.getReasonPhrase() + ")");
        setDateHeaders(httpResponse);
        return httpResponse;
    }

    private void setDateHeaders(HttpResponse response) {
        long time = System.currentTimeMillis();
        response.addHeader("Date", DateUtils.formatDate(new Date(), DateUtils.PATTERN_RFC1123));
    }

    /**
     * Create an empty response with the given status.
     */
    private HttpResponse createResponse(int status) {
        HttpResponse response = new BasicHttpResponse(HttpVersion.HTTP_1_0, status, null);

        if (sReasons == null) {
            sReasons = new Hashtable<Integer, String>();
            sReasons.put(HttpStatus.SC_UNAUTHORIZED, "Unauthorized");
            sReasons.put(HttpStatus.SC_NOT_FOUND, "Not Found");
            sReasons.put(HttpStatus.SC_FORBIDDEN, "Forbidden");
            sReasons.put(HttpStatus.SC_MOVED_TEMPORARILY, "Moved Temporarily");
        }
        // Fill in error reason. Avoid use of the ReasonPhraseCatalog, which is Locale-dependent.
        String reason = sReasons.get(status);

        if (reason != null) {
            StringBuffer buf = new StringBuffer("<html><head><title>");
            buf.append(reason);
            buf.append("</title></head><body>");
            buf.append(reason);
            buf.append("</body></html>");
            response.setEntity(createEntity(buf.toString().getBytes()));
        }
        return response;
    }

    /**
     * Create a string entity for the given content.
     */
    private ByteArrayEntity createEntity(byte[] data) {
        ByteArrayEntity entity = new ByteArrayEntity(data);
        entity.setContentType("text/html");
        return entity;
    }

    private static class ServerThread extends Thread {
        private TestWebServer mServer;
        private ServerSocket mSocket;
        private boolean mIsSsl;
        private boolean mIsCancelled;
        private SSLContext mSslContext;

        /**
         * Defines the keystore contents for the server, BKS version. Holds just a
         * single self-generated key. The subject name is "Test Server".
         */
        private static final String SERVER_KEYS_BKS =
            "AAAAAQAAABQDkebzoP1XwqyWKRCJEpn/t8dqIQAABDkEAAVteWtleQAAARpYl20nAAAAAQAFWC41" +
            "MDkAAAJNMIICSTCCAbKgAwIBAgIESEfU1jANBgkqhkiG9w0BAQUFADBpMQswCQYDVQQGEwJVUzET" +
            "MBEGA1UECBMKQ2FsaWZvcm5pYTEMMAoGA1UEBxMDTVRWMQ8wDQYDVQQKEwZHb29nbGUxEDAOBgNV" +
            "BAsTB0FuZHJvaWQxFDASBgNVBAMTC1Rlc3QgU2VydmVyMB4XDTA4MDYwNTExNTgxNFoXDTA4MDkw" +
            "MzExNTgxNFowaTELMAkGA1UEBhMCVVMxEzARBgNVBAgTCkNhbGlmb3JuaWExDDAKBgNVBAcTA01U" +
            "VjEPMA0GA1UEChMGR29vZ2xlMRAwDgYDVQQLEwdBbmRyb2lkMRQwEgYDVQQDEwtUZXN0IFNlcnZl" +
            "cjCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEA0LIdKaIr9/vsTq8BZlA3R+NFWRaH4lGsTAQy" +
            "DPMF9ZqEDOaL6DJuu0colSBBBQ85hQTPa9m9nyJoN3pEi1hgamqOvQIWcXBk+SOpUGRZZFXwniJV" +
            "zDKU5nE9MYgn2B9AoiH3CSuMz6HRqgVaqtppIe1jhukMc/kHVJvlKRNy9XMCAwEAATANBgkqhkiG" +
            "9w0BAQUFAAOBgQC7yBmJ9O/eWDGtSH9BH0R3dh2NdST3W9hNZ8hIa8U8klhNHbUCSSktZmZkvbPU" +
            "hse5LI3dh6RyNDuqDrbYwcqzKbFJaq/jX9kCoeb3vgbQElMRX8D2ID1vRjxwlALFISrtaN4VpWzV" +
            "yeoHPW4xldeZmoVtjn8zXNzQhLuBqX2MmAAAAqwAAAAUvkUScfw9yCSmALruURNmtBai7kQAAAZx" +
            "4Jmijxs/l8EBaleaUru6EOPioWkUAEVWCxjM/TxbGHOi2VMsQWqRr/DZ3wsDmtQgw3QTrUK666sR" +
            "MBnbqdnyCyvM1J2V1xxLXPUeRBmR2CXorYGF9Dye7NkgVdfA+9g9L/0Au6Ugn+2Cj5leoIgkgApN" +
            "vuEcZegFlNOUPVEs3SlBgUF1BY6OBM0UBHTPwGGxFBBcetcuMRbUnu65vyDG0pslT59qpaR0TMVs" +
            "P+tcheEzhyjbfM32/vwhnL9dBEgM8qMt0sqF6itNOQU/F4WGkK2Cm2v4CYEyKYw325fEhzTXosck" +
            "MhbqmcyLab8EPceWF3dweoUT76+jEZx8lV2dapR+CmczQI43tV9btsd1xiBbBHAKvymm9Ep9bPzM" +
            "J0MQi+OtURL9Lxke/70/MRueqbPeUlOaGvANTmXQD2OnW7PISwJ9lpeLfTG0LcqkoqkbtLKQLYHI" +
            "rQfV5j0j+wmvmpMxzjN3uvNajLa4zQ8l0Eok9SFaRr2RL0gN8Q2JegfOL4pUiHPsh64WWya2NB7f" +
            "V+1s65eA5ospXYsShRjo046QhGTmymwXXzdzuxu8IlnTEont6P4+J+GsWk6cldGbl20hctuUKzyx" +
            "OptjEPOKejV60iDCYGmHbCWAzQ8h5MILV82IclzNViZmzAapeeCnexhpXhWTs+xDEYSKEiG/camt" +
            "bhmZc3BcyVJrW23PktSfpBQ6D8ZxoMfF0L7V2GQMaUg+3r7ucrx82kpqotjv0xHghNIm95aBr1Qw" +
            "1gaEjsC/0wGmmBDg1dTDH+F1p9TInzr3EFuYD0YiQ7YlAHq3cPuyGoLXJ5dXYuSBfhDXJSeddUkl" +
            "k1ufZyOOcskeInQge7jzaRfmKg3U94r+spMEvb0AzDQVOKvjjo1ivxMSgFRZaDb/4qw=";

        private String PASSWORD = "android";

        /**
         * Loads a keystore from a base64-encoded String. Returns the KeyManager[]
         * for the result.
         */
        private KeyManager[] getKeyManagers() throws Exception {
            byte[] bytes = Base64.decode(SERVER_KEYS_BKS, Base64.DEFAULT);
            InputStream inputStream = new ByteArrayInputStream(bytes);

            KeyStore keyStore = KeyStore.getInstance(KeyStore.getDefaultType());
            keyStore.load(inputStream, PASSWORD.toCharArray());
            inputStream.close();

            String algorithm = KeyManagerFactory.getDefaultAlgorithm();
            KeyManagerFactory keyManagerFactory = KeyManagerFactory.getInstance(algorithm);
            keyManagerFactory.init(keyStore, PASSWORD.toCharArray());

            return keyManagerFactory.getKeyManagers();
        }


        public ServerThread(TestWebServer server, boolean ssl) throws Exception {
            super("ServerThread");
            mServer = server;
            mIsSsl = ssl;
            int retry = 3;
            while (true) {
                try {
                    if (mIsSsl) {
                        mSslContext = SSLContext.getInstance("TLS");
                        mSslContext.init(getKeyManagers(), null, null);
                        mSocket = mSslContext.getServerSocketFactory().createServerSocket(
                                SSL_SERVER_PORT);
                    } else {
                        mSocket = new ServerSocket(SERVER_PORT);
                    }
                    return;
                } catch (IOException e) {
                    Log.w(TAG, e);
                    if (--retry == 0) {
                        throw e;
                    }
                    // sleep in case server socket is still being closed
                    Thread.sleep(1000);
                }
            }
        }

        @Override
        public void run() {
            HttpParams params = new BasicHttpParams();
            params.setParameter(CoreProtocolPNames.PROTOCOL_VERSION, HttpVersion.HTTP_1_0);
            while (!mIsCancelled) {
                try {
                    Socket socket = mSocket.accept();
                    DefaultHttpServerConnection conn = new DefaultHttpServerConnection();
                    conn.bind(socket, params);

                    // Determine whether we need to shutdown early before
                    // parsing the response since conn.close() will crash
                    // for SSL requests due to UnsupportedOperationException.
                    HttpRequest request = conn.receiveRequestHeader();
                    if (isShutdownRequest(request)) {
                        mIsCancelled = true;
                    }

                    HttpResponse response = mServer.getResponse(request);
                    conn.sendResponseHeader(response);
                    conn.sendResponseEntity(response);
                    conn.close();

                } catch (IOException e) {
                    // normal during shutdown, ignore
                    Log.w(TAG, e);
                } catch (HttpException e) {
                    Log.w(TAG, e);
                } catch (InterruptedException e) {
                    Log.w(TAG, e);
                } catch (UnsupportedOperationException e) {
                    // DefaultHttpServerConnection's close() throws an
                    // UnsupportedOperationException.
                    Log.w(TAG, e);
                }
            }
            try {
                mSocket.close();
            } catch (IOException ignored) {
                // safe to ignore
            }
        }

        private boolean isShutdownRequest(HttpRequest request) {
            RequestLine requestLine = request.getRequestLine();
            String uriString = requestLine.getUri();
            URI uri = URI.create(uriString);
            String path = uri.getPath();
            return path.equals(SHUTDOWN_PREFIX);
        }
    }
}
