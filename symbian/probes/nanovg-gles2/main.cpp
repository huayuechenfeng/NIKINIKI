#include <QtCore/QResource>
#include <QtCore/QtGlobal>
#include <QtGui/QApplication>
#include <QtGui/QMouseEvent>
#include <QtOpenGL/QGLWidget>

#include <GLES2/gl2.h>

#if defined(Q_OS_SYMBIAN)
// The Belle GLES2 header uses char in its APIs but omits the standard alias.
typedef char GLchar;
#endif

#include "nanovg.h"
#define NANOVG_GLES2_IMPLEMENTATION
#include "nanovg_gl.h"

class NanoVgGateWidget : public QGLWidget
{
public:
    NanoVgGateWidget()
        : m_vg(0),
          m_font(-1),
          m_image(-1),
          m_scroll(0.0f),
          m_selected(0),
          m_dragging(false)
    {
        setWindowTitle(QString::fromLatin1("NIKINIKI NanoVG GLES2 gate"));
        setAutoFillBackground(false);
        setAttribute(Qt::WA_AcceptTouchEvents, true);
    }

    ~NanoVgGateWidget()
    {
        if (m_vg) {
            makeCurrent();
            nvgDeleteGLES2(m_vg);
            m_vg = 0;
        }
    }

protected:
    void initializeGL()
    {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glClearColor(0.035f, 0.025f, 0.055f, 1.0f);

        m_vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        if (!m_vg)
            return;

        QResource fontResource(QString::fromLatin1(":/assets/switch_font.ttf"));
        if (fontResource.isValid()) {
            m_font = nvgCreateFontMem(
                m_vg,
                "ui",
                const_cast<unsigned char *>(fontResource.data()),
                static_cast<int>(fontResource.size()),
                0);
        }

        QResource imageResource(QString::fromLatin1(":/assets/nikiniki_icon.png"));
        if (imageResource.isValid()) {
            m_image = nvgCreateImageMem(
                m_vg,
                0,
                const_cast<unsigned char *>(imageResource.data()),
                static_cast<int>(imageResource.size()));
        }
    }

    void resizeGL(int width, int height)
    {
        glViewport(0, 0, width, height);
    }

    void paintGL()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        if (!m_vg)
            return;

        const float screenWidth = static_cast<float>(width());
        const float screenHeight = static_cast<float>(height());
        nvgBeginFrame(m_vg, screenWidth, screenHeight, 1.0f);

        drawBackground(screenWidth, screenHeight);
        drawHeader(screenWidth);
        drawCards(screenWidth, screenHeight);
        drawFooter(screenWidth, screenHeight);

        nvgEndFrame(m_vg);
    }

    void mousePressEvent(QMouseEvent *event)
    {
        m_dragging = true;
        m_lastPosition = event->pos();
        m_pressPosition = event->pos();
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event)
    {
        if (!m_dragging)
            return;

        const int delta = event->pos().y() - m_lastPosition.y();
        m_scroll += static_cast<float>(delta);
        if (m_scroll > 0.0f)
            m_scroll = 0.0f;
        if (m_scroll < -260.0f)
            m_scroll = -260.0f;
        m_lastPosition = event->pos();
        updateGL();
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event)
    {
        const int movement = (event->pos() - m_pressPosition).manhattanLength();
        if (movement < 18) {
            const float cardTop = 108.0f + m_scroll;
            const int index = static_cast<int>((event->pos().y() - cardTop) / 92.0f);
            if (index >= 0 && index < 6)
                m_selected = index;
        }
        m_dragging = false;
        updateGL();
        event->accept();
    }

private:
    void drawBackground(float width, float height)
    {
        NVGpaint background = nvgLinearGradient(
            m_vg,
            0.0f,
            0.0f,
            width,
            height,
            nvgRGBA(24, 14, 38, 255),
            nvgRGBA(8, 18, 34, 255));
        nvgBeginPath(m_vg);
        nvgRect(m_vg, 0.0f, 0.0f, width, height);
        nvgFillPaint(m_vg, background);
        nvgFill(m_vg);
    }

    void drawHeader(float width)
    {
        nvgBeginPath(m_vg);
        nvgRect(m_vg, 0.0f, 0.0f, width, 94.0f);
        nvgFillColor(m_vg, nvgRGBA(43, 30, 63, 245));
        nvgFill(m_vg);

        if (m_image >= 0) {
            NVGpaint logo = nvgImagePattern(m_vg, 16.0f, 15.0f, 62.0f, 62.0f, 0.0f, m_image, 1.0f);
            nvgBeginPath(m_vg);
            nvgRoundedRect(m_vg, 16.0f, 15.0f, 62.0f, 62.0f, 12.0f);
            nvgFillPaint(m_vg, logo);
            nvgFill(m_vg);
        }

        if (m_font >= 0) {
            nvgFontFaceId(m_vg, m_font);
            nvgTextAlign(m_vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFontSize(m_vg, 24.0f);
            nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
            nvgText(m_vg, 92.0f, 35.0f, "NIKINIKI Symbian", 0);
            nvgFontSize(m_vg, 14.0f);
            nvgFillColor(m_vg, nvgRGBA(199, 184, 222, 255));
            nvgText(m_vg, 92.0f, 63.0f, "upstream NanoVG / GLES2 gate", 0);
        }
    }

    void drawCards(float width, float height)
    {
        static const char *titles[] = {
            "Recommended video",
            "Popular this week",
            "Continue watching",
            "Following creators",
            "Bangumi update",
            "Search Bilibili"
        };

        const float cardWidth = width - 24.0f;
        int index;
        for (index = 0; index < 6; ++index) {
            const float y = 108.0f + m_scroll + index * 92.0f;
            if (y < 88.0f || y > height - 38.0f)
                continue;

            nvgBeginPath(m_vg);
            nvgRoundedRect(m_vg, 12.0f, y, cardWidth, 78.0f, 10.0f);
            nvgFillColor(
                m_vg,
                index == m_selected ? nvgRGBA(225, 68, 133, 255)
                                    : nvgRGBA(54, 45, 74, 238));
            nvgFill(m_vg);

            nvgBeginPath(m_vg);
            nvgRoundedRect(m_vg, 24.0f, y + 12.0f, 78.0f, 54.0f, 7.0f);
            nvgFillColor(m_vg, nvgRGBA(112, 88, 151, 255));
            nvgFill(m_vg);

            if (m_font >= 0) {
                nvgFontFaceId(m_vg, m_font);
                nvgTextAlign(m_vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                nvgFontSize(m_vg, 17.0f);
                nvgFillColor(m_vg, nvgRGBA(255, 255, 255, 255));
                nvgText(m_vg, 116.0f, y + 29.0f, titles[index], 0);
                nvgFontSize(m_vg, 12.0f);
                nvgFillColor(m_vg, nvgRGBA(221, 211, 235, 255));
                nvgText(m_vg, 116.0f, y + 53.0f, "touch to select / drag to scroll", 0);
            }
        }
    }

    void drawFooter(float width, float height)
    {
        nvgBeginPath(m_vg);
        nvgRect(m_vg, 0.0f, height - 30.0f, width, 30.0f);
        nvgFillColor(m_vg, nvgRGBA(18, 14, 28, 245));
        nvgFill(m_vg);

        if (m_font >= 0) {
            char status[64];
            qsnprintf(status, sizeof(status), "selected card: %d", m_selected + 1);
            nvgFontFaceId(m_vg, m_font);
            nvgFontSize(m_vg, 13.0f);
            nvgTextAlign(m_vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(m_vg, nvgRGBA(202, 190, 219, 255));
            nvgText(m_vg, width * 0.5f, height - 15.0f, status, 0);
        }
    }

    NVGcontext *m_vg;
    int m_font;
    int m_image;
    float m_scroll;
    int m_selected;
    bool m_dragging;
    QPoint m_lastPosition;
    QPoint m_pressPosition;
};

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    NanoVgGateWidget widget;
    widget.showMaximized();
    return application.exec();
}
