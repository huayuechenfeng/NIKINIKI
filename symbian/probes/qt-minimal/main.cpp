#include <QtGui/QApplication>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    QWidget window;
    window.setWindowTitle(QString::fromLatin1("wiliwili Symbian probe"));

    QLabel *status = new QLabel(
        QString::fromUtf8("Qt for Symbian is running.\nTouch the button to verify input."),
        &window);
    status->setAlignment(Qt::AlignCenter);
    status->setWordWrap(true);

    QPushButton *button = new QPushButton(QString::fromUtf8("Input OK"), &window);
    QObject::connect(button, SIGNAL(clicked()),
                     status, SLOT(clear()));

    QVBoxLayout *layout = new QVBoxLayout(&window);
    layout->addStretch();
    layout->addWidget(status);
    layout->addWidget(button);
    layout->addStretch();

    window.showMaximized();
    return application.exec();
}

