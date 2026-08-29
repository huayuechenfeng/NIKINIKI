#include "ui/home_screen.h"

#include <QtCore/QByteArray>
#include <QtCore/QtGlobal>

#include "ui/navigation_rail.h"

namespace wiliwili {

static const float kSearchRowHeight = 56.0f;
static const float kCategoryRowHeight = 48.0f;
static const float kHeaderHeight =
    kSearchRowHeight + kCategoryRowHeight;
static const float kGridPadding = 10.0f;
static const float kCardGap = 12.0f;
static const float kCardHeight = 158.0f;
static const float kCardStep = kCardHeight + kCardGap;
static const int kColumns = 2;

HomeScreen::HomeScreen()
    : m_fontId(-1),
      m_logoHandle(-1),
      m_placeholderHandle(-1),
      m_selectedIndex(0),
      m_category(RecommendCategory),
      m_scroll(0.0f),
      m_minScroll(0.0f),
      m_lastWidth(360.0f),
      m_lastHeight(640.0f),
       m_cardWidth(0.0f),
       m_pullDistance(0.0f),
       m_dragging(false),
       m_canLoadMore(false),
       m_loadMoreArmed(false)
{
    m_networkStatus = QString::fromLatin1("BI:WAIT");
}

void HomeScreen::initialize(
    int fontId,
    int logoHandle,
    int cardPlaceholderHandle)
{
    m_fontId = fontId;
    m_logoHandle = logoHandle;
    m_placeholderHandle = cardPlaceholderHandle;
    int index;
    for (index = 0; index < m_imageHandles.size(); ++index) {
        if (m_imageHandles.at(index) < 0)
            m_imageHandles[index] = cardPlaceholderHandle;
    }
}

void HomeScreen::setCards(
    const QVector<RecommendVideoResultCompat> &cards)
{
    m_models = cards;
    m_imageHandles.clear();
    int index;
    for (index = 0; index < cards.size(); ++index)
        m_imageHandles.append(m_placeholderHandle);
    m_selectedIndex = 0;
    m_scroll = 0.0f;
    m_pullDistance = 0.0f;
    m_loadMoreArmed = false;
}

void HomeScreen::appendCards(
    const QVector<RecommendVideoResultCompat> &cards)
{
    int index;
    for (index = 0; index < cards.size(); ++index) {
        const RecommendVideoResultCompat &candidate = cards.at(index);
        bool duplicate = false;
        int existing;
        for (existing = 0; existing < m_models.size(); ++existing) {
            const RecommendVideoResultCompat &present = m_models.at(existing);
            if ((!candidate.bvid.isEmpty() && candidate.bvid == present.bvid) ||
                (candidate.bvid.isEmpty() && candidate.id != 0 &&
                 candidate.id == present.id)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            m_models.append(candidate);
            m_imageHandles.append(m_placeholderHandle);
        }
    }
    m_loadMoreArmed = false;
}

void HomeScreen::setCardImage(int index, int imageHandle)
{
    if (index >= 0 && index < m_imageHandles.size())
        m_imageHandles[index] = imageHandle;
}

void HomeScreen::setNetworkStatus(const QString &status)
{
    m_networkStatus = status;
}

void HomeScreen::setCanLoadMore(bool canLoadMore)
{
    m_canLoadMore = canLoadMore;
    if (!m_canLoadMore)
        m_loadMoreArmed = false;
}

void HomeScreen::setCategory(Category category)
{
    if (category >= RecommendCategory && category < CategoryCount) {
        m_category = category;
        m_selectedIndex = 0;
        m_scroll = 0.0f;
        m_pullDistance = 0.0f;
    }
}

HomeScreen::Category HomeScreen::category() const
{
    return m_category;
}

bool HomeScreen::isCategoryAction(int action)
{
    return action <= CategoryActionBase &&
           action > CategoryActionBase - CategoryCount;
}

HomeScreen::Category HomeScreen::categoryFromAction(int action)
{
    const int index = CategoryActionBase - action;
    if (index < 0 || index >= CategoryCount)
        return RecommendCategory;
    return static_cast<Category>(index);
}

bool HomeScreen::isRefreshAction(int action)
{
    return action == RefreshActionValue;
}

bool HomeScreen::isLoadMoreAction(int action)
{
    return action == LoadMoreActionValue;
}

bool HomeScreen::isSearchAction(int action)
{
    return action == SearchActionValue;
}

QString HomeScreen::compactCount(int value) const
{
    if (value >= 100000000)
        return QString::fromUtf8("%1亿").arg(value / 100000000.0, 0, 'f', 1);
    if (value >= 10000)
        return QString::fromUtf8("%1万").arg(value / 10000.0, 0, 'f', 1);
    return QString::number(value);
}

QString HomeScreen::durationText(int seconds) const
{
    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int remainder = seconds % 60;
    if (hours > 0) {
        return QString::fromLatin1("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(remainder, 2, 10, QLatin1Char('0'));
    }
    return QString::fromLatin1("%1:%2")
        .arg(minutes)
        .arg(remainder, 2, 10, QLatin1Char('0'));
}

void HomeScreen::drawText(
    NVGcontext *context,
    const QString &text,
    float x,
    float y,
    float size,
    const NVGcolor &color,
    int align) const
{
    if (m_fontId < 0 || text.isEmpty())
        return;
    const QByteArray utf8 = text.toUtf8();
    nvgFontFaceId(context, m_fontId);
    nvgFontSize(context, size);
    nvgTextAlign(context, align);
    nvgFillColor(context, color);
    nvgText(context, x, y, utf8.constData(), 0);
}

void HomeScreen::drawWrappedText(
    NVGcontext *context,
    const QString &text,
    float x,
    float y,
    float width,
    float size,
    int maxLines,
    const NVGcolor &color) const
{
    if (m_fontId < 0 || text.isEmpty())
        return;
    const QByteArray utf8 = text.toUtf8();
    const char *start = utf8.constData();
    const char *end = start + utf8.size();
    nvgFontFaceId(context, m_fontId);
    nvgFontSize(context, size);
    nvgTextAlign(context, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextLineHeight(context, 1.08f);
    nvgFillColor(context, color);
    int line = 0;
    while (start < end && line < maxLines) {
        NVGtextRow row;
        if (nvgTextBreakLines(context, start, end, width, &row, 1) < 1)
            break;
        nvgText(context, x, y + line * size * 1.08f,
                row.start, row.end);
        start = row.next;
        ++line;
    }
}

void HomeScreen::updateLayout(float width, float height)
{
    m_lastWidth = width;
    m_lastHeight = height;
    const float contentWidth = width - NavigationRail::width();
    m_cardWidth = (contentWidth - kGridPadding * 2.0f - kCardGap) /
                  static_cast<float>(kColumns);
    const int rows = (m_models.size() + kColumns - 1) / kColumns;
    const float viewportHeight =
        height - NavigationRail::height() - kHeaderHeight;
    const float contentHeight = rows * kCardStep + kGridPadding +
        (m_canLoadMore && !m_models.isEmpty() ? 30.0f : 0.0f);
    m_minScroll = qMin(0.0f, viewportHeight - contentHeight);
    m_scroll = qBound(m_minScroll, m_scroll, 0.0f);
}

QRectF HomeScreen::categoryHitBox(int index) const
{
    const float left = NavigationRail::width();
    const float tabWidth = (m_lastWidth - left) /
                           static_cast<float>(CategoryCount);
    return QRectF(left + index * tabWidth, kSearchRowHeight,
                  tabWidth, kCategoryRowHeight);
}

void HomeScreen::drawCard(
    NVGcontext *context,
    int index,
    const QRectF &frame) const
{
    if (index < 0 || index >= m_models.size())
        return;
    const RecommendVideoResultCompat &card = m_models.at(index);
    const float x = static_cast<float>(frame.x());
    const float y = static_cast<float>(frame.y());
    const float w = static_cast<float>(frame.width());
    const float h = static_cast<float>(frame.height());
    const float imageHeight = w * 9.0f / 16.0f;
    const bool selected = index == m_selectedIndex;

    nvgBeginPath(context);
    nvgRoundedRect(context, x, y, w, h, 9.0f);
    nvgFillColor(context, selected
        ? nvgRGBA(74, 47, 61, 255)
        : nvgRGBA(43, 43, 53, 250));
    nvgFill(context);
    if (selected) {
        nvgStrokeWidth(context, 1.2f);
        nvgStrokeColor(context, nvgRGBA(251, 114, 153, 185));
        nvgStroke(context);
    }

    nvgBeginPath(context);
    nvgRoundedRect(context, x, y, w, imageHeight, 8.0f);
    const int imageHandle = index < m_imageHandles.size()
        ? m_imageHandles.at(index) : m_placeholderHandle;
    if (imageHandle >= 0) {
        nvgFillPaint(context, nvgImagePattern(
            context, x, y, w, imageHeight, 0.0f, imageHandle, 1.0f));
    } else {
        nvgFillColor(context, nvgRGB(77, 72, 91));
    }
    nvgFill(context);

    nvgBeginPath(context);
    nvgRect(context, x, y + imageHeight - 20.0f, w, 20.0f);
    NVGpaint overlay = nvgLinearGradient(
        context, x, y + imageHeight - 20.0f,
        x, y + imageHeight,
        nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 190));
    nvgFillPaint(context, overlay);
    nvgFill(context);

    QString metrics;
    if (card.kind == LiveHomeCard) {
        metrics = QString::fromUtf8("人气 %1").arg(
            compactCount(card.stat.view));
    } else if (card.kind == BangumiHomeCard) {
        metrics = card.badge.isEmpty()
            ? QString::fromUtf8("番剧") : card.badge;
    } else {
        metrics = QString::fromLatin1("P %1  D %2")
            .arg(compactCount(card.stat.view))
            .arg(compactCount(card.stat.danmaku));
    }
    drawText(context,
             metrics,
             x + 6.0f, y + imageHeight - 5.0f, 8.6f,
             nvgRGB(255, 255, 255), NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
    if (card.kind == VideoHomeCard) {
        drawText(context, durationText(card.duration),
                 x + w - 6.0f, y + imageHeight - 5.0f, 8.6f,
                 nvgRGB(255, 255, 255), NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
    }

    drawWrappedText(context, card.title,
                    x + 7.0f, y + imageHeight + 7.0f,
                    w - 14.0f, 12.2f, 2,
                    nvgRGB(247, 247, 249));
    QString footer = card.owner.name.isEmpty()
        ? QString() : QString::fromUtf8("UP  ") + card.owner.name;
    if (card.kind == VideoHomeCard && !card.recommendationReason.isEmpty()) {
        footer = (card.is_followed ? QString::fromUtf8("已关注 · ")
                                   : QString()) +
                 card.recommendationReason;
    }
    if (card.isAdvertisement)
        footer = QString::fromUtf8("广告 · ") + footer;
    if (footer.size() > 24)
        footer = footer.left(23) + QString::fromUtf8("…");
    drawText(context,
             footer,
             x + 7.0f, y + h - 6.0f, 9.1f,
             nvgRGB(164, 164, 178), NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
}

void HomeScreen::draw(
    NVGcontext *context,
    float width,
    float height,
    const MemorySample &memory,
    quint64 frameCount)
{
    Q_UNUSED(memory);
    Q_UNUSED(frameCount);
    updateLayout(width, height);
    const float left = NavigationRail::width();

    NVGpaint background = nvgLinearGradient(
        context, left, 0.0f, width, height,
        nvgRGBA(31, 31, 40, 255), nvgRGBA(20, 21, 29, 255));
    nvgBeginPath(context);
    nvgRect(context, left, 0.0f, width - left, height);
    nvgFillPaint(context, background);
    nvgFill(context);

    nvgBeginPath(context);
    nvgRect(context, left, 0.0f, width - left, kHeaderHeight);
    nvgFillColor(context, nvgRGBA(31, 31, 40, 249));
    nvgFill(context);
    // Search occupies its own full-width row. Keeping it separate from the
    // category tabs gives the native Symbian editor enough touch area and
    // prevents the old 68x18 button from being mistaken for a decoration.
    nvgBeginPath(context);
    nvgRoundedRect(context, left + 10.0f, 8.0f,
                   width - left - 20.0f, 40.0f, 10.0f);
    nvgFillColor(context, nvgRGBA(255, 255, 255, 14));
    nvgFill(context);
    nvgStrokeWidth(context, 1.2f);
    nvgStrokeColor(context, nvgRGBA(180, 180, 192, 150));
    nvgStroke(context);
    nvgBeginPath(context);
    nvgCircle(context, left + 26.0f, 27.0f, 6.0f);
    nvgMoveTo(context, left + 30.0f, 31.0f);
    nvgLineTo(context, left + 35.0f, 36.0f);
    nvgStroke(context);
    drawText(context,
             QString::fromUtf8("搜索视频或用户"),
             left + 44.0f, 28.0f, 11.2f,
             nvgRGB(188, 188, 200),
             NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    drawText(context, m_networkStatus,
             width - 20.0f, 28.0f, 8.8f,
             nvgRGB(148, 148, 162),
             NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);

    static const char *tabLabels[] = {
        "推荐", "热门", "番剧", "直播"
    };
    int tab;
    for (tab = 0; tab < CategoryCount; ++tab) {
        const QRectF box = categoryHitBox(tab);
        const bool selected = tab == static_cast<int>(m_category);
        if (selected) {
            nvgBeginPath(context);
            nvgRoundedRect(context,
                           static_cast<float>(box.x()) + 4.0f,
                           kSearchRowHeight + 6.0f,
                           static_cast<float>(box.width()) - 8.0f,
                           36.0f,
                           9.0f);
            nvgFillColor(context, nvgRGBA(251, 114, 153, 20));
            nvgFill(context);
        }
        drawText(context, QString::fromUtf8(tabLabels[tab]),
                 static_cast<float>(box.center().x()),
                 kSearchRowHeight + 24.0f, 12.4f,
                 selected ? nvgRGB(251, 114, 153)
                          : nvgRGB(206, 206, 214),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    nvgBeginPath(context);
    const QRectF activeTab = categoryHitBox(static_cast<int>(m_category));
    nvgRoundedRect(context,
                   static_cast<float>(activeTab.center().x()) - 14.0f,
                   kHeaderHeight - 3.0f, 28.0f, 3.0f, 1.5f);
    nvgFillColor(context, nvgRGB(251, 114, 153));
    nvgFill(context);

    nvgSave(context);
    nvgScissor(context, left, kHeaderHeight,
               width - left,
               height - NavigationRail::height() - kHeaderHeight);
    if (m_pullDistance > 2.0f) {
        drawText(context,
                 m_pullDistance >= 52.0f
                    ? QString::fromUtf8("松开刷新")
                    : QString::fromUtf8("下拉刷新"),
                 left + (width - left) * 0.5f,
                 kHeaderHeight + m_pullDistance * 0.5f,
                 10.5f,
                 nvgRGB(251, 114, 153),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    int index;
    for (index = 0; index < m_models.size(); ++index) {
        const int row = index / kColumns;
        const int column = index % kColumns;
        const float x = left + kGridPadding +
                        column * (m_cardWidth + kCardGap);
        const float y = kHeaderHeight + kGridPadding + m_scroll +
                        m_pullDistance +
                        row * kCardStep;
        drawCard(context, index, QRectF(x, y, m_cardWidth, kCardHeight));
    }
    if (m_canLoadMore && !m_models.isEmpty()) {
        const int rows = (m_models.size() + kColumns - 1) / kColumns;
        const float hintY = kHeaderHeight + kGridPadding + m_scroll +
            rows * kCardStep + 8.0f;
        drawText(context,
                 QString::fromUtf8("继续上滑加载更多推荐"),
                 left + (width - left) * 0.5f, hintY, 10.0f,
                 nvgRGB(166, 166, 180),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    nvgRestore(context);
}

void HomeScreen::pointerPress(const QPoint &position)
{
    m_dragging = true;
    m_pressPosition = position;
    m_lastPosition = position;
}

void HomeScreen::pointerMove(const QPoint &position)
{
    if (!m_dragging)
        return;
    const float delta = static_cast<float>(
        position.y() - m_lastPosition.y());
    if (m_scroll >= 0.0f && (m_pullDistance > 0.0f || delta > 0.0f)) {
        m_pullDistance = qBound(0.0f, m_pullDistance + delta, 76.0f);
    } else {
        m_scroll += delta;
        m_scroll = qBound(m_minScroll, m_scroll, 0.0f);
        if (m_canLoadMore && delta < -1.0f &&
            m_scroll <= m_minScroll + 0.5f) {
            m_loadMoreArmed = true;
        } else if (m_scroll > m_minScroll + 8.0f) {
            m_loadMoreArmed = false;
        }
    }
    m_lastPosition = position;
}

int HomeScreen::pointerRelease(const QPoint &position)
{
    if (!m_dragging)
        return -1;
    m_dragging = false;
    if (m_pullDistance >= 52.0f) {
        m_pullDistance = 0.0f;
        m_scroll = 0.0f;
        return RefreshActionValue;
    }
    if (m_pullDistance > 0.0f) {
        m_pullDistance = 0.0f;
        return -1;
    }
    if (m_canLoadMore && m_loadMoreArmed &&
        (position - m_pressPosition).manhattanLength() >= 18) {
        m_loadMoreArmed = false;
        return LoadMoreActionValue;
    }
    if ((position - m_pressPosition).manhattanLength() >= 18 ||
        position.x() < static_cast<int>(NavigationRail::width()) ||
        position.y() >= static_cast<int>(
            m_lastHeight - NavigationRail::height())) {
        return -1;
    }

    if (position.y() < static_cast<int>(kSearchRowHeight)) {
        if (position.x() >= static_cast<int>(
                NavigationRail::width() + 8.0f) &&
            position.x() <= static_cast<int>(m_lastWidth - 8.0f)) {
            return SearchActionValue;
        }
        return -1;
    }
    if (position.y() < static_cast<int>(kHeaderHeight)) {
        int tab;
        for (tab = 0; tab < CategoryCount; ++tab) {
            if (categoryHitBox(tab).contains(QPointF(position))) {
                setCategory(static_cast<Category>(tab));
                return CategoryActionBase - tab;
            }
        }
        return -1;
    }
    const float gridX = position.x() - NavigationRail::width() - kGridPadding;
    const float gridY = position.y() - kHeaderHeight - kGridPadding - m_scroll;
    if (gridX < 0.0f || gridY < 0.0f)
        return -1;
    const int column = static_cast<int>(gridX / (m_cardWidth + kCardGap));
    const int row = static_cast<int>(gridY / kCardStep);
    const float withinX = gridX - column * (m_cardWidth + kCardGap);
    const float withinY = gridY - row * kCardStep;
    const int index = row * kColumns + column;
    if (column < 0 || column >= kColumns ||
        withinX < 0.0f || withinX > m_cardWidth ||
        withinY < 0.0f || withinY > kCardHeight ||
        index < 0 || index >= m_models.size()) {
        return -1;
    }
    m_selectedIndex = index;
    return index;
}

} // namespace wiliwili
