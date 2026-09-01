#include <QtGui/QApplication>

#include "anna_probe_ui.h"

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QString::fromLatin1("NIKI A0 Base"));

    nikiniki_anna_probe::ReportWindow window(
        QString::fromLatin1("NIKI A0 Base"));
    window.setReport(
        QString::fromLatin1("A0 BASE QT PASS"),
        nikiniki_anna_probe::baseLines(),
        true);
    window.showMaximized();
    return application.exec();
}
