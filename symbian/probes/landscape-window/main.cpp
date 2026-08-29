#include <QtCore/QDebug>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtGui/QApplication>
#include <QtGui/QCloseEvent>
#include <QtGui/QDesktopWidget>
#include <QtGui/QKeyEvent>
#include <QtGui/QLabel>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPushButton>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

#ifdef Q_OS_SYMBIAN
#include <aknappui.h>
#include <eikenv.h>
#endif

namespace {

static QString rectText(const QRect &rect)
{
    return QString::fromLatin1("%1,%2 %3x%4")
        .arg(rect.x())
        .arg(rect.y())
        .arg(rect.width())
        .arg(rect.height());
}

static int requestNativeOrientation(bool landscape)
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

class LandscapeProbeWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LandscapeProbeWindow(int cycle)
        : QWidget(0, Qt::Window | Qt::FramelessWindowHint)
        , m_cycle(cycle)
        , m_firstPaintDone(false)
    {
        setObjectName(QString::fromLatin1("LandscapeProbeWindow"));
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::StrongFocus);
    }

signals:
    void exitRequested();

protected:
    void paintEvent(QPaintEvent *)
    {
        const bool firstPaint = !m_firstPaintDone;
        if (firstPaint) {
            qDebug() << "WW:LANDSCAPE_PROBE_FIRST_PAINT_BEGIN"
                     << "geometry" << rectText(geometry());
        }

        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);

        QFont titleFont = painter.font();
        titleFont.setBold(true);
        titleFont.setPointSize(18);
        painter.setFont(titleFont);
        painter.setPen(Qt::white);
        painter.drawText(QRect(24, 34, width() - 48, 50),
                         Qt::AlignCenter,
                         QString::fromLatin1("NATIVE LANDSCAPE WINDOW"));

        QFont bodyFont = painter.font();
        bodyFont.setBold(false);
        bodyFont.setPointSize(11);
        painter.setFont(bodyFont);
        painter.setPen(Qt::white);
        painter.drawText(QRect(42, 100, width() - 84, height() - 150),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         QString::fromLatin1(
                             "Cycle %1\n\n"
                             "Opaque disposable top-level QWidget.\n"
                             "Black background and text only.\n\n"
                             "Tap anywhere, or press Back, to delete this window "
                             "and restore portrait.")
                             .arg(m_cycle));
        painter.end();

        if (firstPaint) {
            m_firstPaintDone = true;
            qDebug() << "WW:LANDSCAPE_PROBE_FIRST_PAINT_END"
                     << "geometry" << rectText(geometry());
        }
    }

    void mouseReleaseEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton) {
            event->accept();
            emit exitRequested();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void keyPressEvent(QKeyEvent *event)
    {
        if (event->key() == Qt::Key_Backspace
            || event->key() == Qt::Key_Escape
            || event->key() == Qt::Key_Back
            || event->key() == Qt::Key_No) {
            event->accept();
            emit exitRequested();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void closeEvent(QCloseEvent *event)
    {
        event->ignore();
        emit exitRequested();
    }

private:
    int m_cycle;
    bool m_firstPaintDone;
};

class ProbeControllerWindow : public QWidget
{
    Q_OBJECT

public:
    ProbeControllerWindow()
        : QWidget(0)
        , m_stage(IdlePortrait)
        , m_landscapeWindow(0)
        , m_statusLabel(0)
        , m_startButton(0)
        , m_timeout(this)
        , m_landscapeWorkAreaSeen(false)
        , m_portraitWorkAreaSeen(false)
        , m_fullscreenWorkAreaConfirmed(false)
        , m_completedCycles(0)
    {
        setWindowTitle(QString::fromLatin1("Landscape window probe"));

        QLabel *title = new QLabel(
            QString::fromLatin1("Symbian native landscape window probe"), this);
        QFont titleFont = title->font();
        titleFont.setBold(true);
        titleFont.setPointSize(14);
        title->setFont(titleFont);
        title->setAlignment(Qt::AlignCenter);

        QLabel *description = new QLabel(
            QString::fromLatin1(
                "This diagnostic changes the application orientation, but keeps "
                "the main window out of the landscape paint path. The landscape "
                "window is created fresh for every cycle and is deleted before "
                "portrait is restored."),
            this);
        description->setWordWrap(true);

        m_statusLabel = new QLabel(this);
        m_statusLabel->setWordWrap(true);
        m_statusLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
        m_statusLabel->setMinimumHeight(100);

        m_startButton = new QPushButton(
            QString::fromLatin1("Start one landscape cycle"), this);
        QPushButton *quitButton = new QPushButton(
            QString::fromLatin1("Exit probe"), this);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(14);
        layout->addWidget(title);
        layout->addWidget(description);
        layout->addWidget(m_statusLabel, 1);
        layout->addWidget(m_startButton);
        layout->addWidget(quitButton);

        QDesktopWidget *desktop = QApplication::desktop();
        connect(desktop, SIGNAL(workAreaResized(int)),
                this, SLOT(onWorkAreaResized(int)));
        connect(desktop, SIGNAL(resized(int)),
                this, SLOT(onScreenResized(int)));
        connect(m_startButton, SIGNAL(clicked()),
                this, SLOT(startLandscapeCycle()));
        connect(quitButton, SIGNAL(clicked()),
                qApp, SLOT(quit()));
        connect(&m_timeout, SIGNAL(timeout()),
                this, SLOT(onTimeout()));
        m_timeout.setSingleShot(true);

        updatePortraitStatus(QString::fromLatin1("Ready."));
        qDebug() << "WW:LANDSCAPE_PROBE_MAIN_READY"
                 << "version" << "combined1"
                 << "available" << rectText(desktop->availableGeometry(0))
                 << "screen" << rectText(desktop->screenGeometry(0));
    }

    ~ProbeControllerWindow()
    {
        if (m_landscapeWindow) {
            m_landscapeWindow->hide();
            delete m_landscapeWindow;
            m_landscapeWindow = 0;
        }

        if (m_stage != IdlePortrait)
            requestNativeOrientation(false);
    }

private slots:
    void startLandscapeCycle()
    {
        if (m_stage != IdlePortrait || m_landscapeWindow)
            return;

        m_startButton->setEnabled(false);
        m_landscapeWorkAreaSeen = false;
        m_portraitWorkAreaSeen = false;
        m_fullscreenWorkAreaConfirmed = false;

        m_landscapeWindow = new LandscapeProbeWindow(m_completedCycles + 1);
        connect(m_landscapeWindow, SIGNAL(exitRequested()),
                this, SLOT(schedulePortraitRestore()));

        m_stage = WaitingLandscapeWorkArea;
        m_statusLabel->setText(QString::fromLatin1(
            "Landscape window created but still hidden. Requesting native "
            "landscape and waiting for QDesktopWidget::workAreaResized()."));

        qDebug() << "WW:LANDSCAPE_PROBE_WINDOW_CREATED"
                 << m_landscapeWindow
                 << "cycle" << (m_completedCycles + 1);

        m_timeout.start(6000);
        const int error = requestNativeOrientation(true);
        qDebug() << "WW:LANDSCAPE_PROBE_LANDSCAPE_REQUEST"
                 << "error" << error;

        if (error != 0) {
            m_statusLabel->setText(QString::fromLatin1(
                "SetOrientationL(landscape) failed with error %1. "
                "Restoring portrait without showing the test window.")
                .arg(error));
            schedulePortraitRestore();
        }
    }

    void onWorkAreaResized(int screen)
    {
        QDesktopWidget *desktop = QApplication::desktop();
        const QRect available = desktop->availableGeometry(screen);
        const QRect physical = desktop->screenGeometry(screen);

        qDebug() << "WW:LANDSCAPE_PROBE_WORKAREA"
                 << "stage" << static_cast<int>(m_stage)
                 << "screen" << screen
                 << "available" << rectText(available)
                 << "physical" << rectText(physical);

        if (m_stage == WaitingLandscapeWorkArea
            && available.width() > available.height()) {
            m_landscapeWorkAreaSeen = true;
            QTimer::singleShot(0, this, SLOT(commitLandscapeWindow()));
        } else if (m_stage == WaitingPortraitWorkArea
                   && available.height() >= available.width()) {
            m_portraitWorkAreaSeen = true;
            QTimer::singleShot(0, this, SLOT(commitPortraitWindow()));
        } else if (m_stage == LandscapeVisible
                   && available == QRect(0, 0, 640, 360)
                   && physical == QRect(0, 0, 640, 360)
                   && !m_fullscreenWorkAreaConfirmed) {
            m_fullscreenWorkAreaConfirmed = true;
            qDebug() << "WW:LANDSCAPE_PROBE_FULLSCREEN_WORKAREA_READY"
                     << "available" << rectText(available)
                     << "physical" << rectText(physical);
        }
    }

    void onScreenResized(int screen)
    {
        QDesktopWidget *desktop = QApplication::desktop();
        qDebug() << "WW:LANDSCAPE_PROBE_SCREEN_RESIZED"
                 << "stage" << static_cast<int>(m_stage)
                 << "screen" << screen
                 << "available" << rectText(desktop->availableGeometry(screen))
                 << "physical" << rectText(desktop->screenGeometry(screen));
    }

    void commitLandscapeWindow()
    {
        if (m_stage != WaitingLandscapeWorkArea
            || !m_landscapeWorkAreaSeen
            || !m_landscapeWindow)
            return;

        QDesktopWidget *desktop = QApplication::desktop();
        const QRect available = desktop->availableGeometry(0);
        const QRect physical = desktop->screenGeometry(0);
        if (available.width() <= available.height()
            || physical != QRect(0, 0, 640, 360)) {
            qDebug() << "WW:LANDSCAPE_PROBE_COMMIT_DEFER"
                     << "available" << rectText(available)
                     << "physical" << rectText(physical);
            return;
        }

        m_timeout.stop();
        m_stage = LandscapeVisible;
        qDebug() << "WW:LANDSCAPE_PROBE_SHOW_BEGIN"
                 << "available" << rectText(available)
                 << "physical" << rectText(physical);

        m_landscapeWindow->showFullScreen();
        m_landscapeWindow->raise();
        m_landscapeWindow->activateWindow();
        m_landscapeWindow->setFocus(Qt::OtherFocusReason);
        m_landscapeWindow->update();
        hide();

        qDebug() << "WW:LANDSCAPE_PROBE_VISIBLE"
                 << "geometry" << rectText(m_landscapeWindow->geometry())
                 << "fullscreen" << m_landscapeWindow->isFullScreen();
    }

    void schedulePortraitRestore()
    {
        if (m_stage == WaitingPortraitWorkArea
            || m_stage == DeletingLandscapeWindow)
            return;

        qDebug() << "WW:LANDSCAPE_PROBE_EXIT_REQUEST"
                 << "stage" << static_cast<int>(m_stage);
        m_stage = DeletingLandscapeWindow;
        QTimer::singleShot(0, this, SLOT(beginPortraitRestore()));
    }

    void beginPortraitRestore()
    {
        if (m_stage != DeletingLandscapeWindow)
            return;

        m_timeout.stop();
        if (m_landscapeWindow) {
            m_landscapeWindow->hide();
            delete m_landscapeWindow;
            m_landscapeWindow = 0;
            qDebug() << "WW:LANDSCAPE_PROBE_WINDOW_DELETED";
        }

        m_portraitWorkAreaSeen = false;
        m_stage = WaitingPortraitWorkArea;
        m_timeout.start(6000);
        const int error = requestNativeOrientation(false);
        qDebug() << "WW:LANDSCAPE_PROBE_PORTRAIT_REQUEST"
                 << "error" << error;

        if (error != 0) {
            m_statusLabel->setText(QString::fromLatin1(
                "SetOrientationL(portrait) failed with error %1. "
                "Showing the controller so the probe cannot remain hidden.")
                .arg(error));
            forceControllerVisible(QString::fromLatin1("portrait-request-failed"));
        }
    }

    void commitPortraitWindow()
    {
        if (m_stage != WaitingPortraitWorkArea || !m_portraitWorkAreaSeen)
            return;

        QDesktopWidget *desktop = QApplication::desktop();
        const QRect available = desktop->availableGeometry(0);
        if (available.width() > available.height()) {
            qDebug() << "WW:LANDSCAPE_PROBE_PORTRAIT_COMMIT_DEFER"
                     << "available" << rectText(available);
            return;
        }

        m_timeout.stop();
        ++m_completedCycles;
        m_stage = IdlePortrait;
        showMaximized();
        raise();
        activateWindow();
        m_startButton->setEnabled(true);
        updatePortraitStatus(QString::fromLatin1(
            "Landscape window was deleted and portrait work area is stable."));

        qDebug() << "WW:LANDSCAPE_PROBE_RESTORED"
                 << "cycle" << m_completedCycles
                 << "available" << rectText(available);
    }

    void onTimeout()
    {
        QDesktopWidget *desktop = QApplication::desktop();
        qDebug() << "WW:LANDSCAPE_PROBE_TIMEOUT"
                 << "stage" << static_cast<int>(m_stage)
                 << "available" << rectText(desktop->availableGeometry(0))
                 << "screen" << rectText(desktop->screenGeometry(0));

        if (m_stage == WaitingLandscapeWorkArea) {
            m_statusLabel->setText(QString::fromLatin1(
                "Timed out waiting for a landscape workAreaResized signal. "
                "The landscape window was never shown; restoring portrait."));
            schedulePortraitRestore();
        } else if (m_stage == WaitingPortraitWorkArea) {
            m_statusLabel->setText(QString::fromLatin1(
                "Timed out waiting for a portrait workAreaResized signal. "
                "The controller is being shown as a recovery measure."));
            forceControllerVisible(QString::fromLatin1("portrait-timeout"));
        }
    }

private:
    enum Stage {
        IdlePortrait = 0,
        WaitingLandscapeWorkArea = 1,
        LandscapeVisible = 2,
        DeletingLandscapeWindow = 3,
        WaitingPortraitWorkArea = 4
    };

    void forceControllerVisible(const QString &reason)
    {
        m_timeout.stop();
        m_stage = IdlePortrait;
        showMaximized();
        raise();
        activateWindow();
        m_startButton->setEnabled(true);
        qDebug() << "WW:LANDSCAPE_PROBE_FORCED_CONTROLLER"
                 << "reason" << reason;
    }

    void updatePortraitStatus(const QString &message)
    {
        QDesktopWidget *desktop = QApplication::desktop();
        m_statusLabel->setText(QString::fromLatin1(
            "%1\n\nCompleted cycles: %2\nAvailable: %3\nScreen: %4")
            .arg(message)
            .arg(m_completedCycles)
            .arg(rectText(desktop->availableGeometry(0)))
            .arg(rectText(desktop->screenGeometry(0))));
    }

    Stage m_stage;
    LandscapeProbeWindow *m_landscapeWindow;
    QLabel *m_statusLabel;
    QPushButton *m_startButton;
    QTimer m_timeout;
    bool m_landscapeWorkAreaSeen;
    bool m_portraitWorkAreaSeen;
    bool m_fullscreenWorkAreaConfirmed;
    int m_completedCycles;
};

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(
        QString::fromLatin1("wiliwili landscape window probe"));
    application.setQuitOnLastWindowClosed(true);

    ProbeControllerWindow controller;
    controller.showMaximized();
    return application.exec();
}

#include "main.moc"
