#ifndef WILIWILI_APP_LANDSCAPE_WINDOW_PROBE_H
#define WILIWILI_APP_LANDSCAPE_WINDOW_PROBE_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>

class QWidget;

namespace wiliwili {

class WiliwiliWidget;
class AppLandscapeProbeWindow;

class AppLandscapeWindowProbe : public QObject
{
    Q_OBJECT

public:
    explicit AppLandscapeWindowProbe(
        WiliwiliWidget *host, QObject *parent = 0);
    virtual ~AppLandscapeWindowProbe();

    void requestPortraitRestore();

public slots:
    void start();

private slots:
    void beginCycle();
    void requestLandscapeOrientation();
    void onWorkAreaResized(int screen);
    void onScreenResized(int screen);
    void commitLandscapeWindow();
    void beginPortraitRestore();
    void commitPortraitWindow();
    void completePortraitFullscreen();
    void onTimeout();

private:
    enum Stage {
        IdlePortrait = 0,
        WaitingPortraitChrome = 1,
        WaitingLandscapeWorkArea = 2,
        LandscapeVisible = 3,
        DeletingLandscapeWindow = 4,
        WaitingPortraitWorkArea = 5,
        WaitingPortraitFullscreen = 6,
        ProbeComplete = 7
    };

    void forcePortraitHostVisible(const QString &reason);
    void deleteLandscapeWindow();

    WiliwiliWidget *m_host;
    AppLandscapeProbeWindow *m_landscapeWindow;
    QTimer m_timeout;
    Stage m_stage;
    bool m_landscapeWorkAreaSeen;
    bool m_portraitWorkAreaSeen;
    bool m_portraitFullscreenRestoring;
    bool m_stopAfterRestore;
    int m_completedCycles;
    int m_targetCycles;
};

} // namespace wiliwili

#endif
