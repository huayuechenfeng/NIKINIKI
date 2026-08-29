#ifndef WILIWILI_SYMBIAN_YUV420_FRAME_H
#define WILIWILI_SYMBIAN_YUV420_FRAME_H

#include <QtCore/QByteArray>
#include <QtCore/QtGlobal>

namespace wiliwili {

// Tightly packed planar 4:2:0 frame shared between the decoder thread and the
// existing main GLES2 context. QByteArray keeps hand-off copies implicit while
// avoiding any dependency on libavcodec-owned buffers after av_frame_unref().
struct Yuv420Frame
{
    Yuv420Frame()
        : width(0), height(0), fullRange(false), pts(0), serial(0)
    {
    }

    bool isValid() const
    {
        const int chromaWidth = (width + 1) / 2;
        const int chromaHeight = (height + 1) / 2;
        return width > 0 && height > 0 &&
            yPlane.size() == width * height &&
            uPlane.size() == chromaWidth * chromaHeight &&
            vPlane.size() == chromaWidth * chromaHeight;
    }

    QByteArray yPlane;
    QByteArray uPlane;
    QByteArray vPlane;
    int width;
    int height;
    bool fullRange;
    qint64 pts;
    int serial;
};

} // namespace wiliwili

#endif
