#include "model/video_detail.h"

namespace wiliwili {

VideoDetailCompat videoDetailFromCard(
    const RecommendVideoResultCompat &card)
{
    VideoDetailCompat detail;
    detail.bvid = card.bvid;
    detail.aid = card.id;
    detail.pic = card.pic;
    detail.title = card.title;
    detail.pubdate = card.pubdate;
    detail.duration = card.duration;
    detail.owner = card.owner;
    detail.stat.view = card.stat.view;
    detail.stat.danmaku = card.stat.danmaku;
    return detail;
}

} // namespace wiliwili
