#include "ui/login_screen.h"

#include <QtCore/QByteArray>
#include <QtCore/QtGlobal>

extern "C" {
#include "../../third_party/qrcodegen/qrcodegen.h"
}

#include "ui/navigation_rail.h"

namespace wiliwili {

LoginScreen::LoginScreen()
    : m_fontId(-1),
      m_qrSize(0),
      m_expired(false),
      m_pressed(false),
      m_hasProfile(false)
{
    m_status = QString::fromUtf8("正在准备登录...");
}

void LoginScreen::initialize(int fontId)
{
    m_fontId = fontId;
}

void LoginScreen::setLoading(const QString &status)
{
    m_status = status;
    m_expired = false;
}

bool LoginScreen::setQrContent(const QString &content)
{
    const int bufferLength = qrcodegen_BUFFER_LEN_FOR_VERSION(20);
    QByteArray temporary(bufferLength, '\0');
    QByteArray qr(bufferLength, '\0');
    const QByteArray utf8 = content.toUtf8();
    const bool encoded = qrcodegen_encodeText(
        utf8.constData(),
        reinterpret_cast<uint8_t *>(temporary.data()),
        reinterpret_cast<uint8_t *>(qr.data()),
        qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN,
        20,
        qrcodegen_Mask_AUTO,
        true);
    if (!encoded) {
        m_qrData.clear();
        m_qrSize = 0;
        m_status = QString::fromUtf8("二维码生成失败，请重试");
        m_expired = true;
        return false;
    }
    m_qrData = qr;
    m_qrSize = qrcodegen_getSize(
        reinterpret_cast<const uint8_t *>(m_qrData.constData()));
    m_qrContent = content;
    m_status = QString::fromUtf8("请使用哔哩哔哩客户端扫码");
    m_expired = false;
    return true;
}

void LoginScreen::setLoginStatus(const QString &status, bool expired)
{
    m_status = status;
    m_expired = expired;
}

void LoginScreen::setProfile(const LoginProfileCompat &profile)
{
    m_profile = profile;
    m_hasProfile = true;
    m_qrData.clear();
    m_qrSize = 0;
}

void LoginScreen::clearProfile()
{
    m_profile = LoginProfileCompat();
    m_hasProfile = false;
    m_qrData.clear();
    m_qrSize = 0;
    m_expired = false;
    m_status = QString::fromUtf8("正在准备登录...");
}

bool LoginScreen::hasProfile() const
{
    return m_hasProfile;
}

const LoginProfileCompat &LoginScreen::profile() const
{
    return m_profile;
}

void LoginScreen::drawText(
    NVGcontext *context,
    const QString &text,
    float x,
    float y,
    float size,
    const NVGcolor &color,
    int align) const
{
    if (m_fontId < 0 || text.isEmpty())
        return;
    const QByteArray utf8 = text.toUtf8();
    nvgFontFaceId(context, m_fontId);
    nvgFontSize(context, size);
    nvgTextAlign(context, align);
    nvgFillColor(context, color);
    nvgText(context, x, y, utf8.constData(), 0);
}

void LoginScreen::drawQr(NVGcontext *context, const QRectF &frame) const
{
    if (m_qrSize <= 0 || m_qrData.isEmpty())
        return;
    const int border = 3;
    const float module = qMin(
        static_cast<float>(frame.width()) / (m_qrSize + border * 2),
        static_cast<float>(frame.height()) / (m_qrSize + border * 2));
    const float qrPixels = module * (m_qrSize + border * 2);
    const float originX = static_cast<float>(frame.center().x()) - qrPixels * 0.5f;
    const float originY = static_cast<float>(frame.center().y()) - qrPixels * 0.5f;

    nvgBeginPath(context);
    nvgRoundedRect(context, originX, originY, qrPixels, qrPixels, 5.0f);
    nvgFillColor(context, nvgRGB(255, 255, 255));
    nvgFill(context);

    const uint8_t *data = reinterpret_cast<const uint8_t *>(
        m_qrData.constData());
    nvgBeginPath(context);
    int y;
    for (y = 0; y < m_qrSize; ++y) {
        int x;
        for (x = 0; x < m_qrSize; ++x) {
            if (!qrcodegen_getModule(data, x, y))
                continue;
            nvgRect(context,
                    originX + (x + border) * module,
                    originY + (y + border) * module,
                    module + 0.12f,
                    module + 0.12f);
        }
    }
    nvgFillColor(context, nvgRGB(18, 18, 22));
    nvgFill(context);
}

void LoginScreen::drawLoggedOut(
    NVGcontext *context,
    float width,
    float height)
{
    const float contentLeft = NavigationRail::width();
    const float contentWidth = width - contentLeft;
    const float cardX = contentLeft + 14.0f;
    const float cardWidth = contentWidth - 28.0f;
    const float cardY = 76.0f;
    const float contentBottom = height - NavigationRail::height();
    const float cardHeight = qMin(
        470.0f, contentBottom - cardY - 14.0f);

    nvgBeginPath(context);
    nvgRoundedRect(context, cardX, cardY, cardWidth, cardHeight, 13.0f);
    nvgFillColor(context, nvgRGBA(43, 43, 53, 246));
    nvgFill(context);

    drawText(context, QString::fromUtf8("扫码登录"),
             cardX + cardWidth * 0.5f, cardY + 26.0f, 18.0f,
             nvgRGB(250, 250, 252),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    const float qrSide = qMin(cardWidth - 48.0f, 236.0f);
    const QRectF qrFrame(
        cardX + (cardWidth - qrSide) * 0.5f,
        cardY + 53.0f,
        qrSide,
        qrSide);

    if (m_qrSize > 0) {
        drawQr(context, qrFrame);
    } else {
        nvgBeginPath(context);
        nvgRoundedRect(context,
                       static_cast<float>(qrFrame.x()),
                       static_cast<float>(qrFrame.y()),
                       static_cast<float>(qrFrame.width()),
                       static_cast<float>(qrFrame.height()),
                       8.0f);
        nvgFillColor(context, nvgRGBA(255, 255, 255, 12));
        nvgFill(context);
        drawText(context, QString::fromLatin1("..."),
                 static_cast<float>(qrFrame.center().x()),
                 static_cast<float>(qrFrame.center().y()), 22.0f,
                 nvgRGB(251, 114, 153),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }

    drawText(context, m_status,
             cardX + cardWidth * 0.5f,
             static_cast<float>(qrFrame.bottom()) + 24.0f,
             11.5f,
             m_expired ? nvgRGB(251, 114, 153)
                       : nvgRGB(190, 190, 202),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    drawText(context, QString::fromUtf8("登录凭据仅保存在本机"),
             cardX + cardWidth * 0.5f,
             static_cast<float>(qrFrame.bottom()) + 46.0f,
             9.5f,
             nvgRGB(130, 130, 144),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    if (m_expired) {
        const float buttonWidth = qMin(152.0f, cardWidth - 40.0f);
        const float buttonX = cardX + (cardWidth - buttonWidth) * 0.5f;
        const float buttonY = cardY + cardHeight - 54.0f;
        m_actionHitBox = QRectF(buttonX, buttonY, buttonWidth, 34.0f);
        nvgBeginPath(context);
        nvgRoundedRect(context, buttonX, buttonY, buttonWidth, 34.0f, 9.0f);
        nvgFillColor(context, nvgRGB(251, 114, 153));
        nvgFill(context);
        drawText(context, QString::fromUtf8("刷新二维码"),
                 buttonX + buttonWidth * 0.5f, buttonY + 17.0f,
                 11.5f, nvgRGB(255, 255, 255),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    } else {
        m_actionHitBox = QRectF();
    }
}

void LoginScreen::drawProfile(
    NVGcontext *context,
    float width,
    float height)
{
    const float contentLeft = NavigationRail::width();
    const float cardX = contentLeft + 14.0f;
    const float cardWidth = width - contentLeft - 28.0f;

    nvgBeginPath(context);
    nvgRoundedRect(context, cardX, 70.0f, cardWidth, 218.0f, 13.0f);
    nvgFillColor(context, nvgRGBA(43, 43, 53, 246));
    nvgFill(context);

    const float avatarX = cardX + cardWidth * 0.5f;
    nvgBeginPath(context);
    nvgCircle(context, avatarX, 122.0f, 31.0f);
    nvgFillColor(context, nvgRGB(251, 114, 153));
    nvgFill(context);
    const QString initial = m_profile.name.isEmpty()
        ? QString::fromLatin1("B")
        : m_profile.name.left(1);
    drawText(context, initial, avatarX, 122.0f, 23.0f,
             nvgRGB(255, 255, 255),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    drawText(context, m_profile.name,
             avatarX, 169.0f, 17.0f,
             nvgRGB(250, 250, 252),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    drawText(context,
             QString::fromLatin1("UID %1   LV%2")
                 .arg(m_profile.mid)
                 .arg(m_profile.level),
             avatarX, 195.0f, 10.5f,
             nvgRGB(174, 174, 188),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    QString sign = m_profile.sign.simplified();
    if (sign.isEmpty())
        sign = QString::fromUtf8("这个人很神秘，还没有简介");
    if (sign.size() > 28)
        sign = sign.left(27) + QString::fromUtf8("…");
    drawText(context, sign,
             avatarX, 219.0f, 9.6f,
             nvgRGB(210, 210, 220),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    drawText(context,
             QString::fromUtf8("关注 %1    粉丝 %2")
                 .arg(m_profile.following).arg(m_profile.follower),
             avatarX, 245.0f, 10.0f, nvgRGB(190, 190, 202),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    static const char *featureLabels[] = {
        "历史记录", "我的收藏", "稍后再看", "我的投稿", "我的关注"
    };
    const float gridTop = 304.0f;
    const float gap = 8.0f;
    const float tileWidth = (cardWidth - gap) * 0.5f;
    const float tileHeight = 54.0f;
    int index;
    for (index = 0; index < 5; ++index) {
        const int row = index / 2;
        const int column = index % 2;
        const float tileX = cardX + column * (tileWidth + gap);
        const float tileY = gridTop + row * (tileHeight + gap);
        m_featureHitBoxes[index] =
            QRectF(tileX, tileY, tileWidth, tileHeight);
        nvgBeginPath(context);
        nvgRoundedRect(context, tileX, tileY,
                       tileWidth, tileHeight, 10.0f);
        nvgFillColor(context, nvgRGBA(43, 43, 53, 246));
        nvgFill(context);
        drawText(context, QString::fromUtf8(featureLabels[index]),
                 tileX + tileWidth * 0.5f, tileY + tileHeight * 0.5f,
                 11.5f, index == 1 ? nvgRGB(251, 130, 163)
                                    : nvgRGB(230, 230, 236),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }

    const float logoutY = qMin(
        height - NavigationRail::height() - 44.0f,
        gridTop + 3.0f * (tileHeight + gap) + 2.0f);
    m_logoutHitBox = QRectF(cardX, logoutY, cardWidth, 36.0f);
    nvgBeginPath(context);
    nvgRoundedRect(context, cardX, logoutY, cardWidth, 36.0f, 9.0f);
    nvgFillColor(context, nvgRGBA(255, 255, 255, 12));
    nvgFill(context);
    drawText(context, QString::fromUtf8("退出登录"), avatarX,
             logoutY + 18.0f, 10.5f, nvgRGB(188, 188, 200),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    m_actionHitBox = QRectF();

}

void LoginScreen::draw(NVGcontext *context, float width, float height)
{
    NVGpaint background = nvgLinearGradient(
        context, NavigationRail::width(), 0.0f, width, height,
        nvgRGBA(31, 31, 40, 255),
        nvgRGBA(20, 21, 29, 255));
    nvgBeginPath(context);
    nvgRect(context, NavigationRail::width(), 0.0f,
            width - NavigationRail::width(), height);
    nvgFillPaint(context, background);
    nvgFill(context);

    drawText(context, QString::fromUtf8("账号"),
             NavigationRail::width() + 16.0f, 30.0f, 20.0f,
             nvgRGB(248, 248, 250),
             NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    drawText(context,
             m_hasProfile ? QString::fromUtf8("账号详情")
                          : QString::fromUtf8("使用哔哩哔哩扫码登录"),
             width - 14.0f, 31.0f, 10.0f,
             nvgRGB(148, 148, 160),
             NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);

    if (m_hasProfile)
        drawProfile(context, width, height);
    else
        drawLoggedOut(context, width, height);
}

void LoginScreen::pointerPress(const QPoint &position)
{
    m_pressed = true;
    m_pressPosition = position;
}

LoginScreen::Action LoginScreen::pointerRelease(const QPoint &position)
{
    if (!m_pressed)
        return NoAction;
    m_pressed = false;
    if ((position - m_pressPosition).manhattanLength() >= 18) {
        return NoAction;
    }
    if (!m_hasProfile)
        return m_actionHitBox.contains(QPointF(position))
            ? RefreshQrAction : NoAction;
    if (m_logoutHitBox.contains(QPointF(position)))
        return LogoutAction;
    int index;
    for (index = 0; index < 5; ++index) {
        if (!m_featureHitBoxes[index].contains(QPointF(position)))
            continue;
        static const Action actions[] = {
            HistoryAction, FavoritesAction, WatchLaterAction,
            MyVideosAction, FollowingAction
        };
        return actions[index];
    }
    return NoAction;
}

} // namespace wiliwili
