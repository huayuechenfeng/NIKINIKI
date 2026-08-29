#include "network/native_transport.h"

#include <QtCore/QDebug>
#include <QtCore/QtGlobal>

#ifdef Q_OS_SYMBIAN
#include <e32base.h>
#include <http.h>
#include <uri8.h>
#endif

namespace wiliwili {

#ifdef Q_OS_SYMBIAN

static QByteArray httpHeaderValueBytes(const THTTPHdrVal &value)
{
    if (value.Type() == THTTPHdrVal::KStrFVal) {
        const TPtrC8 text(value.StrF().DesC());
        return QByteArray(
            reinterpret_cast<const char *>(text.Ptr()), text.Length());
    }
    if (value.Type() == THTTPHdrVal::KStrVal) {
        const TPtrC8 text(value.Str().DesC());
        return QByteArray(
            reinterpret_cast<const char *>(text.Ptr()), text.Length());
    }
    if (value.Type() == THTTPHdrVal::KTIntVal)
        return QByteArray::number(value.Int());
    return QByteArray();
}

static bool isCookieName(const QByteArray &name)
{
    if (name.isEmpty())
        return false;
    int index;
    for (index = 0; index < name.size(); ++index) {
        const unsigned char c =
            static_cast<unsigned char>(name.at(index));
        if (c <= 0x20 || c >= 0x7f || c == '(' || c == ')' ||
            c == '<' || c == '>' || c == '@' || c == ',' ||
            c == ';' || c == ':' || c == '\\' || c == '"' ||
            c == '/' || c == '[' || c == ']' || c == '?' ||
            c == '=' || c == '{' || c == '}') {
            return false;
        }
    }
    return true;
}

static QByteArray cookiePairFromText(const QByteArray &input)
{
    QByteArray text = input.trimmed();
    const QByteArray prefix("set-cookie:");
    if (text.left(prefix.size()).toLower() == prefix)
        text = text.mid(prefix.size()).trimmed();

    int end = text.indexOf(';');
    const int carriageReturn = text.indexOf('\r');
    const int lineFeed = text.indexOf('\n');
    if (end < 0 || (carriageReturn >= 0 && carriageReturn < end))
        end = carriageReturn;
    if (end < 0 || (lineFeed >= 0 && lineFeed < end))
        end = lineFeed;
    if (end >= 0)
        text = text.left(end).trimmed();

    const int equals = text.indexOf('=');
    if (equals <= 0)
        return QByteArray();
    const QByteArray name = text.left(equals).trimmed();
    if (!isCookieName(name))
        return QByteArray();
    return name + '=' + text.mid(equals + 1).trimmed();
}

class NativeTransportPrivate : public CBase,
                               public MHTTPTransactionCallback,
                               public MHTTPDataSupplier
{
public:
    static NativeTransportPrivate *NewL(NativeTransport *owner)
    {
        NativeTransportPrivate *self =
            new (ELeave) NativeTransportPrivate(owner);
        CleanupStack::PushL(self);
        self->ConstructL();
        CleanupStack::Pop(self);
        return self;
    }

    ~NativeTransportPrivate()
    {
        closeTransaction();
        if (m_sessionOpen)
            m_session.Close();
        delete m_uriBuffer;
        m_uriBuffer = 0;
        delete m_requestBody;
        m_requestBody = 0;
    }

    void startRequestL(
        const QByteArray &url,
        const QByteArray &cookieHeader,
        const QByteArray &formBody,
        bool post,
        NativeTransport::RequestProfile profile)
    {
        closeTransaction();
        delete m_uriBuffer;
        m_uriBuffer = 0;
        delete m_requestBody;
        m_requestBody = 0;

        m_uriBuffer = HBufC8::NewL(url.size());
        m_uriBuffer->Des().Copy(TPtrC8(
            reinterpret_cast<const TUint8 *>(url.constData()),
            url.size()));
        User::LeaveIfError(m_uriParser.Parse(*m_uriBuffer));

        m_transaction = m_session.OpenTransactionL(
            m_uriParser,
            *this,
            m_session.StringPool().StringF(
                post ? HTTP::EPOST : HTTP::EGET,
                RHTTPSession::GetTable()));
        m_transactionOpen = true;

        RHTTPHeaders headers =
            m_transaction.Request().GetHeaderCollection();
        if (profile == NativeTransport::WebSearchRequestProfile) {
            // /wbi/search/type is a web endpoint.  Sending the legacy native
            // user agent with its generic client referer can yield code=0 plus
            // data.v_voucher, which is not a search result.  This is kept
            // endpoint-specific rather than replacing the application's
            // normal transport identity.
            setHeaderL(headers, HTTP::EUserAgent,
                _L8("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                    "AppleWebKit/537.36 (KHTML, like Gecko) "
                    "Chrome/131.0.0.0 Safari/537.36"));
        } else {
            setHeaderL(headers, HTTP::EUserAgent, _L8("wiliwili"));
        }
        setHeaderL(headers, HTTP::EAccept,
            _L8("application/json,text/xml,application/xml,text/plain,*/*"));
        setHeaderL(headers, HTTP::EAcceptLanguage,
            _L8("zh-CN,zh;q=0.9,en;q=0.6"));
        // Keep payloads directly parseable by the legacy Qt/Symbian stack.
        // In particular, list.so is XML and Qt 4.7 has no transparent gzip
        // handling on this native RHTTP path.
        setHeaderL(headers, HTTP::EAcceptEncoding, _L8("identity"));
        if (profile == NativeTransport::WebSearchRequestProfile) {
            setHeaderL(headers, HTTP::EReferer,
                _L8("https://search.bilibili.com/"));
        } else {
            setHeaderL(headers, HTTP::EReferer,
                _L8("https://www.bilibili.com/client"));
        }
        // Match upstream wiliwili's global request headers. Origin is not in
        // Belle's old HTTP string table, so keep it as an opaque raw field.
        setRawHeaderL(
            headers, _L8("Origin"), _L8("https://www.bilibili.com"));
        if (!cookieHeader.isEmpty()) {
            const TPtrC8 cookieValue(
                reinterpret_cast<const TUint8 *>(cookieHeader.constData()),
                cookieHeader.size());
            // Cookie is a structured header in the Symbian HTTP stack.
            // SetFieldL() treats the complete "a=b; c=d" value as one parsed
            // atom and can re-serialize it differently on the wire. Upstream
            // wiliwili deliberately sends its cookie jar as one verbatim
            // header too, so bypass the legacy codec here.
            const RStringF cookieName =
                m_session.StringPool().StringF(
                    HTTP::ECookie, RHTTPSession::GetTable());
            headers.SetRawFieldL(cookieName, cookieValue, KNullDesC8);
            TPtrC8 serializedCookie;
            const TInt serializedResult =
                headers.GetRawField(cookieName, serializedCookie);
            qDebug() << "WW:COOKIE_WIRE"
                     << cookieHeader.size()
                     << (serializedResult == KErrNone
                            ? serializedCookie.Length() : serializedResult)
                     << cookieHeader.count(';');
        }
        if (post) {
            setHeaderL(headers, HTTP::EContentType,
                _L8("application/x-www-form-urlencoded"));
            m_requestBody = HBufC8::NewL(formBody.size());
            m_requestBody->Des().Copy(TPtrC8(
                reinterpret_cast<const TUint8 *>(formBody.constData()),
                formBody.size()));
            m_transaction.Request().SetBody(*this);
        }
        m_transaction.SubmitL();
    }

    virtual TBool GetNextDataPart(TPtrC8 &dataPart)
    {
        if (m_requestBody)
            dataPart.Set(m_requestBody->Des());
        else
            dataPart.Set(KNullDesC8());
        return ETrue;
    }

    virtual void ReleaseData()
    {
    }

    virtual TInt OverallDataSize()
    {
        return m_requestBody ? m_requestBody->Length() : 0;
    }

    virtual TInt Reset()
    {
        return KErrNone;
    }

    void cancelAndClose()
    {
        if (m_transactionOpen)
            m_transaction.Cancel();
        closeTransaction();
    }

    void closeTransaction()
    {
        if (m_transactionOpen) {
            m_transaction.Close();
            m_transactionOpen = false;
        }
    }

private:
    explicit NativeTransportPrivate(NativeTransport *owner)
        : m_owner(owner),
          m_uriBuffer(0),
          m_requestBody(0),
          m_sessionOpen(false),
          m_transactionOpen(false)
    {
    }

    void ConstructL()
    {
        m_session.OpenL();
        m_sessionOpen = true;
    }

    void setHeaderL(
        RHTTPHeaders headers,
        HTTP::TStrings field,
        const TDesC8 &value)
    {
        RStringF valueString = m_session.StringPool().OpenFStringL(value);
        CleanupClosePushL(valueString);
        headers.SetFieldL(
            m_session.StringPool().StringF(field, RHTTPSession::GetTable()),
            THTTPHdrVal(valueString));
        CleanupStack::PopAndDestroy(&valueString);
    }

    void setRawHeaderL(
        RHTTPHeaders headers,
        const TDesC8 &field,
        const TDesC8 &value)
    {
        RStringF fieldString =
            m_session.StringPool().OpenFStringL(field);
        CleanupClosePushL(fieldString);
        headers.SetRawFieldL(fieldString, value, KNullDesC8);
        CleanupStack::PopAndDestroy(&fieldString);
    }

    virtual void MHFRunL(
        RHTTPTransaction transaction,
        const THTTPEvent &event)
    {
        if (m_owner->state() != NativeTransport::Running)
            return;

        switch (event.iStatus) {
        case THTTPEvent::EGotResponseHeaders: {
            m_owner->setHttpStatus(transaction.Response().StatusCode());
            RHTTPHeaders responseHeaders =
                transaction.Response().GetHeaderCollection();
            const RStringF setCookieName =
                m_session.StringPool().StringF(
                    HTTP::ESetCookie,
                    RHTTPSession::GetTable());
            TPtrC8 rawCookies;
            QByteArray rawCookieFallback;
            if (responseHeaders.GetRawField(
                    setCookieName, rawCookies) == KErrNone) {
                rawCookieFallback = QByteArray(
                    reinterpret_cast<const char *>(rawCookies.Ptr()),
                    rawCookies.Length());
            }

            // Set-Cookie is a repeated response header. GetRawField() can
            // expose only one serialized part on device, so collect every
            // decoded field part too. The HTTP header codec stores the
            // mandatory cookie name/value as ECookieName/ECookieValue
            // parameters. Reading them directly avoids Belle's separate
            // cookiemanager.dll while preserving all QR-login cookies on the
            // original Symbian^3, Anna and Belle HTTP stacks.
            const TInt cookiePartCount =
                responseHeaders.FieldPartsL(setCookieName);
            const RStringF cookieNameParameter =
                m_session.StringPool().StringF(
                    HTTP::ECookieName, RHTTPSession::GetTable());
            const RStringF cookieValueParameter =
                m_session.StringPool().StringF(
                    HTTP::ECookieValue, RHTTPSession::GetTable());
            TInt capturedCookieParts = 0;
            TInt cookiePartIndex;
            for (cookiePartIndex = 0;
                 cookiePartIndex < cookiePartCount;
                 ++cookiePartIndex) {
                THTTPHdrVal nameValue;
                THTTPHdrVal valueValue;
                QByteArray name;
                QByteArray value;
                if (responseHeaders.GetParam(
                        setCookieName, cookieNameParameter, nameValue,
                        cookiePartIndex) == KErrNone) {
                    name = httpHeaderValueBytes(nameValue).trimmed();
                }
                if (responseHeaders.GetParam(
                        setCookieName, cookieValueParameter, valueValue,
                        cookiePartIndex) == KErrNone) {
                    value = httpHeaderValueBytes(valueValue).trimmed();
                }

                THTTPHdrVal fieldValue;
                QByteArray fieldText;
                if (responseHeaders.GetField(
                        setCookieName, cookiePartIndex,
                        fieldValue) == KErrNone) {
                    fieldText = httpHeaderValueBytes(fieldValue).trimmed();
                }

                QByteArray pair;
                if (isCookieName(name)) {
                    // Some older codecs expose the cookie name as the field
                    // value and only the value as a parameter; others expose
                    // both internal parameters. Prefer the explicit pair.
                    if (value.isEmpty() && fieldText != name &&
                        fieldText.indexOf('=') < 0) {
                        value = fieldText;
                    }
                    pair = name + '=' + value;
                }
                if (pair.isEmpty())
                    pair = cookiePairFromText(fieldText);
                if (!pair.isEmpty()) {
                    m_owner->setResponseCookieHeader(pair);
                    ++capturedCookieParts;
                }
            }
            if (!rawCookieFallback.isEmpty())
                m_owner->setResponseCookieHeader(rawCookieFallback);
            qDebug() << "WW:COOKIE_PARTS"
                     << cookiePartCount
                     << capturedCookieParts
                     << rawCookieFallback.size();
            break;
        }

        case THTTPEvent::EGotResponseBodyData: {
            MHTTPDataSupplier *body = transaction.Response().Body();
            if (body) {
                TPtrC8 part;
                body->GetNextDataPart(part);
                const bool accepted = m_owner->appendBody(
                    reinterpret_cast<const char *>(part.Ptr()),
                    part.Length());
                body->ReleaseData();
                if (!accepted)
                    transaction.Cancel();
            }
            break;
        }

        case THTTPEvent::ESucceeded:
            m_owner->finish(
                NativeTransport::Succeeded,
                KErrNone,
                QString());
            break;

        case THTTPEvent::EFailed:
            m_owner->finish(
                NativeTransport::Failed,
                m_owner->result().networkError,
                QString::fromLatin1("native HTTP transaction failed"));
            break;

        default:
            if (event.iStatus < KErrNone)
                m_owner->recordNativeError(event.iStatus);
            break;
        }
    }

    virtual TInt MHFRunError(
        TInt error,
        RHTTPTransaction,
        const THTTPEvent &)
    {
        m_owner->finish(
            NativeTransport::Failed,
            error,
            QString::fromLatin1("native callback error %1").arg(error));
        return KErrNone;
    }

    NativeTransport *m_owner;
    RHTTPSession m_session;
    RHTTPTransaction m_transaction;
    HBufC8 *m_uriBuffer;
    HBufC8 *m_requestBody;
    TUriParser8 m_uriParser;
    bool m_sessionOpen;
    bool m_transactionOpen;
};

#endif

NativeTransport::NativeTransport()
    : m_private(0),
      m_state(Idle),
      m_timeoutMilliseconds(0),
      m_maximumResponseBytes(0),
      m_terminalReported(false)
{
}

NativeTransport::~NativeTransport()
{
    cancel();
    delete m_private;
    m_private = 0;
}

bool NativeTransport::startGet(
    const QByteArray &url,
    int timeoutMilliseconds,
    int maximumResponseBytes,
    const QByteArray &cookieHeader,
    RequestProfile profile)
{
    return startRequest(
        url, QByteArray(), false, timeoutMilliseconds,
        maximumResponseBytes, cookieHeader, profile);
}

bool NativeTransport::startPost(
    const QByteArray &url,
    const QByteArray &formBody,
    int timeoutMilliseconds,
    int maximumResponseBytes,
    const QByteArray &cookieHeader)
{
    return startRequest(
        url, formBody, true, timeoutMilliseconds,
        maximumResponseBytes, cookieHeader, DefaultRequestProfile);
}

bool NativeTransport::startRequest(
    const QByteArray &url,
    const QByteArray &formBody,
    bool post,
    int timeoutMilliseconds,
    int maximumResponseBytes,
    const QByteArray &cookieHeader,
    RequestProfile profile)
{
    if (url.isEmpty() || m_state == Running ||
        (post && formBody.isEmpty()))
        return false;

#ifdef Q_OS_SYMBIAN
    if (!m_private) {
        TRAPD(createError, m_private = NativeTransportPrivate::NewL(this));
        if (createError != KErrNone) {
            finish(
                Failed,
                createError,
                QString::fromLatin1("native session error %1")
                    .arg(createError));
            return false;
        }
    }

    m_private->closeTransaction();
    m_result = Result();
    m_timeoutMilliseconds = qMax(1000, timeoutMilliseconds);
    m_maximumResponseBytes = qMax(1024, maximumResponseBytes);
    m_terminalReported = false;
    m_state = Running;
    m_clock.start();

    TRAPD(startError, m_private->startRequestL(
        url, cookieHeader, formBody, post, profile));
    if (startError != KErrNone) {
        finish(
            Failed,
            startError,
            QString::fromLatin1("native request error %1").arg(startError));
        return false;
    }
    return true;
#else
    Q_UNUSED(formBody);
    Q_UNUSED(post);
    Q_UNUSED(timeoutMilliseconds);
    Q_UNUSED(maximumResponseBytes);
    Q_UNUSED(profile);
    finish(Failed, -1, QString::fromLatin1("Symbian-only transport"));
    return false;
#endif
}

bool NativeTransport::poll()
{
    if (m_state == Running &&
        m_clock.elapsed() >= m_timeoutMilliseconds) {
#ifdef Q_OS_SYMBIAN
        if (m_private)
            m_private->cancelAndClose();
#endif
        finish(
            TimedOut,
            0,
            QString::fromLatin1("native request timed out"));
    }

    if (m_state != Idle && m_state != Running && !m_terminalReported) {
#ifdef Q_OS_SYMBIAN
        if (m_private)
            m_private->closeTransaction();
#endif
        m_terminalReported = true;
        return true;
    }
    return false;
}

void NativeTransport::cancel()
{
#ifdef Q_OS_SYMBIAN
    if (m_private)
        m_private->cancelAndClose();
#endif
    m_state = Idle;
    m_terminalReported = false;
}

NativeTransport::State NativeTransport::state() const
{
    return m_state;
}

const NativeTransport::Result &NativeTransport::result() const
{
    return m_result;
}

void NativeTransport::setHttpStatus(int status)
{
    m_result.httpStatus = status;
}

void NativeTransport::setResponseCookieHeader(const QByteArray &header)
{
    if (header.isEmpty())
        return;
    if (!m_result.setCookieHeader.isEmpty())
        m_result.setCookieHeader += '\n';
    m_result.setCookieHeader += header;
}

bool NativeTransport::appendBody(const char *data, int size)
{
    if (size <= 0)
        return true;
    if (m_result.body.size() + size > m_maximumResponseBytes) {
        finish(
            ResponseTooLarge,
            0,
            QString::fromLatin1("native response exceeds limit"));
        return false;
    }
    m_result.body.append(data, size);
    return true;
}

void NativeTransport::recordNativeError(int error)
{
    m_result.networkError = error;
    m_result.errorText =
        QString::fromLatin1("native error %1").arg(error);
}

void NativeTransport::finish(
    State state,
    int error,
    const QString &errorText)
{
    if (m_state != Running && state != Failed)
        return;
    m_state = state;
    m_result.networkError = error;
    m_result.elapsedMilliseconds = m_clock.elapsed();
    if (!errorText.isEmpty())
        m_result.errorText = errorText;
}

} // namespace wiliwili
