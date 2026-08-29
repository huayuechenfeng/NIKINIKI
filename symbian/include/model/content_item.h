#ifndef WILIWILI_SYMBIAN_CONTENT_ITEM_H
#define WILIWILI_SYMBIAN_CONTENT_ITEM_H

#include <QtCore/QString>
#include <QtCore/QtGlobal>

namespace wiliwili {

enum ContentItemKindCompat {
    TextContentItem = 0,
    VideoContentItem,
    UserContentItem,
    FolderContentItem
};

struct ContentItemCompat
{
    ContentItemCompat()
        : kind(TextContentItem), numericId(0), secondaryId(0), count(0),
          replyCount(0), level(0), timestamp(0), pinned(false)
    {
    }

    ContentItemKindCompat kind;
    QString id;
    quint64 numericId;
    quint64 secondaryId;
    int count;
    int replyCount;
    int level;
    quint64 timestamp;
    bool pinned;
    QString title;
    QString subtitle;
    QString description;
    QString picture;
    // `previewText` keeps the first one or two nested replies for comment
    // cards.  `mediaTitle`/`mediaPicture`/`badge` let the compact Symbian
    // section screens retain the author, body and attachment independently.
    QString previewText;
    QString mediaTitle;
    QString mediaPicture;
    QString badge;
};

} // namespace wiliwili

#endif
