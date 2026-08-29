#ifndef WILIWILI_SYMBIAN_VIEW_NODE_H
#define WILIWILI_SYMBIAN_VIEW_NODE_H

#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QColor>

#include "nanovg.h"

namespace wiliwili {

class ViewNode
{
public:
    ViewNode();
    virtual ~ViewNode();

    void setFrame(const QRectF &frame);
    const QRectF &frame() const;

    void setVisible(bool visible);
    bool isVisible() const;

    virtual void draw(NVGcontext *context) = 0;

private:
    ViewNode(const ViewNode &);
    ViewNode &operator=(const ViewNode &);

    QRectF m_frame;
    bool m_visible;
};

class BoxNode : public ViewNode
{
public:
    BoxNode();
    virtual ~BoxNode();

    void setColor(const QColor &color);
    void setCornerRadius(float radius);
    void addChild(ViewNode *child);

    virtual void draw(NVGcontext *context);

private:
    QColor m_color;
    float m_cornerRadius;
    QVector<ViewNode *> m_children;
};

class LabelNode : public ViewNode
{
public:
    LabelNode();

    void setText(const QString &text);
    void setColor(const QColor &color);
    void setFont(int fontId, float fontSize);
    void setAlignment(int alignment);
    void setLineHeight(float lineHeight);
    void setWrap(bool wrap);
    void setMaxLines(int maxLines);

    virtual void draw(NVGcontext *context);

private:
    QString m_text;
    QColor m_color;
    int m_fontId;
    float m_fontSize;
    int m_alignment;
    float m_lineHeight;
    bool m_wrap;
    int m_maxLines;
};

class ImageNode : public ViewNode
{
public:
    ImageNode();

    void setImageHandle(int imageHandle);
    void setCornerRadius(float radius);
    void setFallbackColor(const QColor &color);

    virtual void draw(NVGcontext *context);

private:
    int m_imageHandle;
    float m_cornerRadius;
    QColor m_fallbackColor;
};

NVGcolor toNvgColor(const QColor &color);

} // namespace wiliwili

#endif
