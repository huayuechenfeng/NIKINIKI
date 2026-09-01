#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QTime>
#include <QtCore/QTimerEvent>
#include <QtGui/QApplication>

#include "anna_probe_ui.h"

namespace {

class FontProbeWindow : public nikiniki_anna_probe::ReportWindow
{
public:
    FontProbeWindow()
        : ReportWindow(QString::fromLatin1("NIKI A3 Font")),
          m_timerId(0)
    {
        QStringList lines = nikiniki_anna_probe::baseLines();
        lines << QString::fromLatin1("8.36 MB cold read starts in 2 seconds.");
        setReport(QString::fromLatin1("A3 FONT LOADER PASS - WAIT"), lines, true);
        m_timerId = startTimer(2000);
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

        const QString subpath = QString::fromLatin1(
            "/resource/apps/nikiniki_anna_a3_font/switch_font.ttf");
        QStringList candidates;
        const QString appPath = QApplication::applicationFilePath();
        if (appPath.size() >= 2 && appPath.at(1) == QLatin1Char(':'))
            candidates << appPath.left(2) + subpath;
        candidates << QString::fromLatin1("C:") + subpath
                   << QString::fromLatin1("E:") + subpath
                   << QString::fromLatin1("F:") + subpath;

        QString path;
        int index;
        for (index = 0; index < candidates.size(); ++index) {
            if (QFile::exists(candidates.at(index))) {
                path = candidates.at(index);
                break;
            }
        }

        QStringList lines = nikiniki_anna_probe::baseLines();
        if (path.isEmpty()) {
            lines << QString::fromLatin1("Font file: NOT FOUND");
            setReport(QString::fromLatin1("A3 FONT DEPLOY FAIL"), lines, false);
            event->accept();
            return;
        }

        const int freeBefore = nikiniki_anna_probe::ramBytes(
            HALData::EMemoryRAMFree);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            lines << QString::fromLatin1("Open failed: %1").arg(path);
            setReport(QString::fromLatin1("A3 FONT OPEN FAIL"), lines, false);
            event->accept();
            return;
        }

        const qint64 expected = file.size();
        QTime clock;
        clock.start();
        m_fontData.resize(static_cast<int>(expected));
        const qint64 bytesRead = file.read(m_fontData.data(), expected);
        file.close();
        const int elapsed = clock.elapsed();
        const int freeAfter = nikiniki_anna_probe::ramBytes(
            HALData::EMemoryRAMFree);
        const bool pass = expected > 0 && bytesRead == expected;

        lines << QString::fromLatin1("Font path: %1").arg(path.left(52));
        lines << QString::fromLatin1("Expected/read: %1 / %2")
                     .arg(expected).arg(bytesRead);
        lines << QString::fromLatin1("Read time: %1 ms").arg(elapsed);
        lines << QString::fromLatin1("RAM before/after: %1 / %2")
                     .arg(nikiniki_anna_probe::megabytes(freeBefore))
                     .arg(nikiniki_anna_probe::megabytes(freeAfter));
        setReport(
            pass ? QString::fromLatin1("A3 FONT COLD READ PASS")
                 : QString::fromLatin1("A3 FONT COLD READ FAIL"),
            lines,
            pass);
        event->accept();
    }

private:
    int m_timerId;
    QByteArray m_fontData;
};

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QString::fromLatin1("NIKI A3 Font"));
    FontProbeWindow window;
    window.showMaximized();
    return application.exec();
}
