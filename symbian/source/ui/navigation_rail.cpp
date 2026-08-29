#include "ui/navigation_rail.h"

#include <QtCore/QByteArray>

namespace wiliwili {

static const float kBarHeight = 60.0f;

NavigationRail::NavigationRail()
    : m_fontId(-1),
      m_logoHandle(-1),
      m_selected(HomeSection),
      m_pressed(NoSection),
      m_lastWidth(360.0f),
      m_lastHeight(640.0f)
{
}

void NavigationRail::initialize(int fontId, int logoHandle)
{
    m_fontId = fontId;
    m_logoHandle = logoHandle;
}

void NavigationRail::setSelected(Section section)
{
    if (section != NoSection)
        m_selected = section;
}

NavigationRail::Section NavigationRail::selected() const
{
    return m_selected;
}

float NavigationRail::width()
{
    return 0.0f;
}

float NavigationRail::height()
{
    return kBarHeight;
}

QRectF NavigationRail::hitBox(
    Section section, float width, float height) const
{
    if (section < HomeSection || section > SettingsSection)
        return QRectF();
    const float itemWidth = width / 5.0f;
    return QRectF(
        static_cast<int>(section) * itemWidth,
        height - kBarHeight,
        itemWidth,
        kBarHeight);
}

NavigationRail::Section NavigationRail::sectionAt(
    const QPoint &position,
    float width,
    float height) const
{
    int section;
    for (section = HomeSection; section <= SettingsSection; ++section) {
        const Section value = static_cast<Section>(section);
        if (hitBox(value, width, height).contains(QPointF(position)))
            return value;
    }
    return NoSection;
}

bool NavigationRail::contains(const QPoint &position) const
{
    return position.x() >= 0 && position.x() < m_lastWidth &&
           position.y() >= m_lastHeight - kBarHeight &&
           position.y() < m_lastHeight;
}

void NavigationRail::pointerPress(const QPoint &position)
{
    m_pressed = sectionAt(position, m_lastWidth, m_lastHeight);
}

NavigationRail::Section NavigationRail::pointerRelease(
    const QPoint &position)
{
    const Section released = sectionAt(
        position, m_lastWidth, m_lastHeight);
    const Section result = released == m_pressed ? released : NoSection;
    m_pressed = NoSection;
    return result;
}

void NavigationRail::drawLabel(
    NVGcontext *context,
    const char *text,
    float x,
    float y,
    const NVGcolor &color) const
{
    if (m_fontId < 0)
        return;
    nvgFontFaceId(context, m_fontId);
    nvgFontSize(context, 8.3f);
    nvgTextAlign(context, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(context, color);
    nvgText(context, x, y, text, 0);
}

void NavigationRail::drawIcon(
    NVGcontext *context,
    Section section,
    float x,
    float y,
    const NVGcolor &color) const
{
    nvgStrokeColor(context, color);
    nvgFillColor(context, color);
    nvgStrokeWidth(context, 1.8f);
    nvgLineCap(context, NVG_ROUND);
    nvgLineJoin(context, NVG_ROUND);

    if (section == HomeSection) {
        nvgBeginPath(context);
        nvgMoveTo(context, x - 8.0f, y - 1.0f);
        nvgLineTo(context, x, y - 8.0f);
        nvgLineTo(context, x + 8.0f, y - 1.0f);
        nvgMoveTo(context, x - 6.0f, y - 2.0f);
        nvgLineTo(context, x - 6.0f, y + 8.0f);
        nvgLineTo(context, x + 6.0f, y + 8.0f);
        nvgLineTo(context, x + 6.0f, y - 2.0f);
        nvgStroke(context);
    } else if (section == DynamicSection) {
        nvgBeginPath(context);
        nvgCircle(context, x, y, 8.0f);
        nvgStroke(context);
        nvgBeginPath(context);
        nvgMoveTo(context, x + 1.0f, y - 8.0f);
        nvgLineTo(context, x - 3.0f, y);
        nvgLineTo(context, x + 2.0f, y);
        nvgLineTo(context, x - 1.0f, y + 8.0f);
        nvgLineTo(context, x + 5.0f, y - 1.0f);
        nvgLineTo(context, x, y - 1.0f);
        nvgClosePath(context);
        nvgFill(context);
    } else if (section == AccountSection) {
        nvgBeginPath(context);
        nvgCircle(context, x, y - 5.0f, 4.2f);
        nvgStroke(context);
        nvgBeginPath(context);
        nvgRoundedRect(context, x - 7.0f, y + 1.0f, 14.0f, 8.0f, 4.0f);
        nvgStroke(context);
    } else if (section == MessagesSection) {
        nvgBeginPath(context);
        nvgRoundedRect(context, x - 8.0f, y - 6.0f, 16.0f, 12.0f, 2.0f);
        nvgMoveTo(context, x - 7.0f, y - 4.0f);
        nvgLineTo(context, x, y + 1.0f);
        nvgLineTo(context, x + 7.0f, y - 4.0f);
        nvgStroke(context);
    } else if (section == SettingsSection) {
        nvgBeginPath(context);
        nvgCircle(context, x, y, 7.0f);
        nvgCircle(context, x, y, 2.4f);
        nvgStroke(context);
        static const float directions[8][2] = {
            { 1.0f, 0.0f }, { 0.7071f, 0.7071f },
            { 0.0f, 1.0f }, { -0.7071f, 0.7071f },
            { -1.0f, 0.0f }, { -0.7071f, -0.7071f },
            { 0.0f, -1.0f }, { 0.7071f, -0.7071f }
        };
        int spoke;
        for (spoke = 0; spoke < 8; ++spoke) {
            const float ax = directions[spoke][0];
            const float ay = directions[spoke][1];
            nvgBeginPath(context);
            nvgMoveTo(context, x + ax * 8.0f, y + ay * 8.0f);
            nvgLineTo(context, x + ax * 10.0f, y + ay * 10.0f);
            nvgStroke(context);
        }
    }
}

void NavigationRail::draw(NVGcontext *context, float width, float height)
{
    m_lastWidth = width;
    m_lastHeight = height;

    nvgBeginPath(context);
    nvgRect(context, 0.0f, height - kBarHeight, width, kBarHeight);
    nvgFillColor(context, nvgRGBA(24, 24, 31, 252));
    nvgFill(context);

    nvgBeginPath(context);
    nvgMoveTo(context, 0.0f, height - kBarHeight + 0.5f);
    nvgLineTo(context, width, height - kBarHeight + 0.5f);
    nvgStrokeWidth(context, 1.0f);
    nvgStrokeColor(context, nvgRGBA(255, 255, 255, 16));
    nvgStroke(context);

    static const char *labels[] = {
        "首页", "动态", "我的", "消息", "设置"
    };
    int section;
    for (section = HomeSection; section <= SettingsSection; ++section) {
        const Section value = static_cast<Section>(section);
        const QRectF box = hitBox(value, width, height);
        const bool selected = value == m_selected;
        const float centerX = static_cast<float>(box.center().x());
        const float iconY = height - 37.0f;
        const NVGcolor color = selected
            ? nvgRGBA(251, 114, 153, 255)
            : nvgRGBA(172, 172, 184, 230);

        if (selected) {
            nvgBeginPath(context);
            nvgRoundedRect(context,
                           static_cast<float>(box.x()) + 5.0f,
                           static_cast<float>(box.y()) + 4.0f,
                           static_cast<float>(box.width()) - 10.0f,
                           static_cast<float>(box.height()) - 8.0f,
                           11.0f);
            nvgFillColor(context, nvgRGBA(251, 114, 153, 24));
            nvgFill(context);
            nvgBeginPath(context);
            nvgRoundedRect(context,
                           static_cast<float>(box.center().x()) - 13.0f,
                           height - kBarHeight,
                           26.0f,
                           3.0f,
                           1.5f);
            nvgFillColor(context, color);
            nvgFill(context);
        }

        drawIcon(context, value, centerX, iconY, color);
        drawLabel(context, labels[section], centerX, iconY + 17.0f, color);
    }
}

} // namespace wiliwili
