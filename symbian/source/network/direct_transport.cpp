#include "network/direct_transport.h"

#include <QtCore/QVariant>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace wiliwili {

DirectTransport::DirectTransport()
    : m_manager(new QNetworkAccessManager()),
      m_reply(0),
      m_state(Idle),
      m_timeoutMilliseconds(0),
      m_maximumResponseBytes(0)
{
}

DirectTransport::~DirectTransport()
{
    cancel();
    delete m_manager;
    m_manager = 0;
}

bool DirectTransport::startGet(
    const QUrl &url,
    int timeoutMilliseconds,
    int maximumResponseBytes)
{
    if (!url.isValid() || m_state == Running)
        return false;

    releaseReply();
    m_result = Result();
    m_timeoutMilliseconds = qMax(1000, timeoutMilliseconds);
    m_maximumResponseBytes = qMax(1024, maximumResponseBytes);

    QNetworkRequest request(url);
    request.setRawHeader(
        "User-Agent",
        "Mozilla/5.0 (Symbian/3; Nokia603) wiliwili-symbian/0.4");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Referer", "https://www.bilibili.com/");
    request.setRawHeader("Accept-Encoding", "identity");

    m_reply = m_manager->get(request);
    if (!m_reply) {
        finish(Failed, QString::fromLatin1("request creation failed"));
        return false;
    }

    m_clock.start();
    m_state = Running;
    return true;
}

bool DirectTransport::poll()
{
    if (m_state != Running || !m_reply)
        return false;

    const QVariant contentLength =
        m_reply->header(QNetworkRequest::ContentLengthHeader);
    if ((contentLength.isValid() &&
         contentLength.toLongLong() > m_maximumResponseBytes) ||
        m_reply->bytesAvailable() > m_maximumResponseBytes) {
        m_reply->abort();
        finish(ResponseTooLarge, QString::fromLatin1("response exceeds limit"));
        return true;
    }

    if (m_reply->isFinished()) {
        m_result.elapsedMilliseconds = m_clock.elapsed();
        m_result.httpStatus = m_reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_result.networkError = static_cast<int>(m_reply->error());
        m_result.errorText = m_reply->errorString();
        m_result.body = m_reply->readAll();

        if (m_result.body.size() > m_maximumResponseBytes) {
            finish(ResponseTooLarge, QString::fromLatin1("response exceeds limit"));
        } else if (m_reply->error() == QNetworkReply::NoError) {
            finish(Succeeded, QString());
        } else {
            finish(Failed, m_result.errorText);
        }
        return true;
    }

    if (m_clock.elapsed() >= m_timeoutMilliseconds) {
        m_reply->abort();
        m_result.elapsedMilliseconds = m_clock.elapsed();
        finish(TimedOut, QString::fromLatin1("request timed out"));
        return true;
    }

    return false;
}

void DirectTransport::cancel()
{
    if (m_reply && m_state == Running)
        m_reply->abort();
    releaseReply();
    m_state = Idle;
}

DirectTransport::State DirectTransport::state() const
{
    return m_state;
}

const DirectTransport::Result &DirectTransport::result() const
{
    return m_result;
}

void DirectTransport::finish(State state, const QString &errorText)
{
    m_state = state;
    if (!errorText.isEmpty())
        m_result.errorText = errorText;
    releaseReply();
}

void DirectTransport::releaseReply()
{
    delete m_reply;
    m_reply = 0;
}

} // namespace wiliwili
