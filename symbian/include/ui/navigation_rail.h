#ifndef WILIWILI_SYMBIAN_NAVIGATION_RAIL_H
#define WILIWILI_SYMBIAN_NAVIGATION_RAIL_H

#include <QtCore/QPoint>
#include <QtCore/QRectF>

#include "nanovg.h"

namespace wiliwili {

class NavigationRail
{
public:
    enum Section {
        HomeSection = 0,
        DynamicSection,
        AccountSection,
        MessagesSection,
        SettingsSection,
        NoSection = -1
    };

    NavigationRail();

    void initialize(int fontId, int logoHandle);
    void setSelected(Section section);
    Section selected() const;
    void draw(NVGcontext *context, float width, float height);

    void pointerPress(const QPoint &position);
    Section pointerRelease(const QPoint &position);
    bool contains(const QPoint &position) const;

    static float width();
    static float height();

private:
    QRectF hitBox(Section section, float width, float height) const;
    Section sectionAt(
        const QPoint &position, float width, float height) const;
    void drawIcon(
        NVGcontext *context,
        Section section,
        float centerX,
        float centerY,
        const NVGcolor &color) const;
    void drawLabel(
        NVGcontext *context,
        const char *text,
        float x,
        float y,
        const NVGcolor &color) const;

    int m_fontId;
    int m_logoHandle;
    Section m_selected;
    Section m_pressed;
    float m_lastWidth;
    float m_lastHeight;
};

} // namespace wiliwili

#endif
