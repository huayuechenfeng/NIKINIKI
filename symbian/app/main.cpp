#include <QtGui/QApplication>
#include <QtCore/QDebug>

#ifdef WILIWILI_ENABLE_APP_LANDSCAPE_WINDOW_PROBE
#include <QtCore/QTimer>
#include "platform/app_landscape_window_probe.h"
#endif

#include "app/wiliwili_widget.h"

int main(int argc, char *argv[])
{
    qDebug() << "WW:MAIN_ENTER";
    // Keep Avkon application panes constructed. Dynamic Qt fullscreen hides
    // them during normal UI/playback, while their retained native objects are
    // required for Belle to emit a consistent workAreaResized() transition
    // before and after native landscape playback.
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(true);
    application.setApplicationName(QString::fromLatin1("NIKINIKI"));
#ifdef WILIWILI_ENABLE_APP_LANDSCAPE_WINDOW_PROBE
    qDebug() << "WW:APP_LANDSCAPE_PROBE_APPLICATION_PANES_CONSTRUCTED";
    application.setApplicationName(
        QString::fromLatin1("wiliwili app landscape probe"));
#elif defined(WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE)
    application.setApplicationName(
        QString::fromLatin1("wiliwili DevVideo direct probe"));
#endif
    qDebug() << "WW:APP_READY";
    wiliwili::WiliwiliWidget widget;
#ifdef WILIWILI_ENABLE_APP_LANDSCAPE_WINDOW_PROBE
    widget.setWindowTitle(
        QString::fromLatin1("wiliwili app landscape probe"));
#elif defined(WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE)
    widget.setWindowTitle(
        QString::fromLatin1("wiliwili DevVideo direct probe"));
#endif
    qDebug() << "WW:WIDGET_READY";
    // AppArc icon launch reaches this code earlier than a CODA debugger
    // launch.  Only show the window here: forcing raise/activate/focus before
    // the event loop is alive is unsafe in Belle's Qt 4.7 window integration.
    // The widget hides Avkon chrome after its first real frame.
    qDebug() << "WW:WINDOW_SHOW_BEGIN";
    widget.showMaximized();
    qDebug() << "WW:WINDOW_SHOW_DONE";
    qDebug() << "WW:WINDOW_SHOWN";
#ifdef WILIWILI_ENABLE_APP_LANDSCAPE_WINDOW_PROBE
    wiliwili::AppLandscapeWindowProbe landscapeProbe(&widget);
    QTimer::singleShot(1500, &landscapeProbe, SLOT(start()));
#elif defined(WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE)
    widget.scheduleDevVideoDirectProbe();
#endif
    const int result = application.exec();
    qDebug() << "WW:EVENT_LOOP_EXIT" << result;
    return result;
}
