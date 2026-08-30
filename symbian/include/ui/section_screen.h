#ifndef WILIWILI_SYMBIAN_SECTION_SCREEN_H
#define WILIWILI_SYMBIAN_SECTION_SCREEN_H

#include <QtCore/QString>
#include <QtCore/QPoint>
#include <QtCore/QRectF>
#include <QtCore/QVector>

#include "model/content_item.h"
#include "nanovg.h"
#include "ui/navigation_rail.h"

namespace wiliwili {

class SectionScreen
{
public:
    enum Action {
        NoAction,
        ExitApplicationAction,
        ReplyTabAction,
        AtTabAction,
        LikeTabAction,
        ChatTabAction,
        NetworkTestAction,
        ToggleImagesAction,
        CyclePlaybackModeAction,
        CycleDecoderModeAction,
        ClearCacheAction,
        AboutAction,
        AboutBackAction,
        RefreshAction,
        LoadMoreAction,
        ItemActionBase = 100
    };

    SectionScreen();

    void initialize(int fontId);
    void setSection(NavigationRail::Section section);
    void setLoggedIn(bool loggedIn);
    void setStatus(const QString &status);
    void setMessageTab(int tab);
    void setImageLoadingEnabled(bool enabled);
    void setPlaybackPreferences(int playbackMode, int decoderMode);
    void setAboutVisible(bool visible);
    bool aboutVisible() const;
    void setItems(const QVector<ContentItemCompat> &items);
    void appendItems(const QVector<ContentItemCompat> &items);
    QString itemId(int index) const;
    QString itemTitle(int index) const;
    QString itemSubtitle(int index) const;
    QString itemDescription(int index) const;
    void setCanLoadMore(bool canLoadMore);
    static bool isItemAction(Action action);
    static int itemIndex(Action action);
    void clearItems();
    void draw(NVGcontext *context, float width, float height);
    void pointerPress(const QPoint &position);
    void pointerMove(const QPoint &position);
    Action pointerRelease(const QPoint &position);

private:
    void drawText(
        NVGcontext *context,
        const QString &text,
        float x,
        float y,
        float size,
        const NVGcolor &color,
        int align) const;
    void drawInfoCard(
        NVGcontext *context,
        float x,
        float y,
        float width,
        const QString &title,
        const QString &subtitle) const;
    void drawWrappedText(
        NVGcontext *context,
        const QString &text,
        float x,
        float y,
        float width,
        float size,
        int maxLines,
        const NVGcolor &color) const;
    void drawAvatar(
        NVGcontext *context,
        const QString &name,
        float centerX,
        float centerY,
        float size,
        bool accented) const;
    void drawAboutPage(
        NVGcontext *context,
        float left,
        float width);
    void drawDynamicCard(
        NVGcontext *context,
        const ContentItemCompat &item,
        const QRectF &frame) const;
    void drawMessageRow(
        NVGcontext *context,
        const ContentItemCompat &item,
        const QRectF &frame) const;
    float itemHeight(int index) const;
    QRectF itemFrame(int index) const;

    int m_fontId;
    NavigationRail::Section m_section;
    bool m_loggedIn;
    QString m_status;
    bool m_pressed;
    bool m_dragging;
    bool m_canLoadMore;
    bool m_loadMoreArmed;
    QPoint m_pressPosition;
    QPoint m_lastPosition;
    float m_scroll;
    float m_minScroll;
    float m_pullDistance;
    float m_lastWidth;
    float m_lastHeight;
    float m_itemTop;
    QRectF m_exitHitBox;
    QRectF m_replyTabHitBox;
    QRectF m_atTabHitBox;
    QRectF m_likeTabHitBox;
    QRectF m_chatTabHitBox;
    QRectF m_networkHitBox;
    QRectF m_imagesHitBox;
    QRectF m_playbackModeHitBox;
    QRectF m_decoderModeHitBox;
    QRectF m_cacheHitBox;
    QRectF m_aboutHitBox;
    QRectF m_aboutBackHitBox;
    int m_messageTab;
    bool m_imageLoadingEnabled;
    int m_playbackMode;
    int m_decoderMode;
    bool m_aboutVisible;
    QVector<ContentItemCompat> m_items;
};

} // namespace wiliwili

#endif
