#include "ui/content_screen.h"

#include <QtCore/QByteArray>
#include <QtCore/QStringList>
#include <QtCore/QtGlobal>

namespace wiliwili {

static const float kContentHeaderHeight = 58.0f;
static const float kContentItemHeight = 86.0f;
static const float kCommentItemHeight = 112.0f;
static const float kCommentReplyItemHeight = 102.0f;
static const float kCommentPreviewExtra = 72.0f;
static const float kContentGap = 8.0f;

static unsigned int contentColorSeed(const QString &text)
{
    const QByteArray bytes = text.toUtf8();
    unsigned int value = 2166136261U;
    int index;
    for (index = 0; index < bytes.size(); ++index)
        value = (value ^ static_cast<unsigned char>(bytes.at(index))) *
            16777619U;
    return value;
}

static NVGcolor commentAvatarColor(const QString &name)
{
    const unsigned int seed = contentColorSeed(name);
    return nvgRGB(
        static_cast<unsigned char>(92 + (seed & 0x3f)),
        static_cast<unsigned char>(102 + ((seed >> 6) & 0x4f)),
        static_cast<unsigned char>(126 + ((seed >> 13) & 0x55)));
}

ContentScreen::ContentScreen()
    : m_fontId(-1), m_width(360.0f), m_height(640.0f),
       m_scroll(0.0f), m_minScroll(0.0f), m_pullDistance(0.0f),
       m_dragging(false), m_canLoadMore(false), m_loadMoreArmed(false)
{
    m_status = QString::fromLatin1("READY");
}

void ContentScreen::initialize(int fontId)
{
    m_fontId = fontId;
}

void ContentScreen::setTitle(
    const QString &title, const QString &subtitle)
{
    m_title = title;
    m_subtitle = subtitle;
    m_scroll = 0.0f;
    m_pullDistance = 0.0f;
}

void ContentScreen::setStatus(const QString &status)
{
    m_status = status;
}

void ContentScreen::setHeaderAction(const QString &label)
{
    m_headerAction = label;
    m_secondaryHeaderAction.clear();
}

void ContentScreen::setSecondaryHeaderAction(const QString &label)
{
    m_secondaryHeaderAction = label;
}

void ContentScreen::setItems(const QVector<ContentItemCompat> &items)
{
    m_items = items;
    m_itemImages.clear();
    m_scroll = 0.0f;
    m_pullDistance = 0.0f;
    m_loadMoreArmed = false;
}

void ContentScreen::appendItems(const QVector<ContentItemCompat> &items)
{
    int index;
    for (index = 0; index < items.size(); ++index) {
        const ContentItemCompat &candidate = items.at(index);
        bool duplicate = false;
        int existing;
        for (existing = 0; existing < m_items.size(); ++existing) {
            const ContentItemCompat &present = m_items.at(existing);
            if ((!candidate.id.isEmpty() && candidate.id == present.id) ||
                (candidate.id.isEmpty() && candidate.numericId != 0 &&
                 candidate.numericId == present.numericId)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            m_items.append(candidate);
            m_itemImages.append(-1);
        }
    }
    m_loadMoreArmed = false;
}

void ContentScreen::setItemImages(const QVector<int> &images)
{
    m_itemImages = images;
}

void ContentScreen::setItemImage(int index, int imageHandle)
{
    if (index >= 0 && index < m_itemImages.size())
        m_itemImages[index] = imageHandle;
}

void ContentScreen::setCanLoadMore(bool canLoadMore)
{
    m_canLoadMore = canLoadMore;
    if (!m_canLoadMore)
        m_loadMoreArmed = false;
}

const QVector<ContentItemCompat> &ContentScreen::items() const
{
    return m_items;
}

ContentScreen::State ContentScreen::state() const
{
    State result;
    result.title = m_title;
    result.subtitle = m_subtitle;
    result.status = m_status;
    result.headerAction = m_headerAction;
    result.secondaryHeaderAction = m_secondaryHeaderAction;
    result.items = m_items;
    result.itemImages = m_itemImages;
    result.scroll = m_scroll;
    result.canLoadMore = m_canLoadMore;
    return result;
}

void ContentScreen::restoreState(const State &state, bool restoreImages)
{
    m_title = state.title;
    m_subtitle = state.subtitle;
    m_status = state.status;
    m_headerAction = state.headerAction;
    m_secondaryHeaderAction = state.secondaryHeaderAction;
    m_items = state.items;
    if (restoreImages && state.itemImages.size() == m_items.size()) {
        m_itemImages = state.itemImages;
    } else {
        m_itemImages.clear();
        int index;
        for (index = 0; index < m_items.size(); ++index)
            m_itemImages.append(-1);
    }
    m_scroll = state.scroll;
    m_pullDistance = 0.0f;
    m_dragging = false;
    m_canLoadMore = state.canLoadMore;
    m_loadMoreArmed = false;
}

void ContentScreen::drawText(
    NVGcontext *context, const QString &text,
    float x, float y, float size,
    const NVGcolor &color, int align) const
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

void ContentScreen::drawWrappedText(
    NVGcontext *context, const QString &text,
    float x, float y, float width, float size,
    int maxLines, const NVGcolor &color) const
{
    if (m_fontId < 0 || text.isEmpty())
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

void ContentScreen::updateLayout(float width, float height)
{
    m_width = width;
    m_height = height;
    float contentHeight = 12.0f;
    int index;
    for (index = 0; index < m_items.size(); ++index)
        contentHeight += itemHeight(index) + kContentGap;
    if (m_canLoadMore && !m_items.isEmpty())
        contentHeight += 30.0f;
    m_minScroll = qMin(0.0f,
        height - kContentHeaderHeight - contentHeight);
    m_scroll = qBound(m_minScroll, m_scroll, 0.0f);
    m_backHitBox = QRectF(0.0f, 0.0f, 82.0f, kContentHeaderHeight);
    if (m_headerAction.isEmpty()) {
        m_headerActionHitBox = QRectF();
        m_secondaryHeaderActionHitBox = QRectF();
    } else if (m_secondaryHeaderAction.isEmpty()) {
        m_headerActionHitBox = QRectF(width - 94.0f, 9.0f, 82.0f, 36.0f);
        m_secondaryHeaderActionHitBox = QRectF();
    } else {
        m_headerActionHitBox = QRectF(width - 142.0f, 9.0f, 62.0f, 36.0f);
        m_secondaryHeaderActionHitBox =
            QRectF(width - 72.0f, 9.0f, 62.0f, 36.0f);
    }
}

float ContentScreen::itemHeight(int index) const
{
    if (index < 0 || index >= m_items.size())
        return kContentItemHeight;
    const QString id = m_items.at(index).id;
    if (id == QString::fromLatin1("comment-reply"))
        return kCommentReplyItemHeight;
    if (id.startsWith(QString::fromLatin1("comment-"))) {
        const ContentItemCompat &item = m_items.at(index);
        return kCommentItemHeight + (item.previewText.trimmed().isEmpty()
            ? 0.0f : kCommentPreviewExtra);
    }
    return kContentItemHeight;
}

QRectF ContentScreen::itemFrame(int index) const
{
    float itemY = kContentHeaderHeight + 10.0f +
        m_scroll + m_pullDistance;
    int previous;
    for (previous = 0; previous < index; ++previous)
        itemY += itemHeight(previous) + kContentGap;
    return QRectF(
        10.0f,
        itemY,
        m_width - 20.0f,
        itemHeight(index));
}

void ContentScreen::draw(NVGcontext *context, float width, float height)
{
    updateLayout(width, height);
    NVGpaint background = nvgLinearGradient(
        context, 0.0f, 0.0f, width, height,
        nvgRGBA(31, 31, 40, 255), nvgRGBA(20, 21, 29, 255));
    nvgBeginPath(context);
    nvgRect(context, 0.0f, 0.0f, width, height);
    nvgFillPaint(context, background);
    nvgFill(context);

    nvgBeginPath(context);
    nvgRect(context, 0.0f, 0.0f, width, kContentHeaderHeight);
    nvgFillColor(context, nvgRGBA(31, 31, 40, 252));
    nvgFill(context);
    drawText(context, QString::fromUtf8("< 返回"), 12.0f, 21.0f, 12.5f,
             nvgRGB(251, 114, 153), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    drawText(context, m_title, 88.0f, 20.0f, 16.0f,
             nvgRGB(248, 248, 250), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    QString metadata = m_subtitle;
    if (!m_status.isEmpty()) {
        if (!metadata.isEmpty())
            metadata += QString::fromLatin1("  /  ");
        metadata += m_status;
    }
    drawText(context, metadata,
             88.0f, 42.0f, 9.0f, nvgRGB(150, 150, 164),
             NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    if (!m_headerAction.isEmpty()) {
        nvgBeginPath(context);
        nvgRoundedRect(context,
                       static_cast<float>(m_headerActionHitBox.x()),
                       static_cast<float>(m_headerActionHitBox.y()),
                       static_cast<float>(m_headerActionHitBox.width()),
                       static_cast<float>(m_headerActionHitBox.height()), 9.0f);
        nvgFillColor(context, nvgRGBA(251, 114, 153, 32));
        nvgFill(context);
        drawText(context, m_headerAction,
                 static_cast<float>(m_headerActionHitBox.center().x()),
                 static_cast<float>(m_headerActionHitBox.center().y()),
                 10.5f, nvgRGB(251, 130, 163),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    if (!m_secondaryHeaderAction.isEmpty()) {
        nvgBeginPath(context);
        nvgRoundedRect(context,
                       static_cast<float>(m_secondaryHeaderActionHitBox.x()),
                       static_cast<float>(m_secondaryHeaderActionHitBox.y()),
                       static_cast<float>(m_secondaryHeaderActionHitBox.width()),
                       static_cast<float>(m_secondaryHeaderActionHitBox.height()),
                       9.0f);
        nvgFillColor(context, nvgRGBA(251, 114, 153, 32));
        nvgFill(context);
        drawText(context, m_secondaryHeaderAction,
                 static_cast<float>(
                     m_secondaryHeaderActionHitBox.center().x()),
                 static_cast<float>(
                     m_secondaryHeaderActionHitBox.center().y()),
                 9.5f, nvgRGB(251, 130, 163),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }

    nvgSave(context);
    nvgScissor(context, 0.0f, kContentHeaderHeight,
               width, height - kContentHeaderHeight);
    if (m_pullDistance > 1.0f) {
        drawText(context,
                 m_pullDistance >= 50.0f
                    ? QString::fromUtf8("松开刷新")
                    : QString::fromUtf8("下拉刷新"),
                 width * 0.5f,
                 kContentHeaderHeight + m_pullDistance * 0.5f,
                 10.5f, nvgRGB(251, 114, 153),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    if (m_items.isEmpty()) {
        drawText(context, m_status, width * 0.5f, height * 0.48f,
                 12.0f, nvgRGB(164, 164, 178),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    int index;
    for (index = 0; index < m_items.size(); ++index) {
        const ContentItemCompat &item = m_items.at(index);
        const QRectF frame = itemFrame(index);
        const float x = static_cast<float>(frame.x());
        const float y = static_cast<float>(frame.y());
        const float w = static_cast<float>(frame.width());
        const float h = static_cast<float>(frame.height());
        const bool isComment =
            item.id.startsWith(QString::fromLatin1("comment-"));

        if (isComment) {
            const bool nested =
                item.id == QString::fromLatin1("comment-reply");
            const float avatarSize = nested ? 24.0f : 30.0f;
            const float avatarX = x + (nested ? 23.0f : 26.0f);
            const float avatarY = y + 20.0f;
            const float left = x + (nested ? 50.0f : 57.0f);
            const float textWidth = w - (left - x) - 13.0f;

            nvgBeginPath(context);
            nvgRoundedRect(context, x, y, w, h, 10.0f);
            nvgFillColor(context, nvgRGBA(38, 39, 49, 242));
            nvgFill(context);
            if (nested) {
                nvgBeginPath(context);
                nvgRoundedRect(context, x + 11.0f, y + 13.0f,
                               2.0f, h - 26.0f, 1.0f);
                nvgFillColor(context, nvgRGBA(251, 114, 153, 180));
                nvgFill(context);
            }

            nvgBeginPath(context);
            nvgCircle(context, avatarX, avatarY, avatarSize * 0.5f);
            nvgFillColor(context, commentAvatarColor(item.subtitle));
            nvgFill(context);
            drawText(context, item.subtitle.trimmed().left(1).toUpper(),
                     avatarX, avatarY + 0.5f,
                     nested ? 9.0f : 10.5f, nvgRGB(250, 250, 253),
                     NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

            QString author = item.subtitle;
            if (author.isEmpty())
                author = QString::fromUtf8("匿名用户");
            if (author.size() > 12)
                author = author.left(11) + QString::fromUtf8("…");
            float authorX = left;
            if (item.pinned) {
                nvgBeginPath(context);
                nvgRoundedRect(context, authorX, y + 9.0f,
                               27.0f, 14.0f, 3.0f);
                nvgFillColor(context, nvgRGBA(251, 114, 153, 48));
                nvgFill(context);
                drawText(context, QString::fromUtf8("置顶"),
                         authorX + 13.5f, y + 16.0f, 7.5f,
                         nvgRGB(251, 143, 174),
                         NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                authorX += 32.0f;
            }
            drawText(context, author, authorX, y + 16.0f,
                     nested ? 10.0f : 10.8f,
                     nvgRGB(244, 190, 208),
                     NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            if (item.level > 0) {
                const float levelX = authorX +
                    qMin(100.0f, author.size() * (nested ? 6.0f : 6.5f)) +
                    6.0f;
                nvgBeginPath(context);
                nvgRoundedRect(context, levelX, y + 9.0f,
                               25.0f, 14.0f, 3.0f);
                nvgFillColor(context, nvgRGBA(255, 173, 112, 50));
                nvgFill(context);
                drawText(context, QString::fromLatin1("LV%1").arg(item.level),
                         levelX + 12.5f, y + 16.0f, 7.0f,
                         nvgRGB(255, 190, 132),
                         NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            }

            drawWrappedText(context, item.title, left, y + 29.0f,
                            textWidth, nested ? 10.2f : 10.8f, 2,
                            nvgRGB(246, 246, 249));
            QString meta = item.description.simplified();
            if (meta.size() > 17)
                meta = meta.left(16) + QString::fromUtf8("…");
            if (!nested)
                meta += meta.isEmpty() ? QString::fromUtf8("回复")
                                       : QString::fromUtf8("  ·  回复");
            drawText(context, meta, left, y + (nested ? 68.0f : 70.0f),
                     8.5f, nvgRGB(155, 155, 170),
                     NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            const QString actions = nested
                ? QString::fromUtf8("赞 %1   ···").arg(item.count)
                : QString::fromUtf8("赞 %1   回复   ···").arg(item.count);
            drawText(context, actions, x + w - 12.0f,
                     y + (nested ? 88.0f : 91.0f), 8.8f,
                     nvgRGB(179, 179, 193),
                     NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);

            if (!nested && !item.previewText.trimmed().isEmpty()) {
                const float previewY = y + 103.0f;
                const float previewHeight = h - 114.0f;
                nvgBeginPath(context);
                nvgRoundedRect(context, left, previewY, textWidth,
                               previewHeight, 7.0f);
                nvgFillColor(context, nvgRGBA(79, 80, 93, 136));
                nvgFill(context);
                const QStringList replies = item.previewText.split(
                    QChar('\n'), QString::SkipEmptyParts);
                int replyIndex;
                for (replyIndex = 0; replyIndex < replies.size() &&
                     replyIndex < 2; ++replyIndex) {
                    drawWrappedText(context, replies.at(replyIndex),
                                    left + 8.0f,
                                    previewY + 7.0f + replyIndex * 13.0f,
                                    textWidth - 16.0f, 8.8f, 1,
                                    nvgRGB(211, 212, 222));
                }
                drawText(context,
                         QString::fromUtf8("共 %1 条回复  >")
                             .arg(item.replyCount),
                         left + 8.0f, previewY + previewHeight - 9.0f,
                         8.8f, nvgRGB(251, 143, 174),
                         NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            }
            continue;
        }

        nvgBeginPath(context);
        nvgRoundedRect(context, x, y, w, h, 11.0f);
        nvgFillColor(context, nvgRGBA(43, 43, 53, 246));
        nvgFill(context);

        const int imageHandle = index < m_itemImages.size()
            ? m_itemImages.at(index) : -1;
        const bool hasImage = imageHandle > 0;
        if (hasImage) {
            const float imageX = x + 9.0f;
            const float imageY = y + 9.0f;
            const float imageWidth = 82.0f;
            const float imageHeight = 68.0f;
            NVGpaint imagePaint = nvgImagePattern(
                context, imageX, imageY,
                imageWidth, imageHeight, 0.0f, imageHandle, 1.0f);
            nvgBeginPath(context);
            nvgRoundedRect(context, imageX, imageY,
                           imageWidth, imageHeight, 7.0f);
            nvgFillPaint(context, imagePaint);
            nvgFill(context);
        }

        QString badge;
        if (item.kind == VideoContentItem)
            badge = QString::fromLatin1("VIDEO");
        else if (item.kind == UserContentItem)
            badge = QString::fromLatin1("UP");
        else if (item.kind == FolderContentItem)
            badge = QString::fromLatin1("FOLDER");
        else
            badge = QString::fromLatin1("TEXT");
        const float textStart = hasImage ? x + 101.0f : x + 10.0f;
        nvgBeginPath(context);
        nvgRoundedRect(context, textStart, y + 10.0f, 47.0f, 18.0f, 5.0f);
        nvgFillColor(context, nvgRGBA(251, 114, 153, 28));
        nvgFill(context);
        drawText(context, badge, textStart + 23.5f, y + 19.0f, 7.6f,
                 nvgRGB(251, 135, 166),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        drawWrappedText(context, item.title, textStart + 56.0f, y + 9.0f,
                        w - (textStart - x) - 74.0f,
                        12.0f, 2, nvgRGB(246, 246, 249));
        const QString detail = item.subtitle.isEmpty()
            ? item.description : item.subtitle;
        drawWrappedText(context, detail,
                        hasImage ? textStart : x + 12.0f,
                        y + 57.0f,
                        hasImage ? w - (textStart - x) - 20.0f : w - 32.0f,
                        9.4f, 1, nvgRGB(158, 158, 173));
        if (item.kind != TextContentItem)
            drawText(context, QString::fromLatin1(">"), x + w - 12.0f,
                     y + 43.0f, 15.0f, nvgRGB(130, 130, 144),
                     NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    if (m_canLoadMore && !m_items.isEmpty()) {
        float hintY = kContentHeaderHeight + 10.0f +
            m_scroll + m_pullDistance;
        for (index = 0; index < m_items.size(); ++index)
            hintY += itemHeight(index) + kContentGap;
        drawText(context, QString::fromUtf8("继续上滑加载更多"),
                 width * 0.5f, hintY + 7.0f, 10.0f,
                 nvgRGB(158, 158, 173),
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    nvgRestore(context);
}

void ContentScreen::pointerPress(const QPoint &position)
{
    m_dragging = true;
    m_pressPosition = position;
    m_lastPosition = position;
}

void ContentScreen::pointerMove(const QPoint &position)
{
    if (!m_dragging)
        return;
    const float delta = static_cast<float>(position.y() - m_lastPosition.y());
    if (m_scroll >= 0.0f && (m_pullDistance > 0.0f || delta > 0.0f))
        m_pullDistance = qBound(0.0f, m_pullDistance + delta, 72.0f);
    else
        m_scroll = qBound(m_minScroll, m_scroll + delta, 0.0f);
    if (m_canLoadMore && delta < -1.0f &&
        m_scroll <= m_minScroll + 0.5f) {
        m_loadMoreArmed = true;
    } else if (m_scroll > m_minScroll + 8.0f) {
        m_loadMoreArmed = false;
    }
    m_lastPosition = position;
}

int ContentScreen::pointerRelease(const QPoint &position)
{
    if (!m_dragging)
        return NoActionValue;
    m_dragging = false;
    if (m_pullDistance >= 50.0f) {
        m_pullDistance = 0.0f;
        m_scroll = 0.0f;
        return RefreshActionValue;
    }
    if (m_pullDistance > 0.0f) {
        m_pullDistance = 0.0f;
        return NoActionValue;
    }
    if (m_canLoadMore && m_loadMoreArmed &&
        (position - m_pressPosition).manhattanLength() >= 18) {
        m_loadMoreArmed = false;
        return LoadMoreActionValue;
    }
    if ((position - m_pressPosition).manhattanLength() >= 18)
        return NoActionValue;
    if (m_backHitBox.contains(QPointF(position)))
        return BackActionValue;
    if (m_headerActionHitBox.contains(QPointF(position)))
        return HeaderActionValue;
    if (m_secondaryHeaderActionHitBox.contains(QPointF(position)))
        return SecondaryHeaderActionValue;
    int index;
    for (index = 0; index < m_items.size(); ++index) {
        if (itemFrame(index).contains(QPointF(position)))
            return index;
    }
    return NoActionValue;
}

} // namespace wiliwili
