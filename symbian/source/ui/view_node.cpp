#include "ui/view_node.h"

#include <QtCore/QByteArray>

namespace wiliwili {

NVGcolor toNvgColor(const QColor &color)
{
    return nvgRGBA(
        static_cast<unsigned char>(color.red()),
        static_cast<unsigned char>(color.green()),
        static_cast<unsigned char>(color.blue()),
        static_cast<unsigned char>(color.alpha()));
}

ViewNode::ViewNode() : m_visible(true)
{
}

ViewNode::~ViewNode()
{
}

void ViewNode::setFrame(const QRectF &frame)
{
    m_frame = frame;
}

const QRectF &ViewNode::frame() const
{
    return m_frame;
}

void ViewNode::setVisible(bool visible)
{
    m_visible = visible;
}

bool ViewNode::isVisible() const
{
    return m_visible;
}

BoxNode::BoxNode()
    : m_color(0, 0, 0, 0), m_cornerRadius(0.0f)
{
}

BoxNode::~BoxNode()
{
    int index;
    for (index = 0; index < m_children.size(); ++index)
        delete m_children.at(index);
    m_children.clear();
}

void BoxNode::setColor(const QColor &color)
{
    m_color = color;
}

void BoxNode::setCornerRadius(float radius)
{
    m_cornerRadius = radius;
}

void BoxNode::addChild(ViewNode *child)
{
    if (child)
        m_children.append(child);
}

void BoxNode::draw(NVGcontext *context)
{
    if (!isVisible())
        return;

    const QRectF &bounds = frame();
    nvgBeginPath(context);
    if (m_cornerRadius > 0.0f) {
        nvgRoundedRect(
            context,
            static_cast<float>(bounds.x()),
            static_cast<float>(bounds.y()),
            static_cast<float>(bounds.width()),
            static_cast<float>(bounds.height()),
            m_cornerRadius);
    } else {
        nvgRect(
            context,
            static_cast<float>(bounds.x()),
            static_cast<float>(bounds.y()),
            static_cast<float>(bounds.width()),
            static_cast<float>(bounds.height()));
    }
    nvgFillColor(context, toNvgColor(m_color));
    nvgFill(context);

    int index;
    for (index = 0; index < m_children.size(); ++index)
        m_children.at(index)->draw(context);
}

LabelNode::LabelNode()
    : m_color(Qt::white),
      m_fontId(-1),
      m_fontSize(16.0f),
      m_alignment(NVG_ALIGN_LEFT | NVG_ALIGN_TOP),
      m_lineHeight(1.15f),
      m_wrap(false),
      m_maxLines(0)
{
}

void LabelNode::setText(const QString &text)
{
    m_text = text;
}

void LabelNode::setColor(const QColor &color)
{
    m_color = color;
}

void LabelNode::setFont(int fontId, float fontSize)
{
    m_fontId = fontId;
    m_fontSize = fontSize;
}

void LabelNode::setAlignment(int alignment)
{
    m_alignment = alignment;
}

void LabelNode::setLineHeight(float lineHeight)
{
    m_lineHeight = lineHeight;
}

void LabelNode::setWrap(bool wrap)
{
    m_wrap = wrap;
}

void LabelNode::setMaxLines(int maxLines)
{
    m_maxLines = qMax(0, maxLines);
}

void LabelNode::draw(NVGcontext *context)
{
    if (!isVisible() || m_fontId < 0 || m_text.isEmpty())
        return;

    const QRectF &bounds = frame();
    const QByteArray utf8 = m_text.toUtf8();

    nvgFontFaceId(context, m_fontId);
    nvgFontSize(context, m_fontSize);
    nvgTextAlign(context, m_alignment);
    nvgTextLineHeight(context, m_lineHeight);
    nvgFillColor(context, toNvgColor(m_color));

    if (m_wrap && m_maxLines > 0) {
        const char *textStart = utf8.constData();
        const char *textEnd = textStart + utf8.size();
        float lineY = static_cast<float>(bounds.y());
        int lineIndex = 0;
        while (textStart < textEnd && lineIndex < m_maxLines) {
            NVGtextRow row;
            const int rowCount = nvgTextBreakLines(
                context,
                textStart,
                textEnd,
                static_cast<float>(bounds.width()),
                &row,
                1);
            if (rowCount < 1)
                break;
            nvgText(
                context,
                static_cast<float>(bounds.x()),
                lineY,
                row.start,
                row.end);
            textStart = row.next;
            lineY += m_fontSize * m_lineHeight;
            ++lineIndex;
        }
    } else if (m_wrap) {
        nvgTextBox(
            context,
            static_cast<float>(bounds.x()),
            static_cast<float>(bounds.y()),
            static_cast<float>(bounds.width()),
            utf8.constData(),
            0);
    } else {
        nvgText(
            context,
            static_cast<float>(bounds.x()),
            static_cast<float>(bounds.y()),
            utf8.constData(),
            0);
    }
}

ImageNode::ImageNode()
    : m_imageHandle(-1),
      m_cornerRadius(0.0f),
      m_fallbackColor(89, 72, 117, 255)
{
}

void ImageNode::setImageHandle(int imageHandle)
{
    m_imageHandle = imageHandle;
}

void ImageNode::setCornerRadius(float radius)
{
    m_cornerRadius = radius;
}

void ImageNode::setFallbackColor(const QColor &color)
{
    m_fallbackColor = color;
}

void ImageNode::draw(NVGcontext *context)
{
    if (!isVisible())
        return;

    const QRectF &bounds = frame();
    nvgBeginPath(context);
    nvgRoundedRect(
        context,
        static_cast<float>(bounds.x()),
        static_cast<float>(bounds.y()),
        static_cast<float>(bounds.width()),
        static_cast<float>(bounds.height()),
        m_cornerRadius);

    if (m_imageHandle >= 0) {
        NVGpaint image = nvgImagePattern(
            context,
            static_cast<float>(bounds.x()),
            static_cast<float>(bounds.y()),
            static_cast<float>(bounds.width()),
            static_cast<float>(bounds.height()),
            0.0f,
            m_imageHandle,
            1.0f);
        nvgFillPaint(context, image);
    } else {
        nvgFillColor(context, toNvgColor(m_fallbackColor));
    }
    nvgFill(context);
}

} // namespace wiliwili
