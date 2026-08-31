#include "ui/section_screen.h"

#include <QtCore/QByteArray>
#include <QtCore/QtGlobal>

#ifndef WILIWILI_SYMBIAN_VERSION_STR
#define WILIWILI_SYMBIAN_VERSION_STR "1.1.0"
#endif

namespace wiliwili {

static unsigned int sectionColorSeed(const QString &text)
{
    const QByteArray bytes = text.toUtf8();
    unsigned int value = 2166136261U;
    int index;
    for (index = 0; index < bytes.size(); ++index)
        value = (value ^ static_cast<unsigned char>(bytes.at(index))) *
            16777619U;
    return value;
}

static NVGcolor sectionAvatarColor(const QString &name, bool accented)
{
    const unsigned int seed = sectionColorSeed(name);
    return nvgRGB(
        static_cast<unsigned char>((accented ? 168 : 84) + (seed & 0x2f)),
        static_cast<unsigned char>((accented ? 91 : 100) +
                                   ((seed >> 6) & 0x3f)),
        static_cast<unsigned char>((accented ? 139 : 132) +
                                   ((seed >> 13) & 0x37)));
}

SectionScreen::SectionScreen()
    : m_fontId(-1), m_section(NavigationRail::DynamicSection),
      m_loggedIn(false), m_pressed(false), m_dragging(false),
      m_canLoadMore(false), m_loadMoreArmed(false),
      m_scroll(0.0f), m_minScroll(0.0f), m_pullDistance(0.0f),
      m_lastWidth(360.0f), m_lastHeight(640.0f), m_itemTop(76.0f),
      m_messageTab(0), m_imageLoadingEnabled(true),
      m_playbackMode(0), m_decoderMode(0),
      m_preferencePage(NoPreferencePage), m_aboutVisible(false)
{
    m_status = QString::fromLatin1("READY");
}

void SectionScreen::setLoggedIn(bool loggedIn)
{
    m_loggedIn = loggedIn;
}

void SectionScreen::setStatus(const QString &status)
{
    m_status = status;
}

void SectionScreen::setMessageTab(int tab)
{
    m_messageTab = qBound(0, tab, 3);
}

void SectionScreen::setImageLoadingEnabled(bool enabled)
{
    m_imageLoadingEnabled = enabled;
}

void SectionScreen::setPlaybackPreferences(
    int playbackMode, int decoderMode)
{
    m_playbackMode = qBound(0, playbackMode, 2);
    m_decoderMode = qBound(0, decoderMode, 2);
}

void SectionScreen::setPreferencePage(PreferencePage page)
{
    if (m_preferencePage == page)
        return;
    m_preferencePage = page;
    m_aboutVisible = false;
    m_scroll = 0.0f;
    m_pullDistance = 0.0f;
    m_loadMoreArmed = false;
}

bool SectionScreen::preferencePageVisible() const
{
    return m_preferencePage != NoPreferencePage;
}

void SectionScreen::setAboutVisible(bool visible)
{
    if (m_aboutVisible == visible)
        return;
    m_aboutVisible = visible;
    if (visible)
        m_preferencePage = NoPreferencePage;
    m_scroll = 0.0f;
    m_pullDistance = 0.0f;
    m_loadMoreArmed = false;
}

bool SectionScreen::aboutVisible() const
{
    return m_aboutVisible;
}

void SectionScreen::setItems(const QVector<ContentItemCompat> &items)
{
    m_items = items;
    m_scroll = 0.0f;
    m_pullDistance = 0.0f;
    m_loadMoreArmed = false;
}

void SectionScreen::appendItems(const QVector<ContentItemCompat> &items)
{
    int index;
    for (index = 0; index < items.size(); ++index) {
        const ContentItemCompat &candidate = items.at(index);
        if (candidate.title.trimmed().isEmpty())
            continue;
        bool duplicate = false;
        int existing;
        for (existing = 0; existing < m_items.size(); ++existing) {
            const ContentItemCompat &present = m_items.at(existing);
            if ((!candidate.id.isEmpty() && candidate.id == present.id) ||
                (candidate.id.isEmpty() && candidate.numericId != 0 &&
                 candidate.numericId == present.numericId) ||
                (candidate.id.isEmpty() && candidate.numericId == 0 &&
                 candidate.title == present.title &&
                 candidate.description == present.description)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            m_items.append(candidate);
    }
    m_loadMoreArmed = false;
}

QString SectionScreen::itemId(int index) const
{
    return index >= 0 && index < m_items.size()
        ? m_items.at(index).id : QString();
}

QString SectionScreen::itemTitle(int index) const
{
    return index >= 0 && index < m_items.size()
        ? m_items.at(index).title : QString();
}

QString SectionScreen::itemSubtitle(int index) const
{
    return index >= 0 && index < m_items.size()
        ? m_items.at(index).subtitle : QString();
}

QString SectionScreen::itemDescription(int index) const
{
    return index >= 0 && index < m_items.size()
        ? m_items.at(index).description : QString();
}

void SectionScreen::setCanLoadMore(bool canLoadMore)
{
    m_canLoadMore = canLoadMore;
    if (!m_canLoadMore)
        m_loadMoreArmed = false;
}

bool SectionScreen::isItemAction(Action action)
{
    return static_cast<int>(action) >= static_cast<int>(ItemActionBase);
}

int SectionScreen::itemIndex(Action action)
{
    return static_cast<int>(action) - static_cast<int>(ItemActionBase);
}

void SectionScreen::clearItems()
{
    m_items.clear();
    m_scroll = 0.0f;
    m_pullDistance = 0.0f;
    m_canLoadMore = false;
    m_loadMoreArmed = false;
}

void SectionScreen::initialize(int fontId)
{
    m_fontId = fontId;
}

void SectionScreen::setSection(NavigationRail::Section section)
{
    m_section = section;
    m_preferencePage = NoPreferencePage;
    m_scroll = 0.0f;
    m_pullDistance = 0.0f;
    m_loadMoreArmed = false;
}

void SectionScreen::drawText(
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

void SectionScreen::drawWrappedText(
    NVGcontext *context,
    const QString &text,
    float x,
    float y,
    float width,
    float size,
    int maxLines,
    const NVGcolor &color) const
{
    if (m_fontId < 0 || text.isEmpty() || width <= 1.0f)
        return;
    const QByteArray utf8 = text.toUtf8();
    const char *start = utf8.constData();
    const char *end = start + utf8.size();
    nvgFontFaceId(context, m_fontId);
    nvgFontSize(context, size);
    nvgTextAlign(context, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextLineHeight(context, 1.12f);
    nvgFillColor(context, color);
    int line = 0;
    while (start < end && line < maxLines) {
        NVGtextRow row;
        if (nvgTextBreakLines(context, start, end, width, &row, 1) < 1)
            break;
        nvgText(context, x, y + line * size * 1.12f, row.start, row.end);
        start = row.next;
        ++line;
    }
}

void SectionScreen::drawAvatar(
    NVGcontext *context,
    const QString &name,
    float centerX,
    float centerY,
    float size,
    bool accented) const
{
    nvgBeginPath(context);
    nvgCircle(context, centerX, centerY, size * 0.5f);
    nvgFillColor(context, sectionAvatarColor(name, accented));
    nvgFill(context);
    QString label = name.trimmed().left(1).toUpper();
    if (label.isEmpty())
        label = QString::fromLatin1("?");
    drawText(context, label, centerX, centerY + 0.5f,
             size * 0.36f, nvgRGB(250, 250, 253),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    if (accented) {
        nvgBeginPath(context);
        nvgCircle(context, centerX + size * 0.34f,
                  centerY + size * 0.34f, size * 0.13f);
        nvgFillColor(context, nvgRGB(251, 114, 153));
        nvgFill(context);
    }
}

void SectionScreen::drawInfoCard(
    NVGcontext *context,
    float x,
    float y,
    float width,
    const QString &title,
    const QString &subtitle) const
{
    nvgBeginPath(context);
    nvgRoundedRect(context, x, y, width, 82.0f, 12.0f);
    nvgFillColor(context, nvgRGBA(44, 44, 54, 246));
    nvgFill(context);
    QString safeTitle = title.simplified();
    QString safeSubtitle = subtitle.simplified();
    if (safeTitle.size() > 24)
        safeTitle = safeTitle.left(23) + QString::fromUtf8("…");
    if (safeSubtitle.size() > 38)
        safeSubtitle = safeSubtitle.left(37) + QString::fromUtf8("…");
    nvgSave(context);
    nvgScissor(context, x + 12.0f, y + 5.0f,
               qMax(1.0f, width - 24.0f), 70.0f);
    drawText(context, safeTitle, x + 14.0f, y + 24.0f, 14.0f,
             nvgRGB(244, 244, 247), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    drawText(context, safeSubtitle, x + 14.0f, y + 52.0f, 10.2f,
             nvgRGB(158, 158, 172), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgRestore(context);
}

void SectionScreen::drawAboutPage(
    NVGcontext *context, float left, float width)
{
    m_aboutBackHitBox = QRectF(0.0f, 0.0f, 96.0f, 52.0f);
    drawText(context, QString::fromUtf8("< 返回"), 12.0f, 26.0f, 13.0f,
             nvgRGB(251, 114, 153), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    drawText(context, QString::fromUtf8("关于 NIKINIKI"),
             width * 0.5f, 26.0f, 17.0f,
             nvgRGB(248, 248, 250), NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgBeginPath(context);
    nvgRect(context, left + 12.0f, 52.0f, width - left - 24.0f, 1.0f);
    nvgFillColor(context, nvgRGBA(255, 255, 255, 24));
    nvgFill(context);

    const float centerX = left + (width - left) * 0.5f;
    const float bodyWidth = qMax(1.0f, width - left - 32.0f);
    float y = 96.0f;
    drawText(context,
             QString::fromUtf8("NIKINIKI %1")
                 .arg(QString::fromLatin1(WILIWILI_SYMBIAN_VERSION_STR)),
             centerX, y, 23.0f, nvgRGB(251, 114, 153),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    y += 36.0f;
    drawText(context, QString::fromUtf8("基于 wiliwili 移植重构"),
             centerX, y, 12.0f, nvgRGB(242, 242, 247),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    y += 27.0f;
    drawText(context, QString::fromUtf8("欢迎加群：977410275"),
             centerX, y, 12.0f, nvgRGB(242, 242, 247),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    y += 27.0f;
    drawText(context, QString::fromUtf8("B站关注：南国飯店"),
             centerX, y, 12.0f, nvgRGB(242, 242, 247),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    y += 24.0f;
    drawWrappedText(
        context,
        QString::fromLatin1(
            "https://space.bilibili.com/1547820/"
            "?spm_id_from=333.788.upinfo.detail.click"),
        left + 16.0f, y, bodyWidth, 9.5f, 4, nvgRGB(138, 173, 235));
    y += 48.0f;
    drawText(context, QString::fromUtf8("谢谢喵"),
             centerX, y, 14.0f, nvgRGB(251, 150, 178),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
}

void SectionScreen::drawPreferencePage(
    NVGcontext *context, float left, float width, float height)
{
    m_minScroll = 0.0f;
    m_scroll = 0.0f;
    m_preferenceBackHitBox = QRectF(0.0f, 0.0f, 104.0f, 52.0f);
    drawText(context, QString::fromUtf8("< 返回设置"),
             12.0f, 26.0f, 13.0f, nvgRGB(251, 114, 153),
             NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    const bool playback = m_preferencePage == PlaybackPreferencePage;
    drawText(context,
             playback ? QString::fromUtf8("选择播放方式")
                      : QString::fromUtf8("选择解码方式"),
             width * 0.5f, 26.0f, 17.0f,
             nvgRGB(248, 248, 250), NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgBeginPath(context);
    nvgRect(context, left + 12.0f, 52.0f, width - left - 24.0f, 1.0f);
    nvgFillColor(context, nvgRGBA(255, 255, 255, 24));
    nvgFill(context);

    const QString playbackTitles[] = {
        QString::fromUtf8("流式播放"),
        QString::fromUtf8("OpenFileL 边下边播"),
        QString::fromUtf8("下载后播放")
    };
    const QString playbackSubtitles[] = {
        QString::fromUtf8("直接把网络地址交给 MMF OpenUrlL"),
        QString::fromUtf8("预缓冲后打开持续增长的本地文件"),
        QString::fromUtf8("完整下载并显示进度，完成后开始播放")
    };
    const QString decoderTitles[] = {
        QString::fromUtf8("自动选择"),
        QString::fromUtf8("全程硬解"),
        QString::fromUtf8("全程软解")
    };
    const QString decoderSubtitles[] = {
        QString::fromUtf8("兼容时硬解，否则回退本机软件视频"),
        QString::fromUtf8("始终使用 MMF，无法解码时可能黑屏"),
        QString::fromUtf8("点播 MP4 始终使用本机软件视频")
    };
    const int selected = playback ? m_playbackMode : m_decoderMode;
    const float cardX = left + 14.0f;
    const float cardWidth = width - left - 28.0f;
    const float cardHeight = 92.0f;
    const float cardGap = 14.0f;
    float cardY = 78.0f;
    int index;
    for (index = 0; index < 3; ++index) {
        m_preferenceOptionHitBoxes[index] =
            QRectF(cardX, cardY, cardWidth, cardHeight);
        nvgBeginPath(context);
        nvgRoundedRect(context, cardX, cardY,
                       cardWidth, cardHeight, 12.0f);
        nvgFillColor(context, index == selected
            ? nvgRGBA(251, 114, 153, 36)
            : nvgRGBA(44, 44, 54, 246));
        nvgFill(context);
        nvgStrokeWidth(context, index == selected ? 1.5f : 1.0f);
        nvgStrokeColor(context, index == selected
            ? nvgRGBA(251, 114, 153, 210)
            : nvgRGBA(255, 255, 255, 18));
        nvgStroke(context);

        nvgBeginPath(context);
        nvgCircle(context, cardX + 22.0f, cardY + 46.0f, 8.0f);
        nvgStrokeWidth(context, 1.5f);
        nvgStrokeColor(context, index == selected
            ? nvgRGB(251, 114, 153) : nvgRGB(132, 132, 146));
        nvgStroke(context);
        if (index == selected) {
            nvgBeginPath(context);
            nvgCircle(context, cardX + 22.0f, cardY + 46.0f, 4.0f);
            nvgFillColor(context, nvgRGB(251, 114, 153));
            nvgFill(context);
        }
        drawText(context,
                 playback ? playbackTitles[index] : decoderTitles[index],
                 cardX + 42.0f, cardY + 31.0f, 13.5f,
                 nvgRGB(244, 244, 247),
                 NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        drawText(context,
                 playback ? playbackSubtitles[index]
                          : decoderSubtitles[index],
                 cardX + 42.0f, cardY + 59.0f, 9.5f,
                 nvgRGB(164, 164, 177),
                 NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        cardY += cardHeight + cardGap;
    }
    drawText(context,
             QString::fromUtf8("选择后立即保存，下次播放生效"),
             width * 0.5f,
             qMin(height - NavigationRail::height() - 20.0f, cardY + 14.0f),
             10.0f, nvgRGB(148, 148, 162),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
}

float SectionScreen::itemHeight(int index) const
{
    if (index < 0 || index >= m_items.size())
        return 76.0f;
    const ContentItemCompat &item = m_items.at(index);
    if (m_section == NavigationRail::DynamicSection) {
        const bool hasAttachment = !item.mediaTitle.trimmed().isEmpty() ||
            !item.mediaPicture.trimmed().isEmpty();
        return hasAttachment ? 178.0f : 122.0f;
    }
    if (m_section == NavigationRail::MessagesSection)
        return m_messageTab == 3 ? 70.0f : 76.0f;
    return 82.0f;
}

QRectF SectionScreen::itemFrame(int index) const
{
    const float cardX = NavigationRail::width() + 14.0f;
    float y = m_itemTop + m_scroll + m_pullDistance;
    int previous;
    for (previous = 0; previous < index; ++previous)
        y += itemHeight(previous) +
            (m_section == NavigationRail::DynamicSection ? 8.0f : 1.0f);
    return QRectF(cardX, y, m_lastWidth - cardX - 14.0f,
                  itemHeight(index));
}

void SectionScreen::drawDynamicCard(
    NVGcontext *context,
    const ContentItemCompat &item,
    const QRectF &frame) const
{
    const float x = static_cast<float>(frame.x());
    const float y = static_cast<float>(frame.y());
    const float width = static_cast<float>(frame.width());
    const float height = static_cast<float>(frame.height());
    const bool hasAttachment = !item.mediaTitle.trimmed().isEmpty() ||
        !item.mediaPicture.trimmed().isEmpty();

    nvgBeginPath(context);
    nvgRoundedRect(context, x, y, width, height, 11.0f);
    nvgFillColor(context, nvgRGBA(43, 43, 53, 246));
    nvgFill(context);
    nvgSave(context);
    nvgScissor(context, x + 1.0f, y + 1.0f, width - 2.0f, height - 2.0f);

    QString author = item.title;
    if (author.size() > 14)
        author = author.left(13) + QString::fromUtf8("…");
    drawAvatar(context, item.title, x + 25.0f, y + 22.0f, 31.0f, false);
    drawText(context, author, x + 47.0f, y + 16.0f, 11.2f,
             nvgRGB(248, 206, 219), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    drawText(context, item.subtitle, x + 47.0f, y + 30.0f, 8.4f,
             nvgRGB(156, 156, 171), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    QString body = item.description;
    if (body.isEmpty())
        body = item.mediaTitle;
    drawWrappedText(context, body, x + 11.0f, y + 43.0f,
                    width - 22.0f, 10.0f, hasAttachment ? 2 : 3,
                    nvgRGB(242, 242, 247));

    if (hasAttachment) {
        const float mediaY = y + 72.0f;
        const float mediaHeight = 78.0f;
        NVGpaint mediaPaint = nvgLinearGradient(
            context, x + 10.0f, mediaY, x + width - 10.0f,
            mediaY + mediaHeight, nvgRGBA(108, 77, 122, 242),
            nvgRGBA(48, 91, 128, 242));
        nvgBeginPath(context);
        nvgRoundedRect(context, x + 10.0f, mediaY, width - 20.0f,
                       mediaHeight, 7.0f);
        nvgFillPaint(context, mediaPaint);
        nvgFill(context);
        nvgBeginPath(context);
        nvgRoundedRect(context, x + 17.0f, mediaY + 8.0f,
                       32.0f, 15.0f, 4.0f);
        nvgFillColor(context, nvgRGBA(21, 21, 29, 142));
        nvgFill(context);
        drawText(context, item.badge.isEmpty()
                     ? QString::fromUtf8("动态") : item.badge,
                 x + 33.0f, mediaY + 15.5f, 7.6f,
                 nvgRGB(251, 245, 250), NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        QString mediaTitle = item.mediaTitle;
        if (mediaTitle.isEmpty())
            mediaTitle = QString::fromUtf8("图片动态");
        if (item.badge == QString::fromUtf8("图文")) {
            const float gridGap = 4.0f;
            const float gridWidth = (width - 42.0f - gridGap * 2.0f) / 3.0f;
            const NVGcolor gridColors[] = {
                nvgRGBA(237, 148, 181, 216),
                nvgRGBA(242, 185, 120, 216),
                nvgRGBA(148, 185, 235, 216)
            };
            int gridIndex;
            for (gridIndex = 0; gridIndex < 3; ++gridIndex) {
                nvgBeginPath(context);
                nvgRoundedRect(context,
                               x + 17.0f + gridIndex * (gridWidth + gridGap),
                               mediaY + 31.0f, gridWidth, 38.0f, 4.0f);
                nvgFillColor(context, gridColors[gridIndex]);
                nvgFill(context);
            }
        } else {
            drawWrappedText(context, mediaTitle, x + 17.0f, mediaY + 31.0f,
                            width - 64.0f, 9.5f, 2,
                            nvgRGB(250, 250, 253));
        }
        if (item.kind == VideoContentItem) {
            nvgBeginPath(context);
            nvgCircle(context, x + width - 26.0f, mediaY + 51.0f, 12.0f);
            nvgFillColor(context, nvgRGBA(21, 21, 29, 166));
            nvgFill(context);
            nvgBeginPath(context);
            nvgMoveTo(context, x + width - 29.0f, mediaY + 45.0f);
            nvgLineTo(context, x + width - 29.0f, mediaY + 57.0f);
            nvgLineTo(context, x + width - 19.0f, mediaY + 51.0f);
            nvgClosePath(context);
            nvgFillColor(context, nvgRGB(255, 255, 255));
            nvgFill(context);
        }
    }

    drawText(context,
             QString::fromUtf8("转发  ·  评论 %1  ·  赞 %2")
                 .arg(item.replyCount).arg(item.count),
             x + 12.0f, y + height - 13.0f, 8.7f,
             nvgRGB(166, 166, 180), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgRestore(context);
}

void SectionScreen::drawMessageRow(
    NVGcontext *context,
    const ContentItemCompat &item,
    const QRectF &frame) const
{
    const float x = static_cast<float>(frame.x());
    const float y = static_cast<float>(frame.y());
    const float width = static_cast<float>(frame.width());
    const float height = static_cast<float>(frame.height());
    const bool unread = item.pinned || item.count > 0;

    nvgBeginPath(context);
    nvgRoundedRect(context, x, y, width, height, 8.0f);
    nvgFillColor(context, nvgRGBA(39, 40, 49, 238));
    nvgFill(context);
    QString author = item.title;
    if (author.size() > 13)
        author = author.left(12) + QString::fromUtf8("…");
    drawAvatar(context, item.title, x + 25.0f, y + height * 0.5f,
               37.0f, unread);
    nvgSave(context);
    nvgScissor(context, x + 46.0f, y + 2.0f, width - 58.0f, height - 4.0f);
    drawText(context, author, x + 51.0f, y + 18.0f, 11.5f,
             unread ? nvgRGB(249, 218, 229) : nvgRGB(245, 245, 248),
             NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    drawText(context, item.subtitle, x + width - 13.0f, y + 18.0f, 8.1f,
             nvgRGB(157, 157, 171), NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    QString preview = item.description;
    if (m_messageTab != 3 && !item.badge.isEmpty())
        preview = item.badge + QString::fromUtf8(" · ") + preview;
    if (preview.isEmpty())
        preview = QString::fromUtf8("点击查看消息内容");
    drawWrappedText(context, preview, x + 51.0f, y + 34.0f,
                    width - 65.0f, 9.5f, 1,
                    nvgRGB(174, 174, 187));
    nvgRestore(context);
    if (unread) {
        nvgBeginPath(context);
        nvgCircle(context, x + width - 13.0f, y + height - 14.0f, 4.0f);
        nvgFillColor(context, nvgRGB(251, 100, 129));
        nvgFill(context);
    }
}

void SectionScreen::draw(NVGcontext *context, float width, float height)
{
    m_lastWidth = width;
    m_lastHeight = height;
    const float left = NavigationRail::width();
    const float contentBottom = height - NavigationRail::height();
    NVGpaint background = nvgLinearGradient(
        context, left, 0.0f, width, height,
        nvgRGBA(31, 31, 40, 255), nvgRGBA(20, 21, 29, 255));
    nvgBeginPath(context);
    nvgRect(context, left, 0.0f, width - left, height);
    nvgFillPaint(context, background);
    nvgFill(context);

    if (m_aboutVisible) {
        drawAboutPage(context, left, width);
        return;
    }
    if (m_preferencePage != NoPreferencePage) {
        drawPreferencePage(context, left, width, height);
        return;
    }

    QString title;
    QString subtitle;
    if (m_section == NavigationRail::DynamicSection) {
        title = QString::fromUtf8("动态");
        subtitle = QString::fromUtf8("关注的 UP 主最新内容");
    } else if (m_section == NavigationRail::MessagesSection) {
        title = QString::fromUtf8("消息");
        subtitle = QString::fromUtf8("回复、@、赞和私信");
    } else {
        title = QString::fromUtf8("设置");
        subtitle = QString::fromLatin1("wiliwili for Symbian 3");
    }

    drawText(context, title, left + 16.0f, 31.0f, 20.0f,
             nvgRGB(248, 248, 250), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    drawText(context, subtitle, width - 14.0f, 31.0f, 10.0f,
             nvgRGB(148, 148, 160), NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);

    const float cardX = left + 14.0f;
    const float cardWidth = width - left - 28.0f;
    float itemTop = 58.0f;
    m_exitHitBox = QRectF();
    m_networkHitBox = QRectF();
    m_imagesHitBox = QRectF();
    m_playbackModeHitBox = QRectF();
    m_decoderModeHitBox = QRectF();
    if (m_section == NavigationRail::MessagesSection) {
        const float quickGap = 7.0f;
        const float quickWidth = (cardWidth - quickGap * 2.0f) / 3.0f;
        const float quickY = 47.0f;
        const float quickHeight = 66.0f;
        const QRectF replyVisual(cardX, quickY, quickWidth, quickHeight);
        const QRectF likeVisual(cardX + quickWidth + quickGap,
                                quickY, quickWidth, quickHeight);
        const QRectF chatVisual(cardX + (quickWidth + quickGap) * 2.0f,
                                quickY, quickWidth, quickHeight);
        m_replyTabHitBox = QRectF(cardX, quickY,
                                  quickWidth * 0.68f, quickHeight);
        m_atTabHitBox = QRectF(cardX + quickWidth * 0.68f, quickY,
                               quickWidth * 0.32f, quickHeight);
        m_likeTabHitBox = likeVisual;
        m_chatTabHitBox = chatVisual;
        const QRectF quickFrames[] = { replyVisual, likeVisual, chatVisual };
        const QString labels[] = {
            QString::fromUtf8("回复与@"),
            QString::fromUtf8("收到喜欢"),
            QString::fromUtf8("私信会话")
        };
        const QString icons[] = {
            QString::fromUtf8("回"),
            QString::fromUtf8("赞"),
            QString::fromUtf8("信")
        };
        int quickIndex;
        for (quickIndex = 0; quickIndex < 3; ++quickIndex) {
            const QRectF &quick = quickFrames[quickIndex];
            const bool selected = quickIndex == 0
                ? m_messageTab == 0 || m_messageTab == 1
                : quickIndex == 1 ? m_messageTab == 2 : m_messageTab == 3;
            const NVGcolor iconColor = quickIndex == 0
                ? nvgRGB(64, 207, 169)
                : quickIndex == 1 ? nvgRGB(251, 100, 157)
                                  : nvgRGB(75, 181, 223);
            nvgBeginPath(context);
            nvgRoundedRect(context,
                           static_cast<float>(quick.x()),
                           static_cast<float>(quick.y()),
                           static_cast<float>(quick.width()),
                           static_cast<float>(quick.height()), 9.0f);
            nvgFillColor(context, selected
                ? nvgRGBA(255, 255, 255, 24)
                : nvgRGBA(255, 255, 255, 11));
            nvgFill(context);
            const float centerX = static_cast<float>(quick.center().x());
            nvgBeginPath(context);
            nvgCircle(context, centerX,
                      static_cast<float>(quick.y()) + 23.0f, 15.0f);
            nvgFillColor(context, iconColor);
            nvgFill(context);
            drawText(context, icons[quickIndex], centerX,
                     static_cast<float>(quick.y()) + 23.5f, 10.0f,
                     nvgRGB(250, 250, 253),
                     NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            drawText(context, labels[quickIndex], centerX,
                     static_cast<float>(quick.y()) + 52.0f, 9.2f,
                     selected ? nvgRGB(250, 223, 233)
                              : nvgRGB(190, 190, 202),
                     NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }
        itemTop = 129.0f;
    } else {
        m_replyTabHitBox = QRectF();
        m_atTabHitBox = QRectF();
        m_likeTabHitBox = QRectF();
        m_chatTabHitBox = QRectF();
    }
    m_itemTop = itemTop;
    if ((m_section == NavigationRail::DynamicSection ||
         m_section == NavigationRail::MessagesSection) && m_loggedIn) {
        float contentHeight = 0.0f;
        int index;
        for (index = 0; index < m_items.size(); ++index) {
            contentHeight += itemHeight(index);
            if (index + 1 < m_items.size())
                contentHeight += m_section == NavigationRail::DynamicSection
                    ? 8.0f : 1.0f;
        }
        if (m_canLoadMore && !m_items.isEmpty())
            contentHeight += 28.0f;
        m_minScroll = qMin(0.0f,
            contentBottom - itemTop - contentHeight - 4.0f);
        m_scroll = qBound(m_minScroll, m_scroll, 0.0f);
        nvgSave(context);
        nvgScissor(context, left, itemTop, width - left,
                   qMax(1.0f, contentBottom - itemTop));
        if (m_pullDistance > 1.0f) {
            drawText(context,
                     m_pullDistance >= 50.0f
                         ? QString::fromUtf8("松开刷新")
                         : QString::fromUtf8("下拉刷新"),
                     left + (width - left) * 0.5f,
                     itemTop + m_pullDistance * 0.5f,
                     10.5f, nvgRGB(251, 114, 153),
                     NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }
        if (m_items.isEmpty()) {
            drawText(context, m_status, left + (width - left) * 0.5f,
                     itemTop + 38.0f, 11.0f, nvgRGB(164, 164, 178),
                     NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }
        for (index = 0; index < m_items.size(); ++index) {
            const QRectF frame = itemFrame(index);
            if (frame.bottom() < itemTop || frame.top() > contentBottom)
                continue;
            if (m_section == NavigationRail::DynamicSection)
                drawDynamicCard(context, m_items.at(index), frame);
            else
                drawMessageRow(context, m_items.at(index), frame);
        }
        if (m_canLoadMore && !m_items.isEmpty()) {
            const QRectF last = itemFrame(m_items.size() - 1);
            drawText(context, QString::fromUtf8("继续上滑加载更多"),
                     left + (width - left) * 0.5f,
                     static_cast<float>(last.bottom()) + 16.0f,
                     9.5f, nvgRGB(158, 158, 173),
                     NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }
        nvgRestore(context);
    } else if (m_section == NavigationRail::DynamicSection) {
        m_minScroll = 0.0f;
        m_scroll = 0.0f;
        drawInfoCard(context, cardX, 76.0f, cardWidth,
                     QString::fromLatin1("DYNAMIC FEED"),
                     m_loggedIn ? m_status
                                : QString::fromUtf8("登录后读取关注动态"));
        drawInfoCard(context, cardX, 170.0f, cardWidth,
                     QString::fromLatin1("VIDEO / IMAGE / TEXT"),
                     QString::fromUtf8("视频、图文和纯文本动态统一卡片流"));
    } else if (m_section == NavigationRail::MessagesSection) {
        m_minScroll = 0.0f;
        m_scroll = 0.0f;
        drawInfoCard(context, cardX, itemTop + 8.0f, cardWidth,
                     QString::fromLatin1("MESSAGE CENTER"),
                     m_loggedIn ? m_status
                                : QString::fromUtf8("登录后读取互动与私信"));
    } else {
        const float cardHeight = 82.0f;
        const float cardGap = 12.0f;
        const int cardCount = 9;
        const float cardsTop = 76.0f;
        const float buttonY = contentBottom - 48.0f;
        const float cardsBottom = buttonY - 8.0f;
        const float totalHeight = cardCount * cardHeight +
            (cardCount - 1) * cardGap;
        m_minScroll = qMin(0.0f,
            cardsBottom - cardsTop - totalHeight);
        m_scroll = qBound(m_minScroll, m_scroll, 0.0f);
        float cardY = cardsTop + m_scroll;

        m_networkHitBox = QRectF(cardX, cardY, cardWidth, cardHeight);
        m_imagesHitBox = QRectF();
        m_playbackModeHitBox = QRectF();
        m_decoderModeHitBox = QRectF();
        m_cacheHitBox = QRectF();
        m_aboutHitBox = QRectF();
        nvgSave(context);
        nvgScissor(context, left, cardsTop - 8.0f, width - left,
                   qMax(1.0f, cardsBottom - cardsTop + 8.0f));
        drawInfoCard(context, cardX, cardY, cardWidth,
                     QString::fromLatin1("NETWORK"),
                     m_status == QString::fromLatin1("READY")
                         ? QString::fromUtf8("点击运行 HTTPS / Bilibili API 诊断")
                         : m_status);
        cardY += cardHeight + cardGap;
        m_imagesHitBox = QRectF(cardX, cardY, cardWidth, cardHeight);
        drawInfoCard(context, cardX, cardY, cardWidth,
                     m_imageLoadingEnabled
                         ? QString::fromLatin1("IMAGES ON")
                         : QString::fromLatin1("IMAGES OFF"),
                     QString::fromUtf8("点击切换列表缩略图（节省内存/流量）"));
        cardY += cardHeight + cardGap;
        m_playbackModeHitBox = QRectF(
            cardX, cardY, cardWidth, cardHeight);
        const QString playbackTitles[] = {
            QString::fromLatin1("PLAYBACK / OPENURL"),
            QString::fromLatin1("PLAYBACK / OPENFILE"),
            QString::fromLatin1("PLAYBACK / DOWNLOAD")
        };
        const QString playbackSubtitles[] = {
            QString::fromUtf8("当前：流式播放（OpenUrlL）· 点击选择"),
            QString::fromUtf8("当前：OpenFileL 边下边播 · 点击选择"),
            QString::fromUtf8("当前：完整下载后播放 · 点击选择")
        };
        drawInfoCard(context, cardX, cardY, cardWidth,
                     playbackTitles[m_playbackMode],
                     playbackSubtitles[m_playbackMode]);
        cardY += cardHeight + cardGap;
        m_decoderModeHitBox = QRectF(
            cardX, cardY, cardWidth, cardHeight);
        const QString decoderTitles[] = {
            QString::fromLatin1("DECODER / AUTO"),
            QString::fromLatin1("DECODER / HARDWARE"),
            QString::fromLatin1("DECODER / SOFTWARE")
        };
        const QString decoderSubtitles[] = {
            QString::fromUtf8("当前：自动选择 · 点击选择"),
            QString::fromUtf8("当前：全程硬解 · 点击选择"),
            QString::fromUtf8("当前：全程软解 · 点击选择")
        };
        drawInfoCard(context, cardX, cardY, cardWidth,
                     decoderTitles[m_decoderMode],
                     decoderSubtitles[m_decoderMode]);
        cardY += cardHeight + cardGap;
        m_cacheHitBox = QRectF(cardX, cardY, cardWidth, cardHeight);
        drawInfoCard(context, cardX, cardY, cardWidth,
                     QString::fromLatin1("CACHE"),
                     QString::fromUtf8("点击清理临时播放缓存"));
        cardY += cardHeight + cardGap;
        m_aboutHitBox = QRectF(cardX, cardY, cardWidth, cardHeight);
        drawInfoCard(context, cardX, cardY, cardWidth,
                     QString::fromLatin1("ABOUT"),
                     QString::fromUtf8("NIKINIKI 版本信息 / 关注"));
        cardY += cardHeight + cardGap;
        drawInfoCard(context, cardX, cardY, cardWidth,
                     QString::fromLatin1("STARTUP"),
                     QString::fromUtf8("AppArc 安全冷启动 / 首帧后全屏"));
        cardY += cardHeight + cardGap;
        drawInfoCard(context, cardX, cardY, cardWidth,
                     QString::fromLatin1("VERSION %1")
                         .arg(QString::fromLatin1(
                             WILIWILI_SYMBIAN_VERSION_STR)),
                     QString::fromUtf8("单窗口播放器 / WBI 搜索修复"));
        cardY += cardHeight + cardGap;
        drawInfoCard(context, cardX, cardY, cardWidth,
                     QString::fromLatin1("PLAYER / MMF"),
                     QString::fromUtf8("H.264 硬解、横屏控制、倍速与弹幕"));
        nvgRestore(context);

        m_exitHitBox = QRectF(cardX, buttonY, cardWidth, 40.0f);
        nvgBeginPath(context);
        nvgRoundedRect(context, cardX, buttonY, cardWidth, 40.0f, 10.0f);
        nvgFillColor(context, nvgRGBA(251, 114, 153, 42));
        nvgFill(context);
        nvgStrokeWidth(context, 1.0f);
        nvgStrokeColor(context, nvgRGBA(251, 114, 153, 155));
        nvgStroke(context);
        drawText(context, QString::fromUtf8("EXIT APPLICATION / 退出程序"),
                 cardX + cardWidth * 0.5f, buttonY + 20.0f, 11.5f,
                 nvgRGB(251, 150, 178),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }

    if (m_section != NavigationRail::SettingsSection)
        m_exitHitBox = QRectF();
    if (m_section != NavigationRail::SettingsSection) {
        m_networkHitBox = QRectF();
        m_imagesHitBox = QRectF();
        m_playbackModeHitBox = QRectF();
        m_decoderModeHitBox = QRectF();
        m_cacheHitBox = QRectF();
        m_aboutHitBox = QRectF();
    }
}

void SectionScreen::pointerPress(const QPoint &position)
{
    m_pressed = true;
    m_dragging = true;
    m_pressPosition = position;
    m_lastPosition = position;
}

void SectionScreen::pointerMove(const QPoint &position)
{
    if (!m_dragging)
        return;
    if (m_aboutVisible || m_preferencePage != NoPreferencePage) {
        m_lastPosition = position;
        return;
    }
    const float delta = static_cast<float>(
        position.y() - m_lastPosition.y());
    if (m_scroll >= 0.0f && (m_pullDistance > 0.0f || delta > 0.0f)) {
        m_pullDistance = qBound(0.0f, m_pullDistance + delta, 72.0f);
    } else {
        m_scroll = qBound(m_minScroll, m_scroll + delta, 0.0f);
        if (m_canLoadMore && delta < -1.0f &&
            m_scroll <= m_minScroll + 0.5f) {
            m_loadMoreArmed = true;
        } else if (m_scroll > m_minScroll + 8.0f) {
            m_loadMoreArmed = false;
        }
    }
    m_lastPosition = position;
}

SectionScreen::Action SectionScreen::pointerRelease(
    const QPoint &position)
{
    if (!m_pressed)
        return NoAction;
    m_pressed = false;
    m_dragging = false;
    if (m_pullDistance >= 50.0f) {
        m_pullDistance = 0.0f;
        m_scroll = 0.0f;
        return RefreshAction;
    }
    if (m_pullDistance > 0.0f) {
        m_pullDistance = 0.0f;
        return NoAction;
    }
    if (m_canLoadMore && m_loadMoreArmed &&
        (position - m_pressPosition).manhattanLength() >= 18) {
        m_loadMoreArmed = false;
        return LoadMoreAction;
    }
    if ((position - m_pressPosition).manhattanLength() >= 18)
        return NoAction;
    if (m_section == NavigationRail::SettingsSection &&
        m_aboutVisible) {
        if (m_aboutBackHitBox.contains(QPointF(position)))
            return AboutBackAction;
        return NoAction;
    }
    if (m_section == NavigationRail::SettingsSection &&
        m_preferencePage != NoPreferencePage) {
        if (m_preferenceBackHitBox.contains(QPointF(position)))
            return PreferenceBackAction;
        int index;
        for (index = 0; index < 3; ++index) {
            if (!m_preferenceOptionHitBoxes[index].contains(
                    QPointF(position))) {
                continue;
            }
            if (m_preferencePage == PlaybackPreferencePage) {
                return static_cast<Action>(
                    SelectUrlStreamingAction + index);
            }
            return static_cast<Action>(
                SelectAutomaticDecoderAction + index);
        }
        return NoAction;
    }
    if (m_section == NavigationRail::SettingsSection &&
        m_exitHitBox.contains(QPointF(position)))
        return ExitApplicationAction;
    if (m_section == NavigationRail::SettingsSection &&
        m_networkHitBox.contains(QPointF(position)))
        return NetworkTestAction;
    if (m_section == NavigationRail::SettingsSection &&
        m_imagesHitBox.contains(QPointF(position)))
        return ToggleImagesAction;
    if (m_section == NavigationRail::SettingsSection &&
        m_playbackModeHitBox.contains(QPointF(position)))
        return OpenPlaybackModeAction;
    if (m_section == NavigationRail::SettingsSection &&
        m_decoderModeHitBox.contains(QPointF(position)))
        return OpenDecoderModeAction;
    if (m_section == NavigationRail::SettingsSection &&
        m_cacheHitBox.contains(QPointF(position)))
        return ClearCacheAction;
    if (m_section == NavigationRail::SettingsSection &&
        m_aboutHitBox.contains(QPointF(position)))
        return AboutAction;
    if (m_section == NavigationRail::MessagesSection) {
        if (m_replyTabHitBox.contains(QPointF(position)))
            return ReplyTabAction;
        if (m_atTabHitBox.contains(QPointF(position)))
            return AtTabAction;
        if (m_likeTabHitBox.contains(QPointF(position)))
            return LikeTabAction;
        if (m_chatTabHitBox.contains(QPointF(position)))
            return ChatTabAction;
    }
    if ((m_section == NavigationRail::DynamicSection ||
         m_section == NavigationRail::MessagesSection) &&
        m_loggedIn) {
        int index;
        for (index = 0; index < m_items.size(); ++index) {
            if (itemFrame(index).contains(QPointF(position)))
                return static_cast<Action>(ItemActionBase + index);
        }
    }
    return NoAction;
}

} // namespace wiliwili
