#ifndef WILIWILI_SYMBIAN_HOME_CARD_H
#define WILIWILI_SYMBIAN_HOME_CARD_H

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QtGlobal>

namespace wiliwili {

enum HomeCardKindCompat
{
    VideoHomeCard = 0,
    BangumiHomeCard,
    LiveHomeCard
};

// C++03-compatible subset of upstream RecommendVideoResult.
struct UserSimpleResultCompat
{
    quint64 mid;
    QString name;

    UserSimpleResultCompat() : mid(0) {}
};

struct VideoSimpleStateResultCompat
{
    int view;
    int danmaku;

    VideoSimpleStateResultCompat() : view(0), danmaku(0) {}
};

struct RecommendVideoResultCompat
{
    quint64 id;
    QString bvid;
    QString pic;
    QString title;
    int duration;
    int pubdate;
    UserSimpleResultCompat owner;
    VideoSimpleStateResultCompat stat;
    int is_followed;
    HomeCardKindCompat kind;
    QString badge;
    QString recommendationReason;
    bool isAdvertisement;

    RecommendVideoResultCompat()
        : id(0), duration(0), pubdate(0), is_followed(0),
          kind(VideoHomeCard), isAdvertisement(false)
    {
    }
};

QVector<RecommendVideoResultCompat> buildHomeFixture();

} // namespace wiliwili

#endif
