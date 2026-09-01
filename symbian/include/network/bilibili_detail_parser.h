#ifndef WILIWILI_SYMBIAN_BILIBILI_DETAIL_PARSER_H
#define WILIWILI_SYMBIAN_BILIBILI_DETAIL_PARSER_H

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QVector>

#include "model/video_detail.h"

namespace wiliwili {

class BilibiliDetailParser
{
public:
    static bool parseVideoDetail(
        const QByteArray &body,
        VideoDetailCompat *detail,
        int *apiCode,
        QString *errorText,
        QVector<RecommendVideoResultCompat> *related = 0);
};

} // namespace wiliwili

#endif
