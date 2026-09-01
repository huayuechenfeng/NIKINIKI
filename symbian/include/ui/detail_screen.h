#ifndef WILIWILI_SYMBIAN_DETAIL_SCREEN_H
#define WILIWILI_SYMBIAN_DETAIL_SCREEN_H

#include <QtCore/QPoint>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtCore/QVector>

#include "model/video_detail.h"
#include "ui/view_node.h"

namespace wiliwili {

class DetailScreen
{
public:
    struct State {
        State() : pageIndex(0), imageHandle(-1), scroll(0.0f) {}

        VideoDetailCompat video;
        int pageIndex;
        int imageHandle;
        QVector<RecommendVideoResultCompat> recommendations;
        QVector<int> recommendationImages;
        float scroll;
        QString networkStatus;
    };

    enum Action {
        NoAction,
        BackAction,
        PlayAction,
        OwnerAction,
        CommentsAction,
        LikeAction,
        CoinAction,
        FavoriteAction,
        WatchLaterAction,
        PageAction,
        RecommendationActionBase = 100
    };

    DetailScreen();
    ~DetailScreen();

    void initialize(int fontId, int placeholderImageHandle);
    void setVideo(const VideoDetailCompat &video);
    void setImageHandle(int imageHandle);
    void setRecommendations(
        const QVector<RecommendVideoResultCompat> &recommendations);
    void setRecommendationImage(int index, int imageHandle);
    const QVector<RecommendVideoResultCompat> &recommendations() const;
    static bool isRecommendationAction(Action action);
    static int recommendationIndex(Action action);
    void setNetworkStatus(const QString &status);
    const VideoDetailCompat &video() const;
    int selectedPageIndex() const;
    State state() const;
    void restoreState(const State &state);
    void cyclePage();

    void draw(NVGcontext *context, float width, float height);

    void pointerPress(const QPoint &position);
    void pointerMove(const QPoint &position);
    Action pointerRelease(const QPoint &position);

private:
    DetailScreen(const DetailScreen &);
    DetailScreen &operator=(const DetailScreen &);

    void clearNodes();
    void buildNodes();
    void updateLabels();
    void layout(float width, float height);
    void drawBackground(NVGcontext *context, float width, float height);
    void drawActionButton(
        NVGcontext *context, const QRectF &frame,
        const QString &label, bool accent) const;
    void drawRecommendation(
        NVGcontext *context, int index, const QRectF &frame) const;

    VideoDetailCompat m_video;
    int m_pageIndex;
    int m_fontId;
    int m_imageHandle;
    QVector<RecommendVideoResultCompat> m_recommendations;
    QVector<int> m_recommendationImages;
    QVector<QRectF> m_recommendationHitBoxes;
    float m_scroll;
    float m_minScroll;
    QString m_networkStatus;

    BoxNode *m_header;
    LabelNode *m_backLabel;
    LabelNode *m_headerLabel;
    ImageNode *m_cover;
    BoxNode *m_infoPanel;
    LabelNode *m_title;
    LabelNode *m_owner;
    LabelNode *m_stats;
    LabelNode *m_pageInfo;
    BoxNode *m_playButton;
    LabelNode *m_playLabel;
    BoxNode *m_descriptionPanel;
    LabelNode *m_descriptionLabel;
    BoxNode *m_footer;
    LabelNode *m_footerLabel;

    QRectF m_backHitBox;
    QRectF m_playHitBox;
    QRectF m_ownerHitBox;
    QRectF m_commentHitBox;
    QRectF m_likeHitBox;
    QRectF m_coinHitBox;
    QRectF m_favoriteHitBox;
    QRectF m_watchLaterHitBox;
    QRectF m_pageInfoHitBox;
    bool m_pressed;
    QPoint m_pressPosition;
    QPoint m_lastPosition;
};

} // namespace wiliwili

#endif
