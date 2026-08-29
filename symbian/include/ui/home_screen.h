#ifndef WILIWILI_SYMBIAN_HOME_SCREEN_H
#define WILIWILI_SYMBIAN_HOME_SCREEN_H

#include <QtCore/QPoint>
#include <QtCore/QRectF>
#include <QtCore/QVector>

#include "model/home_card.h"
#include "platform/platform_metrics.h"
#include "nanovg.h"

namespace wiliwili {

class HomeScreen
{
public:
    enum Category {
        RecommendCategory = 0,
        PopularCategory,
        BangumiCategory,
        LiveCategory,
        CategoryCount
    };

    enum ActionValue {
        NoActionValue = -1,
        CategoryActionBase = -100,
        RefreshActionValue = -200,
        LoadMoreActionValue = -201,
        SearchActionValue = -300
    };

    HomeScreen();

    void setCards(const QVector<RecommendVideoResultCompat> &cards);
    void appendCards(const QVector<RecommendVideoResultCompat> &cards);
    void setCardImage(int index, int imageHandle);
    void setNetworkStatus(const QString &status);
    void setCanLoadMore(bool canLoadMore);
    void setCategory(Category category);
    Category category() const;
    static bool isCategoryAction(int action);
    static Category categoryFromAction(int action);
    static bool isRefreshAction(int action);
    static bool isLoadMoreAction(int action);
    static bool isSearchAction(int action);
    void initialize(int fontId, int logoHandle, int cardPlaceholderHandle);

    void draw(
        NVGcontext *context,
        float width,
        float height,
        const MemorySample &memory,
        quint64 frameCount);

    void pointerPress(const QPoint &position);
    void pointerMove(const QPoint &position);
    int pointerRelease(const QPoint &position);

private:
    HomeScreen(const HomeScreen &);
    HomeScreen &operator=(const HomeScreen &);

    void drawText(
        NVGcontext *context,
        const QString &text,
        float x,
        float y,
        float size,
        const NVGcolor &color,
        int align) const;
    void drawWrappedText(
        NVGcontext *context,
        const QString &text,
        float x,
        float y,
        float width,
        float size,
        int maxLines,
        const NVGcolor &color) const;
    void drawCard(
        NVGcontext *context,
        int index,
        const QRectF &frame) const;
    void updateLayout(float width, float height);
    QRectF categoryHitBox(int index) const;
    QString compactCount(int value) const;
    QString durationText(int seconds) const;

    QVector<RecommendVideoResultCompat> m_models;
    QVector<int> m_imageHandles;
    int m_fontId;
    int m_logoHandle;
    int m_placeholderHandle;
    int m_selectedIndex;
    Category m_category;
    float m_scroll;
    float m_minScroll;
    float m_lastWidth;
    float m_lastHeight;
    float m_cardWidth;
    float m_pullDistance;
    bool m_dragging;
    bool m_canLoadMore;
    bool m_loadMoreArmed;
    QPoint m_pressPosition;
    QPoint m_lastPosition;
    QString m_networkStatus;
};

} // namespace wiliwili

#endif
