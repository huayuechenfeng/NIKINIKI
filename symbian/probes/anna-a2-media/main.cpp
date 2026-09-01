#include <QtCore/QTimerEvent>
#include <QtGui/QApplication>

#include <qmediaplayer.h>

#include "anna_probe_ui.h"

namespace {

class MediaProbeWindow : public nikiniki_anna_probe::ReportWindow
{
public:
    MediaProbeWindow()
        : ReportWindow(QString::fromLatin1("NIKI A2 Media")),
          m_timerId(0),
          m_player(0)
    {
        QStringList lines = nikiniki_anna_probe::baseLines();
        lines << QString::fromLatin1("Media object starts in 2 seconds.");
        setReport(
            QString::fromLatin1("A2 MOBILITY LOADER PASS - WAIT"),
            lines,
            true);
        m_timerId = startTimer(2000);
    }

    ~MediaProbeWindow()
    {
        delete m_player;
        m_player = 0;
    }

protected:
    void timerEvent(QTimerEvent *event)
    {
        if (event->timerId() != m_timerId) {
            ReportWindow::timerEvent(event);
            return;
        }
        killTimer(m_timerId);
        m_timerId = 0;

        m_player = new QMediaPlayer(this);
        const bool available = m_player->isAvailable();
        QStringList lines = nikiniki_anna_probe::baseLines();
        lines << QString::fromLatin1("QMediaPlayer created: YES");
        lines << QString::fromLatin1("Media service: %1")
                     .arg(available ? QString::fromLatin1("AVAILABLE")
                                    : QString::fromLatin1("UNAVAILABLE"));
        lines << QString::fromLatin1("State/error: %1 / %2")
                     .arg(static_cast<int>(m_player->state()))
                     .arg(static_cast<int>(m_player->error()));
        const QString errorText = m_player->errorString();
        if (!errorText.isEmpty())
            lines << QString::fromLatin1("Error text: %1")
                         .arg(nikiniki_anna_probe::singleLine(errorText).left(56));
        setReport(
            available ? QString::fromLatin1("A2 MULTIMEDIA PASS")
                      : QString::fromLatin1("A2 API PASS / SERVICE FAIL"),
            lines,
            available);
        event->accept();
    }

private:
    int m_timerId;
    QMediaPlayer *m_player;
};

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QString::fromLatin1("NIKI A2 Media"));
    MediaProbeWindow window;
    window.showMaximized();
    return application.exec();
}
