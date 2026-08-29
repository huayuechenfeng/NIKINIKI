#ifndef WILIWILI_SYMBIAN_BILIBILI_WBI_H
#define WILIWILI_SYMBIAN_BILIBILI_WBI_H

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace wiliwili {

class BilibiliWbi
{
public:
    static bool requiresSigning(const QString &endpoint);
    static bool parseMixinKey(
        const QByteArray &body, QString *mixinKey, QString *errorText);
    static QString signUrl(
        const QString &endpoint, const QString &mixinKey);
};

} // namespace wiliwili

#endif
