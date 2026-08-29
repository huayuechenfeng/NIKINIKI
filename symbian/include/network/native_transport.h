#ifndef WILIWILI_SYMBIAN_NATIVE_TRANSPORT_H
#define WILIWILI_SYMBIAN_NATIVE_TRANSPORT_H

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QTime>

namespace wiliwili {

class NativeTransportPrivate;

class NativeTransport
{
public:
    // Most Symbian requests use the compact client profile.  The WBI web
    // search endpoint is different: it validates the web search request
    // context and can otherwise return a successful-looking v_voucher body
    // instead of a result set.  Keep that compatibility header set scoped to
    // search so playback and the other native API calls do not change.
    enum RequestProfile {
        DefaultRequestProfile = 0,
        WebSearchRequestProfile
    };

    enum State {
        Idle,
        Running,
        Succeeded,
        Failed,
        TimedOut,
        ResponseTooLarge
    };

    struct Result {
        Result()
            : httpStatus(0),
              networkError(0),
              elapsedMilliseconds(0)
        {
        }

        int httpStatus;
        int networkError;
        int elapsedMilliseconds;
        QByteArray body;
        QByteArray setCookieHeader;
        QString errorText;
    };

    NativeTransport();
    ~NativeTransport();

    bool startGet(
        const QByteArray &url,
        int timeoutMilliseconds,
        int maximumResponseBytes,
        const QByteArray &cookieHeader = QByteArray(),
        RequestProfile profile = DefaultRequestProfile);
    bool startPost(
        const QByteArray &url,
        const QByteArray &formBody,
        int timeoutMilliseconds,
        int maximumResponseBytes,
        const QByteArray &cookieHeader = QByteArray());
    bool poll();
    void cancel();

    State state() const;
    const Result &result() const;

private:
    NativeTransport(const NativeTransport &);
    NativeTransport &operator=(const NativeTransport &);
    friend class NativeTransportPrivate;

    void setHttpStatus(int status);
    void setResponseCookieHeader(const QByteArray &header);
    bool appendBody(const char *data, int size);
    void recordNativeError(int error);
    void finish(State state, int error, const QString &errorText);
    bool startRequest(
        const QByteArray &url,
        const QByteArray &formBody,
        bool post,
        int timeoutMilliseconds,
        int maximumResponseBytes,
        const QByteArray &cookieHeader,
        RequestProfile profile);

    NativeTransportPrivate *m_private;
    State m_state;
    Result m_result;
    QTime m_clock;
    int m_timeoutMilliseconds;
    int m_maximumResponseBytes;
    bool m_terminalReported;
};

} // namespace wiliwili

#endif
