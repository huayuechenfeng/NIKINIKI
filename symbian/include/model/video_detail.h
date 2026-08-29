#ifndef WILIWILI_SYMBIAN_VIDEO_DETAIL_H
#define WILIWILI_SYMBIAN_VIDEO_DETAIL_H

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QtGlobal>

#include "model/home_card.h"

namespace wiliwili {

struct VideoDetailPageCompat
{
    quint64 cid;
    int page;
    int duration;
    int width;
    int height;
    QString part;

    VideoDetailPageCompat()
        : cid(0), page(0), duration(0), width(0), height(0) {}
};

struct VideoDetailStatCompat
{
    int view;
    int danmaku;
    int favorite;
    int coin;
    int share;
    int like;
    int reply;

    VideoDetailStatCompat()
        : view(0), danmaku(0), favorite(0), coin(0), share(0),
          like(0), reply(0)
    {
    }
};

struct VideoDetailCompat
{
    QString bvid;
    quint64 aid;
    QString pic;
    QString title;
    QString description;
    int pubdate;
    int duration;
    UserSimpleResultCompat owner;
    VideoDetailStatCompat stat;
    QVector<VideoDetailPageCompat> pages;

    VideoDetailCompat() : aid(0), pubdate(0), duration(0) {}
};

VideoDetailCompat videoDetailFromCard(
    const RecommendVideoResultCompat &card);

} // namespace wiliwili

#endif
