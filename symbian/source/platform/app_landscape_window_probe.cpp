#include "platform/app_landscape_window_probe.h"

#include <QtCore/QDebug>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtGui/QApplication>
#include <QtGui/QCloseEvent>
#include <QtGui/QDesktopWidget>
#include <QtGui/QImage>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtCore/QTimerEvent>
#include <QtGui/QWidget>

#include "app/wiliwili_widget.h"

#ifdef Q_OS_SYMBIAN
#include <aknappui.h>
#include <eikenv.h>
#endif

namespace wiliwili {

namespace {

static QString appProbeRectText(const QRect &rect)
{
    return QString::fromLatin1("%1,%2 %3x%4")
        .arg(rect.x())
        .arg(rect.y())
        .arg(rect.width())
        .arg(rect.height());
}

static int requestAppProbeOrientation(bool landscape)
{
#ifdef Q_OS_SYMBIAN
    CEikonEnv *environment = CEikonEnv::Static();
    if (!environment || !environment->EikAppUi())
        return KErrNotReady;

    CAknAppUiBase *appUi =
        static_cast<CAknAppUiBase *>(environment->EikAppUi());
    if (!appUi->OrientationCanBeChanged())
        return KErrNotSupported;

    TInt error = KErrNone;
    const CAknAppUiBase::TAppUiOrientation orientation = landscape
        ? CAknAppUiBase::EAppUiOrientationLandscape
        : CAknAppUiBase::EAppUiOrientationPortrait;
    TRAP(error, appUi->SetOrientationL(orientation));
    return error;
#else
    Q_UNUSED(landscape);
    return -5;
#endif
}

} // namespace

class AppLandscapeProbeWindow : public QWidget
{
public:
    AppLandscapeProbeWindow(
        AppLandscapeWindowProbe *owner, int cycle, int target)
        : QWidget(0, Qt::Window | Qt::FramelessWindowHint)
        , m_owner(owner)
        , m_cycle(cycle)
        , m_target(target)
        , m_firstPaintDone(false)
        , m_frame(640, 360, QImage::Format_RGB16)
        , m_frameTimerId(0)
        , m_frameTick(0)
        , m_paintCount(0)
    {
        setObjectName(QString::fromLatin1("AppLandscapeProbeWindow"));
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::StrongFocus);
        initializeRgb565Frame();
        m_frameTimerId = startTimer(40);
    }

protected:
    virtual void paintEvent(QPaintEvent *)
    {
        const bool firstPaint = !m_firstPaintDone;
        if (firstPaint) {
            qDebug() << "WW:APP_LANDSCAPE_PROBE_FIRST_PAINT_BEGIN"
                     << "geometry" << appProbeRectText(geometry());
        }

        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
        painter.drawImage(QRect(0, 0, width(), height()), m_frame);
        painter.fillRect(QRect(18, 18, width() - 36, height() - 36),
                         QColor(0, 0, 0, 145));
        painter.setPen(Qt::white);

        QFont titleFont = painter.font();
        titleFont.setBold(true);
        titleFont.setPointSize(18);
        painter.setFont(titleFont);
        painter.drawText(QRect(24, 45, width() - 48, 50),
                         Qt::AlignCenter,
                         QString::fromLatin1("RGB565 NATIVE LANDSCAPE"));

        QFont bodyFont = painter.font();
        bodyFont.setBold(false);
        bodyFont.setPointSize(11);
        painter.setFont(bodyFont);
        painter.drawText(QRect(42, 105, width() - 84, height() - 145),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         QString::fromLatin1(
                             "Cycle %1 / %2\n\n"
                             "Dynamic fullscreen + 25 fps RGB565 presentation.\n"
                             "The real QGLWidget remains alive behind this "
                             "disposable raster window.\n\n"
                             "Tap or press Back to restore portrait.")
                             .arg(m_cycle)
                             .arg(m_target));
        painter.end();

        if (firstPaint) {
            m_firstPaintDone = true;
            qDebug() << "WW:APP_LANDSCAPE_PROBE_FIRST_PAINT_END"
                     << "geometry" << appProbeRectText(geometry())
                     << "format" << static_cast<int>(m_frame.format())
                     << "bytesPerLine" << m_frame.bytesPerLine();
        }
        ++m_paintCount;
        if ((m_paintCount % 25) == 0) {
            qDebug() << "WW:APP_LANDSCAPE_PROBE_RGB565_PRESENT"
                     << "cycle" << m_cycle
                     << "paints" << m_paintCount
                     << "ticks" << m_frameTick;
        }
    }

    virtual void timerEvent(QTimerEvent *event)
    {
        if (event->timerId() != m_frameTimerId) {
            QWidget::timerEvent(event);
            return;
        }
        if (!isVisible()) {
            event->accept();
            return;
        }

        const int oldX = m_frameTick % m_frame.width();
        ++m_frameTick;
        const int newX = m_frameTick % m_frame.width();
        restoreRgb565Column(oldX);
        quint16 *line = 0;
        int y;
        for (y = 0; y < m_frame.height(); ++y) {
            line = reinterpret_cast<quint16 *>(m_frame.scanLine(y));
            line[newX] = (y & 8) ? 0xffff : 0x0000;
        }
        update();
        event->accept();
    }

    virtual void mouseReleaseEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton && m_owner) {
            event->accept();
            m_owner->requestPortraitRestore();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    virtual void keyPressEvent(QKeyEvent *event)
    {
        if ((event->key() == Qt::Key_Backspace
             || event->key() == Qt::Key_Escape
             || event->key() == Qt::Key_Back
             || event->key() == Qt::Key_No) && m_owner) {
            event->accept();
            m_owner->requestPortraitRestore();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    virtual void closeEvent(QCloseEvent *event)
    {
        event->ignore();
        if (m_owner)
            m_owner->requestPortraitRestore();
    }

private:
    quint16 rgb565BarColor(int x) const
    {
        static const quint16 colors[] = {
            0xf800, 0xffe0, 0x07e0, 0x07ff,
            0x001f, 0xf81f, 0xffff, 0x0000
        };
        int index = (x * 8) / m_frame.width();
        if (index < 0)
            index = 0;
        if (index > 7)
            index = 7;
        return colors[index];
    }

    void restoreRgb565Column(int x)
    {
        if (x < 0 || x >= m_frame.width())
            return;
        const quint16 color = rgb565BarColor(x);
        int y;
        for (y = 0; y < m_frame.height(); ++y) {
            quint16 *line = reinterpret_cast<quint16 *>(m_frame.scanLine(y));
            line[x] = color;
        }
    }

    void initializeRgb565Frame()
    {
        int x;
        for (x = 0; x < m_frame.width(); ++x)
            restoreRgb565Column(x);
    }

    AppLandscapeWindowProbe *m_owner;
    int m_cycle;
    int m_target;
    bool m_firstPaintDone;
    QImage m_frame;
    int m_frameTimerId;
    int m_frameTick;
    int m_paintCount;
};

AppLandscapeWindowProbe::AppLandscapeWindowProbe(
    WiliwiliWidget *host, QObject *parent)
    : QObject(parent)
    , m_host(host)
    , m_landscapeWindow(0)
    , m_timeout(this)
    , m_stage(IdlePortrait)
    , m_landscapeWorkAreaSeen(false)
    , m_portraitWorkAreaSeen(false)
    , m_portraitFullscreenRestoring(false)
    , m_stopAfterRestore(false)
    , m_completedCycles(0)
    , m_targetCycles(20)
{
    QDesktopWidget *desktop = QApplication::desktop();
    connect(desktop, SIGNAL(workAreaResized(int)),
            this, SLOT(onWorkAreaResized(int)));
    connect(desktop, SIGNAL(resized(int)),
            this, SLOT(onScreenResized(int)));
    connect(&m_timeout, SIGNAL(timeout()),
            this, SLOT(onTimeout()));
    m_timeout.setSingleShot(true);
}

AppLandscapeWindowProbe::~AppLandscapeWindowProbe()
{
    m_timeout.stop();
    deleteLandscapeWindow();
    if (m_stage != IdlePortrait && m_stage != ProbeComplete)
        requestAppProbeOrientation(false);
    if (m_host)
        m_host->setLandscapeWindowProbeActive(false);
}

void AppLandscapeWindowProbe::start()
{
    if (!m_host || m_stage != IdlePortrait || m_completedCycles != 0)
        return;

    QDesktopWidget *desktop = QApplication::desktop();
    qDebug() << "WW:APP_LANDSCAPE_PROBE_BEGIN"
             << "version" << "appshell7-final-dynamic-fullscreen-rgb565"
             << "targetCycles" << m_targetCycles
             << "host" << static_cast<void *>(m_host)
             << "available" << appProbeRectText(
                    desktop->availableGeometry(0))
             << "physical" << appProbeRectText(
                    desktop->screenGeometry(0));
    m_portraitFullscreenRestoring = false;
    m_stage = WaitingPortraitFullscreen;
    m_timeout.start(6000);
    m_host->setLandscapeWindowProbeActive(true);
    qDebug() << "WW:APP_LANDSCAPE_PROBE_INITIAL_FULLSCREEN_REQUEST";
    m_host->showFullScreen();
    m_host->raise();
    m_host->activateWindow();
    QApplication::setActiveWindow(m_host);
    m_host->setFocus(Qt::ActiveWindowFocusReason);
    m_host->updateGL();
    QTimer::singleShot(0, this, SLOT(completePortraitFullscreen()));
}

void AppLandscapeWindowProbe::beginCycle()
{
    if (!m_host || m_stage != IdlePortrait
        || m_completedCycles >= m_targetCycles)
        return;

    m_landscapeWorkAreaSeen = false;
    m_portraitWorkAreaSeen = false;
    m_stopAfterRestore = false;

    m_landscapeWindow = new AppLandscapeProbeWindow(
        this, m_completedCycles + 1, m_targetCycles);
    m_stage = WaitingPortraitChrome;
    qDebug() << "WW:APP_LANDSCAPE_PROBE_WINDOW_CREATED"
             << static_cast<void *>(m_landscapeWindow)
             << "host" << static_cast<void *>(m_host)
             << "cycle" << (m_completedCycles + 1);

    // Leave Qt fullscreen while still in stable portrait. This restores the
    // constructed Avkon panes for the orientation transition without ever
    // destroying them. Keep the QGL host visible so Belle does not background
    // the process while waiting for workAreaResized().
    m_host->setLandscapeWindowProbeActive(true);
    m_timeout.start(6000);
    qDebug() << "WW:APP_LANDSCAPE_PROBE_PORTRAIT_CHROME_REQUEST"
             << "hostVisible" << m_host->isVisible();
    m_host->showMaximized();
    m_host->raise();
    m_host->activateWindow();
    QApplication::setActiveWindow(m_host);
    m_host->setFocus(Qt::ActiveWindowFocusReason);
    m_host->updateGL();
}

void AppLandscapeWindowProbe::requestLandscapeOrientation()
{
    if (m_stage != WaitingPortraitChrome || !m_landscapeWindow)
        return;

    m_stage = WaitingLandscapeWorkArea;
    m_timeout.start(6000);
    const int error = requestAppProbeOrientation(true);
    qDebug() << "WW:APP_LANDSCAPE_PROBE_LANDSCAPE_REQUEST"
             << "error" << error;
    if (error != 0) {
        m_stopAfterRestore = true;
        requestPortraitRestore();
    }
}

void AppLandscapeWindowProbe::onWorkAreaResized(int screen)
{
    QDesktopWidget *desktop = QApplication::desktop();
    const QRect available = desktop->availableGeometry(screen);
    const QRect physical = desktop->screenGeometry(screen);
    qDebug() << "WW:APP_LANDSCAPE_PROBE_WORKAREA"
             << "stage" << static_cast<int>(m_stage)
             << "screen" << screen
             << "available" << appProbeRectText(available)
             << "physical" << appProbeRectText(physical);

    if (m_stage == WaitingPortraitChrome
        && available.height() > available.width()
        && physical == QRect(0, 0, 360, 640)
        && available != QRect(0, 0, 360, 640)) {
        m_timeout.stop();
        qDebug() << "WW:APP_LANDSCAPE_PROBE_PORTRAIT_CHROME_READY"
                 << "available" << appProbeRectText(available)
                 << "physical" << appProbeRectText(physical);
        QTimer::singleShot(0, this, SLOT(requestLandscapeOrientation()));
    } else if (m_stage == WaitingLandscapeWorkArea
        && available.width() > available.height()
        && physical == QRect(0, 0, 640, 360)) {
        m_landscapeWorkAreaSeen = true;
        qDebug() << "WW:APP_LANDSCAPE_PROBE_WORKAREA_640X360_READY";
        QTimer::singleShot(0, this, SLOT(commitLandscapeWindow()));
    } else if (m_stage == WaitingPortraitWorkArea
               && available.height() > available.width()
               && physical == QRect(0, 0, 360, 640)) {
        m_portraitWorkAreaSeen = true;
        QTimer::singleShot(0, this, SLOT(commitPortraitWindow()));
    } else if (m_stage == WaitingPortraitFullscreen
               && available == QRect(0, 0, 360, 640)
               && physical == QRect(0, 0, 360, 640)) {
        QTimer::singleShot(0, this, SLOT(completePortraitFullscreen()));
    }
}

void AppLandscapeWindowProbe::onScreenResized(int screen)
{
    QDesktopWidget *desktop = QApplication::desktop();
    qDebug() << "WW:APP_LANDSCAPE_PROBE_SCREEN_RESIZED"
             << "stage" << static_cast<int>(m_stage)
             << "screen" << screen
             << "available" << appProbeRectText(
                    desktop->availableGeometry(screen))
             << "physical" << appProbeRectText(
                    desktop->screenGeometry(screen));
}

void AppLandscapeWindowProbe::commitLandscapeWindow()
{
    if (m_stage != WaitingLandscapeWorkArea
        || !m_landscapeWorkAreaSeen || !m_landscapeWindow)
        return;

    QDesktopWidget *desktop = QApplication::desktop();
    const QRect available = desktop->availableGeometry(0);
    const QRect physical = desktop->screenGeometry(0);
    // Match the standalone combined1 success condition: do not create a
    // fullscreen surface until Qt reports that the native screen itself has
    // rotated. availableGeometry may still reserve Avkon chrome here; the
    // fullscreen window removes it after showFullScreen().
    if (available.width() <= available.height()
        || physical != QRect(0, 0, 640, 360)) {
        qDebug() << "WW:APP_LANDSCAPE_PROBE_COMMIT_DEFER"
                 << "available" << appProbeRectText(available)
                 << "physical" << appProbeRectText(physical);
        return;
    }

    m_timeout.stop();
    m_stage = LandscapeVisible;
    qDebug() << "WW:APP_LANDSCAPE_PROBE_SHOW_BEGIN"
             << "available" << appProbeRectText(available)
             << "physical" << appProbeRectText(physical)
             << "hostVisible" << m_host->isVisible();

    m_landscapeWindow->showFullScreen();
    m_landscapeWindow->raise();
    m_landscapeWindow->activateWindow();
    m_landscapeWindow->setFocus(Qt::OtherFocusReason);
    m_landscapeWindow->update();

    qDebug() << "WW:APP_LANDSCAPE_PROBE_VISIBLE"
             << "geometry" << appProbeRectText(
                    m_landscapeWindow->geometry())
             << "fullscreen" << m_landscapeWindow->isFullScreen()
             << "hostVisible" << m_host->isVisible()
             << "hostKeptForeground" << true;
}

void AppLandscapeWindowProbe::requestPortraitRestore()
{
    if (m_stage == DeletingLandscapeWindow
        || m_stage == WaitingPortraitWorkArea
        || m_stage == WaitingPortraitFullscreen
        || m_stage == ProbeComplete)
        return;

    qDebug() << "WW:APP_LANDSCAPE_PROBE_EXIT_REQUEST"
             << "stage" << static_cast<int>(m_stage);
    m_stage = DeletingLandscapeWindow;
    QTimer::singleShot(0, this, SLOT(beginPortraitRestore()));
}

void AppLandscapeWindowProbe::beginPortraitRestore()
{
    if (m_stage != DeletingLandscapeWindow)
        return;

    m_timeout.stop();
    deleteLandscapeWindow();
    m_portraitWorkAreaSeen = false;
    m_stage = WaitingPortraitWorkArea;
    m_timeout.start(6000);
    const int error = requestAppProbeOrientation(false);
    qDebug() << "WW:APP_LANDSCAPE_PROBE_PORTRAIT_REQUEST"
             << "error" << error;
    if (error != 0)
        forcePortraitHostVisible(QString::fromLatin1(
            "portrait-request-failed"));
}

void AppLandscapeWindowProbe::commitPortraitWindow()
{
    if (m_stage != WaitingPortraitWorkArea || !m_portraitWorkAreaSeen)
        return;

    QDesktopWidget *desktop = QApplication::desktop();
    const QRect available = desktop->availableGeometry(0);
    const QRect physical = desktop->screenGeometry(0);
    // With Avkon panes constructed, the stable portrait work area reserves
    // status/control-pane space (observed as 0,26 360x554). Orientation is
    // complete when the native screen is 360x640 and the work area is
    // portrait; requiring a pane-free 360x640 rectangle creates a false
    // timeout and makes the diagnosis app quit intentionally.
    if (available.height() <= available.width()
        || physical != QRect(0, 0, 360, 640)) {
        qDebug() << "WW:APP_LANDSCAPE_PROBE_PORTRAIT_COMMIT_DEFER"
                 << "available" << appProbeRectText(available)
                 << "physical" << appProbeRectText(physical);
        return;
    }

    m_timeout.stop();
    m_stage = WaitingPortraitFullscreen;
    m_portraitFullscreenRestoring = true;
    m_host->showMaximized();
    m_host->raise();
    m_host->activateWindow();
    QApplication::setActiveWindow(m_host);
    m_host->setFocus(Qt::ActiveWindowFocusReason);
    m_host->updateGL();

    // Keep the foreground guard active while showFullScreen() changes the
    // native QGL window. Otherwise the ordinary activation timer calls
    // showMaximized() and immediately cancels this transition.
    m_timeout.start(6000);
    qDebug() << "WW:APP_LANDSCAPE_PROBE_PORTRAIT_FULLSCREEN_REQUEST"
             << "cycle" << (m_completedCycles + 1)
             << "available" << appProbeRectText(available)
             << "physical" << appProbeRectText(physical)
             << "hostVisible" << m_host->isVisible();
    m_host->showFullScreen();
    QTimer::singleShot(0, this, SLOT(completePortraitFullscreen()));
}

void AppLandscapeWindowProbe::completePortraitFullscreen()
{
    if (m_stage != WaitingPortraitFullscreen || !m_host)
        return;

    QDesktopWidget *desktop = QApplication::desktop();
    const QRect available = desktop->availableGeometry(0);
    const QRect physical = desktop->screenGeometry(0);
    if (available != QRect(0, 0, 360, 640)
        || physical != QRect(0, 0, 360, 640))
        return;

    m_timeout.stop();
    const bool restoredCycle = m_portraitFullscreenRestoring;
    m_portraitFullscreenRestoring = false;
    if (restoredCycle)
        ++m_completedCycles;
    m_stage = IdlePortrait;
    m_host->setLandscapeWindowProbeActive(false);
    m_host->updateGL();

    qDebug() << (restoredCycle
                    ? "WW:APP_LANDSCAPE_PROBE_RESTORED_FULLSCREEN"
                    : "WW:APP_LANDSCAPE_PROBE_INITIAL_FULLSCREEN_READY")
             << "cycle" << m_completedCycles
             << "available" << appProbeRectText(available)
             << "physical" << appProbeRectText(physical)
             << "hostVisible" << m_host->isVisible()
             << "fullscreen" << m_host->isFullScreen();

    if (m_stopAfterRestore) {
        m_stage = ProbeComplete;
        qDebug() << "WW:APP_LANDSCAPE_PROBE_ABORTED"
                 << "completed" << m_completedCycles;
        QTimer::singleShot(1500, qApp, SLOT(quit()));
    } else if (m_completedCycles >= m_targetCycles) {
        m_stage = ProbeComplete;
        qDebug() << "WW:APP_LANDSCAPE_PROBE_COMPLETE"
                 << "cycles" << m_completedCycles;
        QTimer::singleShot(1500, qApp, SLOT(quit()));
    } else {
        QTimer::singleShot(700, this, SLOT(beginCycle()));
    }
}

void AppLandscapeWindowProbe::onTimeout()
{
    QDesktopWidget *desktop = QApplication::desktop();
    qDebug() << "WW:APP_LANDSCAPE_PROBE_TIMEOUT"
             << "stage" << static_cast<int>(m_stage)
             << "available" << appProbeRectText(
                    desktop->availableGeometry(0))
             << "physical" << appProbeRectText(
                    desktop->screenGeometry(0));
    m_stopAfterRestore = true;
    if (m_stage == WaitingPortraitChrome
        || m_stage == WaitingLandscapeWorkArea)
        requestPortraitRestore();
    else if (m_stage == WaitingPortraitWorkArea
             || m_stage == WaitingPortraitFullscreen)
        forcePortraitHostVisible(QString::fromLatin1("portrait-timeout"));
}

void AppLandscapeWindowProbe::forcePortraitHostVisible(
    const QString &reason)
{
    m_timeout.stop();
    deleteLandscapeWindow();
    requestAppProbeOrientation(false);
    m_stage = ProbeComplete;
    if (m_host) {
        m_host->showMaximized();
        m_host->raise();
        m_host->activateWindow();
        QApplication::setActiveWindow(m_host);
        m_host->setFocus(Qt::ActiveWindowFocusReason);
        m_host->setLandscapeWindowProbeActive(false);
        m_host->updateGL();
    }
    qDebug() << "WW:APP_LANDSCAPE_PROBE_FORCED_PORTRAIT"
             << "reason" << reason
             << "completed" << m_completedCycles;
    QTimer::singleShot(1500, qApp, SLOT(quit()));
}

void AppLandscapeWindowProbe::deleteLandscapeWindow()
{
    if (!m_landscapeWindow)
        return;
    m_landscapeWindow->hide();
    delete m_landscapeWindow;
    m_landscapeWindow = 0;
    qDebug() << "WW:APP_LANDSCAPE_PROBE_WINDOW_DELETED";
}

} // namespace wiliwili
