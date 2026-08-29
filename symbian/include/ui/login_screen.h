#ifndef WILIWILI_SYMBIAN_LOGIN_SCREEN_H
#define WILIWILI_SYMBIAN_LOGIN_SCREEN_H

#include <QtCore/QByteArray>
#include <QtCore/QPoint>
#include <QtCore/QRectF>
#include <QtCore/QString>

#include "model/login_session.h"
#include "nanovg.h"

namespace wiliwili {

class LoginScreen
{
public:
    enum Action {
        NoAction,
        RefreshQrAction,
        LogoutAction,
        HistoryAction,
        FavoritesAction,
        WatchLaterAction,
        MyVideosAction,
        FollowingAction
    };

    LoginScreen();

    void initialize(int fontId);
    void setLoading(const QString &status);
    bool setQrContent(const QString &content);
    void setLoginStatus(const QString &status, bool expired = false);
    void setProfile(const LoginProfileCompat &profile);
    void clearProfile();
    bool hasProfile() const;
    const LoginProfileCompat &profile() const;

    void draw(NVGcontext *context, float width, float height);
    void pointerPress(const QPoint &position);
    Action pointerRelease(const QPoint &position);

private:
    void drawText(
        NVGcontext *context,
        const QString &text,
        float x,
        float y,
        float size,
        const NVGcolor &color,
        int align = NVG_ALIGN_LEFT | NVG_ALIGN_TOP) const;
    void drawQr(NVGcontext *context, const QRectF &frame) const;
    void drawLoggedOut(NVGcontext *context, float width, float height);
    void drawProfile(NVGcontext *context, float width, float height);

    int m_fontId;
    QByteArray m_qrData;
    int m_qrSize;
    QString m_qrContent;
    QString m_status;
    bool m_expired;
    bool m_pressed;
    QPoint m_pressPosition;
    QRectF m_actionHitBox;
    QRectF m_logoutHitBox;
    QRectF m_featureHitBoxes[5];
    LoginProfileCompat m_profile;
    bool m_hasProfile;
};

} // namespace wiliwili

#endif
