#ifndef WILIWILI_SYMBIAN_GLES2_COMPAT_H
#define WILIWILI_SYMBIAN_GLES2_COMPAT_H

#include <QtCore/QtGlobal>
#include <GLES2/gl2.h>

#if defined(Q_OS_SYMBIAN)
// Belle's GLES2 header exposes the corresponding APIs as char* but omits GLchar.
typedef char GLchar;
#endif

#endif
