#include <string>

#include <QtGui/QApplication>
#include <QtGui/QLabel>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

#include "network/bilibili_endpoints.h"

static bool verifyEndpointBoundary()
{
    return nikiniki::BilibiliEndpoint::Recommend ==
               "//api.bilibili.com/x/web-interface/wbi/index/top/feed/rcmd" &&
           nikiniki::BilibiliEndpoint::Detail ==
               "//api.bilibili.com/x/web-interface/view" &&
           nikiniki::BilibiliEndpoint::HotsAll ==
               "//api.bilibili.com/x/web-interface/popular";
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    QWidget window;
    window.setWindowTitle(QString::fromLatin1("NIKINIKI endpoint probe"));

    const bool passed = verifyEndpointBoundary();
    QLabel *headline = new QLabel(
        passed ? QString::fromLatin1("PASS: focused endpoint list is self-contained")
               : QString::fromLatin1("FAIL: endpoint boundary changed"),
        &window);
    headline->setAlignment(Qt::AlignCenter);
    headline->setWordWrap(true);

    QLabel *endpoint = new QLabel(
        QString::fromStdString(nikiniki::BilibiliEndpoint::Recommend), &window);
    endpoint->setAlignment(Qt::AlignCenter);
    endpoint->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout(&window);
    layout->addStretch();
    layout->addWidget(headline);
    layout->addWidget(endpoint);
    layout->addStretch();

    window.showMaximized();
    return application.exec();
}
