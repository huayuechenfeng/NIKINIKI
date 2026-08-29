#ifndef WILIWILI_SYMBIAN_DIRECT_TRANSPORT_H
#define WILIWILI_SYMBIAN_DIRECT_TRANSPORT_H

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QTime>
#include <QtCore/QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace wiliwili {

class DirectTransport
{
public:
    enum State {
        Idle,
        Running,
        Succeeded,
        Failed,
        TimedOut,
        ResponseTooLarge
    };

    struct Result {
        int httpStatus;
        int networkError;
        int elapsedMilliseconds;
        QByteArray body;
        QString errorText;

        Result()
            : httpStatus(0),
              networkError(0),
              elapsedMilliseconds(0)
        {
        }
    };

    DirectTransport();
    ~DirectTransport();

    bool startGet(
        const QUrl &url,
        int timeoutMilliseconds,
        int maximumResponseBytes);
    bool poll();
    void cancel();

    State state() const;
    const Result &result() const;

private:
    DirectTransport(const DirectTransport &);
    DirectTransport &operator=(const DirectTransport &);

    void finish(State state, const QString &errorText);
    void releaseReply();

    QNetworkAccessManager *m_manager;
    QNetworkReply *m_reply;
    State m_state;
    Result m_result;
    QTime m_clock;
    int m_timeoutMilliseconds;
    int m_maximumResponseBytes;
};

} // namespace wiliwili

#endif
