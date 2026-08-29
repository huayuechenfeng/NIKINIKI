#include "model/home_card.h"

namespace wiliwili {

static RecommendVideoResultCompat makeCard(
    quint64 id,
    const char *bvid,
    const char *title,
    const char *owner,
    int duration,
    int view,
    int danmaku)
{
    RecommendVideoResultCompat card;
    card.id = id;
    card.bvid = QString::fromLatin1(bvid);
    card.title = QString::fromUtf8(title);
    card.owner.name = QString::fromUtf8(owner);
    card.duration = duration;
    card.stat.view = view;
    card.stat.danmaku = danmaku;
    return card;
}

QVector<RecommendVideoResultCompat> buildHomeFixture()
{
    QVector<RecommendVideoResultCompat> cards;
    cards.reserve(10);

    // Field names and page semantics mirror upstream RecommendVideoResult.
    // Values are deterministic and intentionally contain no user data.
    cards.append(makeCard(101, "BV1SYMBIAN01", "wiliwili arrives on Symbian Belle", "wiliwili port", 245, 60300, 808));
    cards.append(makeCard(102, "BV1SYMBIAN02", "NanoVG and OpenGL ES 2.0 on Nokia 603", "render lab", 318, 42150, 612));
    cards.append(makeCard(103, "BV1SYMBIAN03", "Reusing the upstream Bilibili API model", "core team", 472, 38720, 455));
    cards.append(makeCard(104, "BV1SYMBIAN04", "A tiny retained UI for a classic phone", "borealis notes", 196, 28104, 371));
    cards.append(makeCard(105, "BV1SYMBIAN05", "QtNetwork transport comes in the next gate", "network lab", 529, 24011, 266));
    cards.append(makeCard(106, "BV1SYMBIAN06", "Testing touch scrolling and bounded memory", "device bench", 361, 19736, 198));
    cards.append(makeCard(107, "BV1SYMBIAN07", "Preparing image cache and cancellation", "image helper", 283, 16421, 173));
    cards.append(makeCard(108, "BV1SYMBIAN08", "Hardware video playback with MMF", "player lab", 614, 13788, 121));
    cards.append(makeCard(109, "BV1SYMBIAN09", "Presenter behavior without modern C++", "compat layer", 339, 11092, 94));
    cards.append(makeCard(110, "BV1SYMBIAN10", "Next stop: a live recommendation response", "M1 fixture", 225, 9081, 73));

    return cards;
}

} // namespace wiliwili
