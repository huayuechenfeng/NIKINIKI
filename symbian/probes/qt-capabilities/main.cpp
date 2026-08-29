#include <QtCore/QCoreApplication>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtNetwork/QNetworkAccessManager>
#include <QtOpenGL/QGLWidget>

#include <qmediaplayer.h>
#include <GLES2/gl2.h>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QNetworkAccessManager network;
    QDeclarativeEngine declarative;
    QMediaPlayer player;

    // Compile against the GLES2 types used by the prospective nanovg backend.
    const GLuint texture = 0;
    return network.networkAccessible() == QNetworkAccessManager::UnknownAccessibility
               && declarative.rootContext() == 0
               && player.state() == QMediaPlayer::StoppedState
               && texture != 0;
}
