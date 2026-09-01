#include <QtCore/QTimerEvent>
#include <QtGui/QApplication>
#include <QtOpenGL/QGLWidget>

#include <GLES2/gl2.h>

#include "anna_probe_ui.h"

namespace {

QString glText(GLenum name)
{
    const GLubyte *value = glGetString(name);
    if (!value)
        return QString::fromLatin1("null");
    return QString::fromLatin1(
        reinterpret_cast<const char *>(value)).left(56);
}

class GlProbeWindow : public nikiniki_anna_probe::ReportWindow
{
public:
    GlProbeWindow()
        : ReportWindow(QString::fromLatin1("NIKI A1 GL")),
          m_timerId(0),
          m_glWidget(0)
    {
        QStringList lines = nikiniki_anna_probe::baseLines();
        lines << QString::fromLatin1("GL stage starts in 2 seconds.");
        setReport(QString::fromLatin1("A1 GL LOADER PASS - WAIT"), lines, true);
        m_timerId = startTimer(2000);
    }

    ~GlProbeWindow()
    {
        delete m_glWidget;
        m_glWidget = 0;
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

        QGLFormat requested;
        requested.setSampleBuffers(false);
        requested.setDepth(false);
        requested.setStencil(true);
        m_glWidget = new QGLWidget(requested, 0);
        m_glWidget->setWindowTitle(QString::fromLatin1("NIKI GL worker"));
        m_glWidget->resize(8, 8);
        m_glWidget->show();
        m_glWidget->makeCurrent();

        const bool valid = m_glWidget->isValid() &&
            QGLContext::currentContext() != 0;
        QStringList lines = nikiniki_anna_probe::baseLines();
        lines << QString::fromLatin1("Context valid: %1")
                     .arg(valid ? QString::fromLatin1("YES")
                                : QString::fromLatin1("NO"));
        if (valid) {
            lines << QString::fromLatin1("GL vendor: %1").arg(
                         glText(GL_VENDOR));
            lines << QString::fromLatin1("GL renderer: %1").arg(
                         glText(GL_RENDERER));
            lines << QString::fromLatin1("GL version: %1").arg(
                         glText(GL_VERSION));
            lines << QString::fromLatin1("GL error: %1")
                         .arg(static_cast<unsigned int>(glGetError()));
            m_glWidget->doneCurrent();
        }
        m_glWidget->hide();
        showMaximized();
        raise();
        activateWindow();
        QApplication::setActiveWindow(this);
        setReport(
            valid ? QString::fromLatin1("A1 OPENGL PASS")
                  : QString::fromLatin1("A1 OPENGL FAIL"),
            lines,
            valid);
        event->accept();
    }

private:
    int m_timerId;
    QGLWidget *m_glWidget;
};

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QString::fromLatin1("NIKI A1 GL"));
    GlProbeWindow window;
    window.showMaximized();
    return application.exec();
}
