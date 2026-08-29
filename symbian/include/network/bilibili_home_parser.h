#ifndef WILIWILI_SYMBIAN_BILIBILI_HOME_PARSER_H
#define WILIWILI_SYMBIAN_BILIBILI_HOME_PARSER_H

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QVector>

#include "model/home_card.h"

namespace wiliwili {

class BilibiliHomeParser
{
public:
    static bool parseHotsAll(
        const QByteArray &body,
        QVector<RecommendVideoResultCompat> *cards,
        int *apiCode,
        QString *errorText);
    static bool parseRecommend(
        const QByteArray &body,
        QVector<RecommendVideoResultCompat> *cards,
        int *apiCode,
        QString *errorText);
    static bool parseBangumi(
        const QByteArray &body,
        QVector<RecommendVideoResultCompat> *cards,
        int *apiCode,
        QString *errorText);
    static bool parseLive(
        const QByteArray &body,
        QVector<RecommendVideoResultCompat> *cards,
        int *apiCode,
        QString *errorText);
};

} // namespace wiliwili

#endif
