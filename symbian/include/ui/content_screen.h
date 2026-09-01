#ifndef WILIWILI_SYMBIAN_CONTENT_SCREEN_H
#define WILIWILI_SYMBIAN_CONTENT_SCREEN_H

#include <QtCore/QPoint>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtCore/QVector>

#include "model/content_item.h"
#include "nanovg.h"

namespace wiliwili {

class ContentScreen
{
public:
    struct State {
        State() : scroll(0.0f), canLoadMore(false) {}

        QString title;
        QString subtitle;
        QString status;
        QString headerAction;
        QString secondaryHeaderAction;
        QVector<ContentItemCompat> items;
        QVector<int> itemImages;
        float scroll;
        bool canLoadMore;
    };

    enum ActionValue {
        NoActionValue = -1,
        BackActionValue = -2,
        HeaderActionValue = -3,
        RefreshActionValue = -4,
        SecondaryHeaderActionValue = -5,
        LoadMoreActionValue = -6
    };

    ContentScreen();
    void initialize(int fontId);
    void setTitle(const QString &title, const QString &subtitle = QString());
    void setStatus(const QString &status);
    void setHeaderAction(const QString &label);
    void setSecondaryHeaderAction(const QString &label);
    void setItems(const QVector<ContentItemCompat> &items);
    void appendItems(const QVector<ContentItemCompat> &items);
    void setItemImages(const QVector<int> &images);
    void setItemImage(int index, int imageHandle);
    void setCanLoadMore(bool canLoadMore);
    const QVector<ContentItemCompat> &items() const;
    State state() const;
    void restoreState(const State &state, bool restoreImages);
    void draw(NVGcontext *context, float width, float height);
    void pointerPress(const QPoint &position);
    void pointerMove(const QPoint &position);
    int pointerRelease(const QPoint &position);

private:
    void drawText(
        NVGcontext *context, const QString &text,
        float x, float y, float size,
        const NVGcolor &color, int align) const;
    void drawWrappedText(
        NVGcontext *context, const QString &text,
        float x, float y, float width, float size,
        int maxLines, const NVGcolor &color) const;
    int wrappedLineCount(
        NVGcontext *context, const QString &text,
        float width, float size) const;
    void updateItemHeights(NVGcontext *context, float width);
    void updateLayout(float width, float height);
    QRectF itemFrame(int index) const;
    float itemHeight(int index) const;

    int m_fontId;
    QString m_title;
    QString m_subtitle;
    QString m_status;
    QString m_headerAction;
    QString m_secondaryHeaderAction;
    QVector<ContentItemCompat> m_items;
    QVector<int> m_itemImages;
    QVector<float> m_itemHeights;
    float m_itemHeightWidth;
    float m_width;
    float m_height;
    float m_scroll;
    float m_minScroll;
    float m_pullDistance;
    bool m_dragging;
    bool m_canLoadMore;
    bool m_loadMoreArmed;
    QPoint m_pressPosition;
    QPoint m_lastPosition;
    QRectF m_backHitBox;
    QRectF m_headerActionHitBox;
    QRectF m_secondaryHeaderActionHitBox;
};

} // namespace wiliwili

#endif
