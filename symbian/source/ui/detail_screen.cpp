#include "ui/detail_screen.h"

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QtGlobal>

namespace wiliwili {

static QString detailCount(int value)
{
    if (value >= 1000000)
        return QString::fromLatin1("%1M").arg(value / 1000000.0, 0, 'f', 1);
    if (value >= 1000)
        return QString::fromLatin1("%1K").arg(value / 1000.0, 0, 'f', 1);
    return QString::number(value);
}

static QString detailDuration(int seconds)
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

DetailScreen::DetailScreen()
    : m_pageIndex(0),
      m_fontId(-1),
      m_imageHandle(-1),
      m_scroll(0.0f),
      m_minScroll(0.0f),
      m_header(0),
      m_backLabel(0),
      m_headerLabel(0),
      m_cover(0),
      m_infoPanel(0),
      m_title(0),
      m_owner(0),
      m_stats(0),
      m_pageInfo(0),
      m_playButton(0),
      m_playLabel(0),
      m_descriptionPanel(0),
      m_descriptionLabel(0),
      m_footer(0),
      m_footerLabel(0),
      m_pressed(false)
{
}

DetailScreen::~DetailScreen()
{
    clearNodes();
}

void DetailScreen::initialize(int fontId, int placeholderImageHandle)
{
    m_fontId = fontId;
    m_imageHandle = placeholderImageHandle;
    buildNodes();
}

void DetailScreen::setVideo(const VideoDetailCompat &video)
{
    m_video = video;
    m_pageIndex = 0;
    m_scroll = 0.0f;
    updateLabels();
}

void DetailScreen::setImageHandle(int imageHandle)
{
    m_imageHandle = imageHandle;
    if (m_cover)
        m_cover->setImageHandle(imageHandle);
}

void DetailScreen::setRecommendations(
    const QVector<RecommendVideoResultCompat> &recommendations)
{
    m_recommendations = recommendations;
    m_recommendationImages.clear();
    int index;
    for (index = 0; index < m_recommendations.size(); ++index)
        m_recommendationImages.append(-1);
    m_scroll = 0.0f;
}

void DetailScreen::setRecommendationImage(int index, int imageHandle)
{
    if (index < 0 || index >= m_recommendationImages.size())
        return;
    m_recommendationImages[index] = imageHandle;
}

const QVector<RecommendVideoResultCompat> &
DetailScreen::recommendations() const
{
    return m_recommendations;
}

bool DetailScreen::isRecommendationAction(Action action)
{
    return static_cast<int>(action) >= RecommendationActionBase;
}

int DetailScreen::recommendationIndex(Action action)
{
    return static_cast<int>(action) - RecommendationActionBase;
}

void DetailScreen::setNetworkStatus(const QString &status)
{
    m_networkStatus = status;
    updateLabels();
}

const VideoDetailCompat &DetailScreen::video() const
{
    return m_video;
}

int DetailScreen::selectedPageIndex() const
{
    return m_pageIndex;
}

DetailScreen::State DetailScreen::state() const
{
    State result;
    result.video = m_video;
    result.pageIndex = m_pageIndex;
    result.imageHandle = m_imageHandle;
    result.recommendations = m_recommendations;
    result.recommendationImages = m_recommendationImages;
    result.scroll = m_scroll;
    result.networkStatus = m_networkStatus;
    return result;
}

void DetailScreen::restoreState(const State &state)
{
    m_video = state.video;
    m_pageIndex = qBound(0, state.pageIndex,
                         qMax(1, m_video.pages.size()) - 1);
    m_networkStatus = state.networkStatus;
    m_recommendations = state.recommendations;
    m_recommendationImages = state.recommendationImages;
    m_scroll = state.scroll;
    setImageHandle(state.imageHandle);
    updateLabels();
}

void DetailScreen::cyclePage()
{
    if (m_video.pages.size() <= 1)
        return;
    m_pageIndex = (m_pageIndex + 1) % m_video.pages.size();
    updateLabels();
}

void DetailScreen::clearNodes()
{
    delete m_header;
    delete m_cover;
    delete m_infoPanel;
    delete m_playButton;
    delete m_descriptionPanel;
    delete m_footer;

    m_header = 0;
    m_backLabel = 0;
    m_headerLabel = 0;
    m_cover = 0;
    m_infoPanel = 0;
    m_title = 0;
    m_owner = 0;
    m_stats = 0;
    m_pageInfo = 0;
    m_playButton = 0;
    m_playLabel = 0;
    m_descriptionPanel = 0;
    m_descriptionLabel = 0;
    m_footer = 0;
    m_footerLabel = 0;
}

void DetailScreen::buildNodes()
{
    clearNodes();

    m_header = new BoxNode();
    m_header->setColor(QColor(31, 31, 40, 252));
    m_backLabel = new LabelNode();
    m_backLabel->setFont(m_fontId, 13.0f);
    m_backLabel->setColor(QColor(251, 114, 153));
    m_backLabel->setText(QString::fromUtf8("<  返回"));
    m_backLabel->setAlignment(NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    m_headerLabel = new LabelNode();
    m_headerLabel->setFont(m_fontId, 12.0f);
    m_headerLabel->setColor(QColor(205, 194, 215));
    m_headerLabel->setText(QString::fromUtf8("视频详情"));
    m_headerLabel->setAlignment(NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    m_header->addChild(m_backLabel);
    m_header->addChild(m_headerLabel);

    m_cover = new ImageNode();
    m_cover->setImageHandle(m_imageHandle);
    m_cover->setCornerRadius(10.0f);

    m_infoPanel = new BoxNode();
    m_infoPanel->setColor(QColor(43, 43, 53, 246));
    m_infoPanel->setCornerRadius(10.0f);
    m_title = new LabelNode();
    m_title->setFont(m_fontId, 14.0f);
    m_title->setColor(QColor(255, 255, 255));
    m_title->setWrap(true);
    m_title->setMaxLines(2);
    m_title->setLineHeight(1.1f);
    m_owner = new LabelNode();
    m_owner->setFont(m_fontId, 10.5f);
    m_owner->setColor(QColor(190, 177, 201));
    m_stats = new LabelNode();
    m_stats->setFont(m_fontId, 9.5f);
    m_stats->setColor(QColor(222, 211, 230));
    m_pageInfo = new LabelNode();
    m_pageInfo->setFont(m_fontId, 9.5f);
    m_pageInfo->setColor(QColor(251, 145, 173));
    m_infoPanel->addChild(m_title);
    m_infoPanel->addChild(m_owner);
    m_infoPanel->addChild(m_stats);
    m_infoPanel->addChild(m_pageInfo);

    m_playButton = new BoxNode();
    m_playButton->setColor(QColor(251, 114, 153));
    m_playButton->setCornerRadius(6.0f);
    m_playLabel = new LabelNode();
    m_playLabel->setFont(m_fontId, 11.0f);
    m_playLabel->setColor(QColor(255, 255, 255));
    m_playLabel->setText(QString::fromUtf8("播放"));
    m_playLabel->setAlignment(NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    m_playButton->addChild(m_playLabel);

    m_descriptionPanel = new BoxNode();
    m_descriptionPanel->setColor(QColor(39, 39, 49, 242));
    m_descriptionPanel->setCornerRadius(10.0f);
    m_descriptionLabel = new LabelNode();
    m_descriptionLabel->setFont(m_fontId, 10.5f);
    m_descriptionLabel->setColor(QColor(220, 211, 226));
    m_descriptionLabel->setWrap(true);
    m_descriptionLabel->setMaxLines(8);
    m_descriptionLabel->setLineHeight(1.18f);
    m_descriptionPanel->addChild(m_descriptionLabel);

    m_footer = new BoxNode();
    m_footer->setColor(QColor(24, 24, 31, 250));
    m_footerLabel = new LabelNode();
    m_footerLabel->setFont(m_fontId, 10.0f);
    m_footerLabel->setColor(QColor(205, 191, 222));
    m_footerLabel->setAlignment(NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    m_footer->addChild(m_footerLabel);

    updateLabels();
}

void DetailScreen::updateLabels()
{
    if (!m_title)
        return;

    m_title->setText(m_video.title);
    m_owner->setText(QString::fromUtf8("UP ") + m_video.owner.name);
    m_stats->setText(
        QString::fromUtf8("播放 %1    弹幕 %2    点赞 %3")
            .arg(detailCount(m_video.stat.view))
            .arg(detailCount(m_video.stat.danmaku))
            .arg(detailCount(m_video.stat.like)));

    const int pageCount = qMax(1, m_video.pages.size());
    const int safePage = qBound(0, m_pageIndex, pageCount - 1);
    const int duration = !m_video.pages.isEmpty()
        ? m_video.pages.at(safePage).duration : m_video.duration;
    QString pageText = QString::fromUtf8("P%1/%2  %3  点击切换")
        .arg(safePage + 1)
        .arg(pageCount)
        .arg(detailDuration(duration));
    if (!m_video.pages.isEmpty() &&
        !m_video.pages.at(safePage).part.isEmpty()) {
        pageText += QString::fromUtf8("  ") +
            m_video.pages.at(safePage).part.left(28);
    }
    m_pageInfo->setText(pageText);

    const QString description = m_video.description.trimmed().isEmpty()
        ? QString::fromUtf8("正在加载视频简介...")
        : m_video.description.trimmed();
    m_descriptionLabel->setText(
        QString::fromUtf8("简介\n") + description);

    m_footerLabel->setText(
        QString::fromLatin1("%1  |  %2  |  FAV %3  COIN %4")
            .arg(m_networkStatus)
            .arg(m_video.bvid)
            .arg(detailCount(m_video.stat.favorite))
            .arg(detailCount(m_video.stat.coin)));
}

void DetailScreen::layout(float width, float height)
{
    const float headerHeight = 50.0f;
    const float footerHeight = 22.0f;
    const float margin = 10.0f;
    const float gap = 8.0f;
    const float coverWidth = width - margin * 2.0f;
    const float coverHeight = coverWidth * 9.0f / 16.0f;
    const float contentTop = headerHeight + margin;
    const float infoX = margin;
    const float infoWidth = width - margin * 2.0f;
    const float infoTop = contentTop + coverHeight + gap;
    const float infoHeight = 146.0f;
    const float descriptionTop = infoTop + infoHeight + gap;
    const float descriptionHeight = 104.0f;
    const float recommendationTitleTop =
        descriptionTop + descriptionHeight + 12.0f;
    const float recommendationRowHeight = 82.0f;
    const float contentBottom = recommendationTitleTop + 28.0f +
        m_recommendations.size() * recommendationRowHeight + margin;
    const float viewportBottom = height - footerHeight;
    m_minScroll = qMin(0.0f, viewportBottom - contentBottom);
    m_scroll = qBound(m_minScroll, m_scroll, 0.0f);
    const float bodyOffset = m_scroll;

    m_header->setFrame(QRectF(0.0, 0.0, width, headerHeight));
    m_backLabel->setFrame(QRectF(12.0, headerHeight * 0.5f, 0.0, 0.0));
    m_headerLabel->setFrame(QRectF(width - 12.0f, headerHeight * 0.5f, 0.0, 0.0));
    m_backHitBox = QRectF(0.0, 0.0, 105.0, headerHeight);

    m_cover->setFrame(QRectF(
        margin, contentTop + bodyOffset, coverWidth, coverHeight));
    m_infoPanel->setFrame(QRectF(
        infoX, infoTop + bodyOffset, infoWidth, infoHeight));
    m_title->setFrame(QRectF(infoX + 10.0f, infoTop + bodyOffset + 10.0f, infoWidth - 20.0f, 38.0f));
    m_owner->setFrame(QRectF(infoX + 10.0f, infoTop + bodyOffset + 55.0f, infoWidth - 20.0f, 12.0f));
    m_ownerHitBox = QRectF(infoX + 6.0f, infoTop + bodyOffset + 47.0f,
                           infoWidth - 12.0f, 26.0f);
    m_stats->setFrame(QRectF(infoX + 10.0f, infoTop + bodyOffset + 76.0f, infoWidth - 20.0f, 11.0f));
    m_pageInfo->setFrame(QRectF(infoX + 10.0f, infoTop + bodyOffset + 95.0f, infoWidth - 20.0f, 11.0f));
    m_pageInfoHitBox = QRectF(
        infoX + 6.0f, infoTop + bodyOffset + 88.0f,
        infoWidth - 12.0f, 23.0f);

    const float buttonGap = 4.0f;
    const float buttonWidth =
        (infoWidth - 20.0f - buttonGap * 5.0f) / 6.0f;
    const float buttonY = infoTop + bodyOffset + infoHeight - 36.0f;
    m_playButton->setFrame(QRectF(infoX + 10.0f, buttonY, buttonWidth, 27.0f));
    m_playLabel->setFrame(QRectF(
        infoX + 10.0f + buttonWidth * 0.5f,
        buttonY + 13.5f,
        0.0,
        0.0));
    m_playHitBox = m_playButton->frame();
    m_commentHitBox = QRectF(
        infoX + 10.0f + (buttonWidth + buttonGap), buttonY,
        buttonWidth, 27.0f);
    m_likeHitBox = QRectF(
        infoX + 10.0f + (buttonWidth + buttonGap) * 2.0f, buttonY,
        buttonWidth, 27.0f);
    m_coinHitBox = QRectF(
        infoX + 10.0f + (buttonWidth + buttonGap) * 3.0f, buttonY,
        buttonWidth, 27.0f);
    m_favoriteHitBox = QRectF(
        infoX + 10.0f + (buttonWidth + buttonGap) * 4.0f, buttonY,
        buttonWidth, 27.0f);
    m_watchLaterHitBox = QRectF(
        infoX + 10.0f + (buttonWidth + buttonGap) * 5.0f, buttonY,
        buttonWidth, 27.0f);

    m_descriptionPanel->setFrame(QRectF(
        margin,
        descriptionTop + bodyOffset,
        width - margin * 2.0f,
        descriptionHeight));
    m_descriptionLabel->setFrame(QRectF(
        margin + 8.0f,
        descriptionTop + bodyOffset + 7.0f,
        width - margin * 2.0f - 16.0f,
        descriptionHeight - 12.0f));

    m_recommendationHitBoxes.clear();
    int recommendationIndex;
    for (recommendationIndex = 0;
         recommendationIndex < m_recommendations.size();
         ++recommendationIndex) {
        m_recommendationHitBoxes.append(QRectF(
            margin,
            recommendationTitleTop + bodyOffset + 25.0f +
                recommendationIndex * recommendationRowHeight,
            width - margin * 2.0f,
            recommendationRowHeight - 7.0f));
    }

    m_footer->setFrame(QRectF(0.0, height - footerHeight, width, footerHeight));
    m_footerLabel->setFrame(QRectF(
        width * 0.5f,
        height - footerHeight * 0.5f,
        0.0,
        0.0));
}

void DetailScreen::drawRecommendation(
    NVGcontext *context, int index, const QRectF &frame) const
{
    if (index < 0 || index >= m_recommendations.size())
        return;
    const RecommendVideoResultCompat &video = m_recommendations.at(index);
    const float x = static_cast<float>(frame.x());
    const float y = static_cast<float>(frame.y());
    const float w = static_cast<float>(frame.width());
    const float h = static_cast<float>(frame.height());
    nvgBeginPath(context);
    nvgRoundedRect(context, x, y, w, h, 9.0f);
    nvgFillColor(context, nvgRGBA(43, 43, 53, 244));
    nvgFill(context);
    const float imageWidth = 112.0f;
    const float imageHeight = 63.0f;
    const int imageHandle = index < m_recommendationImages.size()
        ? m_recommendationImages.at(index) : -1;
    nvgBeginPath(context);
    nvgRoundedRect(context, x + 7.0f, y + 6.0f,
                   imageWidth, imageHeight, 7.0f);
    if (imageHandle > 0) {
        nvgFillPaint(context, nvgImagePattern(
            context, x + 7.0f, y + 6.0f,
            imageWidth, imageHeight, 0.0f, imageHandle, 1.0f));
    } else {
        nvgFillColor(context, nvgRGB(69, 67, 81));
    }
    nvgFill(context);
    if (m_fontId < 0)
        return;
    nvgFontFaceId(context, m_fontId);
    nvgFillColor(context, nvgRGB(246, 246, 249));
    nvgFontSize(context, 11.0f);
    nvgTextAlign(context, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    QString titleText = video.title;
    if (titleText.size() > 38)
        titleText = titleText.left(37) + QChar(0x2026);
    const QByteArray title = titleText.toUtf8();
    nvgTextBox(context, x + 128.0f, y + 8.0f,
               w - 143.0f, title.constData(), 0);
    nvgFontSize(context, 8.8f);
    nvgFillColor(context, nvgRGB(165, 165, 180));
    const QString metadata = QString::fromUtf8("%1  ·  播放 %2  ·  %3")
        .arg(video.owner.name.left(12))
        .arg(detailCount(video.stat.view))
        .arg(detailDuration(video.duration));
    const QByteArray metadataUtf8 = metadata.toUtf8();
    nvgText(context, x + 128.0f, y + h - 13.0f,
            metadataUtf8.constData(), 0);
}

void DetailScreen::drawActionButton(
    NVGcontext *context, const QRectF &frame,
    const QString &label, bool accent) const
{
    nvgBeginPath(context);
    nvgRoundedRect(context,
                   static_cast<float>(frame.x()),
                   static_cast<float>(frame.y()),
                   static_cast<float>(frame.width()),
                   static_cast<float>(frame.height()), 6.0f);
    nvgFillColor(context, accent
        ? nvgRGBA(251, 114, 153, 42)
        : nvgRGBA(255, 255, 255, 14));
    nvgFill(context);
    if (m_fontId >= 0) {
        const QByteArray utf8 = label.toUtf8();
        nvgFontFaceId(context, m_fontId);
        nvgFontSize(context, 9.2f);
        nvgTextAlign(context, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(context, accent
            ? nvgRGB(251, 142, 171)
            : nvgRGB(222, 222, 230));
        nvgText(context,
                static_cast<float>(frame.center().x()),
                static_cast<float>(frame.center().y()),
                utf8.constData(), 0);
    }
}

void DetailScreen::drawBackground(
    NVGcontext *context,
    float width,
    float height)
{
    NVGpaint background = nvgLinearGradient(
        context,
        0.0f,
        0.0f,
        width,
        height,
        nvgRGBA(31, 31, 40, 255),
        nvgRGBA(20, 21, 29, 255));
    nvgBeginPath(context);
    nvgRect(context, 0.0f, 0.0f, width, height);
    nvgFillPaint(context, background);
    nvgFill(context);
}

void DetailScreen::draw(NVGcontext *context, float width, float height)
{
    if (!m_header)
        return;
    layout(width, height);
    drawBackground(context, width, height);
    nvgSave(context);
    nvgScissor(context, 0.0f, 50.0f, width, height - 72.0f);
    m_cover->draw(context);
    m_infoPanel->draw(context);
    m_playButton->draw(context);
    drawActionButton(context, m_commentHitBox,
                     QString::fromUtf8("评论"), false);
    drawActionButton(context, m_likeHitBox,
                     QString::fromUtf8("点赞"), false);
    drawActionButton(context, m_coinHitBox,
                     QString::fromUtf8("投币"), false);
    drawActionButton(context, m_favoriteHitBox,
                     QString::fromUtf8("收藏"), true);
    drawActionButton(context, m_watchLaterHitBox,
                     QString::fromUtf8("稍后"), false);
    m_descriptionPanel->draw(context);
    if (!m_recommendations.isEmpty()) {
        const float headingY = static_cast<float>(
            m_recommendationHitBoxes.first().top()) - 12.0f;
        nvgFontFaceId(context, m_fontId);
        nvgFontSize(context, 11.0f);
        nvgTextAlign(context, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(context, nvgRGB(229, 229, 235));
        const QByteArray heading = QString::fromUtf8("相关推荐").toUtf8();
        nvgText(context, 12.0f, headingY, heading.constData(), 0);
        int index;
        for (index = 0; index < m_recommendationHitBoxes.size(); ++index)
            drawRecommendation(context, index,
                               m_recommendationHitBoxes.at(index));
    }
    nvgRestore(context);
    m_header->draw(context);
    m_footer->draw(context);
}

void DetailScreen::pointerPress(const QPoint &position)
{
    m_pressed = true;
    m_pressPosition = position;
    m_lastPosition = position;
}

void DetailScreen::pointerMove(const QPoint &position)
{
    if (!m_pressed)
        return;
    const float delta = static_cast<float>(
        position.y() - m_lastPosition.y());
    m_scroll = qBound(m_minScroll, m_scroll + delta, 0.0f);
    m_lastPosition = position;
}

DetailScreen::Action DetailScreen::pointerRelease(const QPoint &position)
{
    if (!m_pressed)
        return NoAction;
    m_pressed = false;
    if ((position - m_pressPosition).manhattanLength() >= 18)
        return NoAction;
    if (m_backHitBox.contains(QPointF(position)))
        return BackAction;
    if (m_playHitBox.contains(QPointF(position)))
        return PlayAction;
    if (m_ownerHitBox.contains(QPointF(position)))
        return OwnerAction;
    if (m_commentHitBox.contains(QPointF(position)))
        return CommentsAction;
    if (m_likeHitBox.contains(QPointF(position)))
        return LikeAction;
    if (m_coinHitBox.contains(QPointF(position)))
        return CoinAction;
    if (m_favoriteHitBox.contains(QPointF(position)))
        return FavoriteAction;
    if (m_watchLaterHitBox.contains(QPointF(position)))
        return WatchLaterAction;
    if (m_pageInfoHitBox.contains(QPointF(position)))
        return PageAction;
    int index;
    for (index = 0; index < m_recommendationHitBoxes.size(); ++index) {
        if (m_recommendationHitBoxes.at(index).contains(QPointF(position))) {
            return static_cast<Action>(
                RecommendationActionBase + index);
        }
    }
    return NoAction;
}

} // namespace wiliwili
