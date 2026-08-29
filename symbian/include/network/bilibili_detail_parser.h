#ifndef WILIWILI_SYMBIAN_BILIBILI_DETAIL_PARSER_H
#define WILIWILI_SYMBIAN_BILIBILI_DETAIL_PARSER_H

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include "model/video_detail.h"

namespace wiliwili {

class BilibiliDetailParser
{
public:
    static bool parseVideoDetail(
        const QByteArray &body,
        VideoDetailCompat *detail,
        int *apiCode,
        QString *errorText);
};

} // namespace wiliwili

#endif
