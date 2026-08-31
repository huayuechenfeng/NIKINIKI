#include "app/wiliwili_widget.h"

#include <string>

#include <QtCore/QResource>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QTimerEvent>
#include <QtCore/QUrl>
#include <QtGui/QApplication>
#include <QtGui/QCloseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QInputDialog>
#include <QtGui/QLineEdit>

#include "model/home_card.h"
#include "model/login_session.h"
#include "network/bilibili_detail_parser.h"
#include "network/bilibili_content_parser.h"
#include "network/bilibili_home_parser.h"
#include "network/bilibili_login_parser.h"
#include "network/bilibili_playback_parser.h"
#include "network/bilibili_section_parser.h"
#include "network/bilibili_wbi.h"
#include "platform/gles2_compat.h"
#include "ui/video_player_widget.h"
#include "nanovg_gl.h"
#include "network/bilibili_endpoints.h"

#ifdef Q_OS_SYMBIAN
#include <aknappui.h>
#include <apgtask.h>
#include <eikbtgpc.h>
#include <eikenv.h>
#include <eikspane.h>
#endif

namespace wiliwili {

static GLuint compileYuvShader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    if (!shader)
        return 0;
    const GLchar *shaderSource =
        reinterpret_cast<const GLchar *>(source);
    glShaderSource(shader, 1, &shaderSource, 0);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE)
        return shader;
    GLchar log[512];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof(log) - 1, &length, log);
    log[length >= 0 && length < static_cast<GLsizei>(sizeof(log))
        ? length : static_cast<GLsizei>(sizeof(log) - 1)] = '\0';
    qDebug() << "WW:GLES_YUV_SHADER_ERROR" << type
             << QByteArray(reinterpret_cast<const char *>(log));
    glDeleteShader(shader);
    return 0;
}

#ifdef Q_OS_SYMBIAN
static bool platformWindowGroupHasFocus()
{
    CEikonEnv *environment = CEikonEnv::Static();
    return environment &&
        environment->WsSession().GetFocusWindowGroup() ==
            environment->RootWin().Identifier();
}

static void requestPlatformForeground()
{
    CEikonEnv *environment = CEikonEnv::Static();
    if (!environment)
        return;
    TApaTask task(environment->WsSession());
    task.SetWgId(environment->RootWin().Identifier());
    if (task.Exists()) {
        task.BringToForeground();
        qDebug() << "WW:APPARC_FOREGROUND_REQUEST";
    }
}
#else
static bool platformWindowGroupHasFocus()
{
    return true;
}

static void requestPlatformForeground()
{
}
#endif

static int loadNanoVgFontWithQt(
    NVGcontext *context,
    const char *name,
    const QString &path,
    QByteArray *storage)
{
    if (!context || !name || !storage)
        return -1;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return -1;
    const qint64 size = file.size();
    if (size <= 0 || size > 32 * 1024 * 1024)
        return -1;
    storage->resize(static_cast<int>(size));
    const qint64 bytesRead = file.read(storage->data(), size);
    file.close();
    if (bytesRead != size) {
        storage->clear();
        return -1;
    }
    const int fontId = nvgCreateFontMem(
        context,
        name,
        reinterpret_cast<unsigned char *>(storage->data()),
        storage->size(),
        0);
    if (fontId < 0)
        storage->clear();
    return fontId;
}

static int loadNanoVgFontResource(
    NVGcontext *context,
    const char *name,
    const QString &path)
{
    if (!context || !name)
        return -1;
    QResource resource(path);
    if (!resource.isValid() || resource.size() <= 0 ||
        resource.size() > 32 * 1024 * 1024) {
        return -1;
    }
    // resources.qrc stores this font uncompressed. Its backing bytes remain
    // registered for the process lifetime, which matches NanoVG's freeData=0
    // contract and avoids a 1.8 MB cold-start copy on the Symbian heap.
    return nvgCreateFontMem(
        context,
        name,
        const_cast<unsigned char *>(resource.data()),
        static_cast<int>(resource.size()),
        0);
}

static QStringList installedCjkFontPaths()
{
    QStringList paths;
#ifdef Q_OS_SYMBIAN
    const QString installedFontSubpath = QString::fromLatin1(
        "/resource/apps/wiliwili_symbian/switch_font.ttf");
    const QString executablePath = QApplication::applicationFilePath();
    if (executablePath.size() >= 2 &&
        executablePath.at(1) == QLatin1Char(':')) {
        paths.append(executablePath.left(2) + installedFontSubpath);
    }
    const char installDrives[] = { 'C', 'E', 'F', 'D', 'G' };
    int driveIndex;
    for (driveIndex = 0;
         driveIndex < static_cast<int>(sizeof(installDrives)); ++driveIndex) {
        const QString path = QString::fromLatin1("%1:")
                .arg(QLatin1Char(installDrives[driveIndex])) +
            installedFontSubpath;
        if (!paths.contains(path, Qt::CaseInsensitive))
            paths.append(path);
    }
#endif
    return paths;
}

static void appendFormField(
    QByteArray *form, const QByteArray &name, const QString &value)
{
    if (!form)
        return;
    if (!form->isEmpty())
        form->append('&');
    form->append(name);
    form->append('=');
    form->append(QUrl::toPercentEncoding(value));
}

static int appendUniqueHomeCards(
    QVector<RecommendVideoResultCompat> *target,
    const QVector<RecommendVideoResultCompat> &incoming)
{
    if (!target)
        return 0;
    int added = 0;
    int index;
    for (index = 0; index < incoming.size(); ++index) {
        const RecommendVideoResultCompat &candidate = incoming.at(index);
        bool duplicate = false;
        int existing;
        for (existing = 0; existing < target->size(); ++existing) {
            const RecommendVideoResultCompat &present = target->at(existing);
            if ((!candidate.bvid.isEmpty() && candidate.bvid == present.bvid) ||
                (candidate.bvid.isEmpty() && candidate.id != 0 &&
                 candidate.id == present.id)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            target->append(candidate);
            ++added;
        }
    }
    return added;
}

static int appendUniqueContentItems(
    QVector<ContentItemCompat> *target,
    const QVector<ContentItemCompat> &incoming)
{
    if (!target)
        return 0;
    int added = 0;
    int index;
    for (index = 0; index < incoming.size(); ++index) {
        const ContentItemCompat &candidate = incoming.at(index);
        bool duplicate = false;
        int existing;
        for (existing = 0; existing < target->size(); ++existing) {
            const ContentItemCompat &present = target->at(existing);
            if ((!candidate.id.isEmpty() && candidate.id == present.id) ||
                (candidate.id.isEmpty() && candidate.numericId != 0 &&
                 candidate.numericId == present.numericId)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            target->append(candidate);
            ++added;
        }
    }
    return added;
}

static QString bvidFromReference(const QString &reference)
{
    const int start = reference.indexOf(QString::fromLatin1("BV"));
    if (start < 0)
        return QString();
    int end = start + 2;
    while (end < reference.size()) {
        const QChar character = reference.at(end);
        if (!character.isLetterOrNumber())
            break;
        ++end;
    }
    const QString bvid = reference.mid(start, end - start);
    return bvid.size() >= 4 ? bvid : QString();
}

static QString historyCursorEndpoint(quint64 max, quint64 viewAt)
{
    return QString::fromLatin1(
        "https://api.bilibili.com/x/web-interface/history/cursor"
        "?max=%1&view_at=%2&business=&ps=20")
        .arg(QString::number(max))
        .arg(QString::number(viewAt));
}

WiliwiliWidget::WiliwiliWidget(QWidget *parent)
    : QGLWidget(parent),
      m_context(0),
      m_fontId(-1),
      m_cjkFontId(-1),
      m_cjkFontFile(0),
      m_cjkFontExpectedBytes(0),
      m_cjkFontBytesRead(0),
      m_logoHandle(-1),
      m_cardPlaceholderHandle(-1),
      m_yuvProgram(0),
      m_yuvTextureY(0),
      m_yuvTextureU(0),
      m_yuvTextureV(0),
      m_yuvTextureYAlt(0),
      m_yuvTextureUAlt(0),
      m_yuvTextureVAlt(0),
      m_yuvTextureSet(0),
      m_yuvPositionLocation(-1),
      m_yuvTexCoordLocation(-1),
      m_yuvSamplerYLocation(-1),
      m_yuvSamplerULocation(-1),
      m_yuvSamplerVLocation(-1),
      m_yuvFullRangeLocation(-1),
      m_yuvTextureWidth(0),
      m_yuvTextureHeight(0),
      m_yuvUploadedSerial(-1),
      m_yuvPresentedCount(0),
      m_yuvUploadedCount(0),
      m_yuvUploadMilliseconds(0),
      m_yuvUploadYMilliseconds(0),
      m_yuvUploadUMilliseconds(0),
      m_yuvUploadVMilliseconds(0),
      m_yuvUploadOtherMilliseconds(0),
      m_yuvDrawMilliseconds(0),
      m_yuvSwapMilliseconds(0),
      m_yuvPresentCallMilliseconds(0),
      m_yuvPaintGlMilliseconds(0),
      m_yuvLastPaintGlMilliseconds(0),
      m_yuvPresentCallCount(0),
      m_yuvRendererReady(false),
      m_yuvFirstFrameLogged(false),
      m_startupTimerId(0),
      m_startupPhase(0),
      m_foregroundTimerId(0),
      m_foregroundAttemptCount(0),
      m_uiActionTimerId(0),
      m_pendingUiAction(NoPendingUiAction),
      m_searchFocusTimerId(0),
      m_searchFocusPhase(0),
      m_metricsTimerId(0),
      m_metricsTickCount(0),
      m_softTelemetryTickCount(0),
      m_frameCount(0),
      m_hasPainted(false),
      m_uiResourcesReady(false),
      m_shuttingDown(false),
      m_hasActivated(false),
      m_chromeHidden(false),
#ifdef WILIWILI_ENABLE_APP_LANDSCAPE_WINDOW_PROBE
      m_landscapeWindowProbeActive(false),
#endif
       m_searchEdit(0),
       m_networkStage(FetchingHome),
       m_homeFreshIndex(0),
       m_homeRequestedFreshIndex(0),
       m_homeRequestedFreshType(0),
       m_homeAppend(false),
       m_homeCanLoadMore(false),
       m_homeSessionChanged(false),
       m_thumbnailIndex(0),
       m_thumbnailSuccessCount(0),
       m_contentImageIndex(0),
       m_contentImageGeneration(0),
       m_contentImageLimit(14),
       m_playbackMode(VideoPlayerWidget::UrlStreamingPlayback),
       m_decoderMode(VideoPlayerWidget::AutomaticDecoder),
       m_contentAppend(false),
       m_contentCanLoadMore(false),
       m_contentPage(1),
       m_contentPageSize(0),
       m_historyCursorMax(0),
       m_historyCursorViewAt(0),
       m_searchUsers(false),
       m_selectedVideoIndex(-1),
      m_playbackCid(0),
      m_playbackVideoWidth(0),
      m_playbackVideoHeight(0),
      m_requestedPlaybackQuality(32),
      m_playbackQualitySwitch(false),
      m_playbackIsLive(false),
      m_liveRoomId(0),
      m_currentScreen(TopLevelScreenView),
      m_contentMode(SearchContentMode),
      m_pendingAction(NoPendingAction),
      m_contentSubjectId(0),
      m_commentMode(3),
       m_commentLegacyFallback(false),
       m_messageType(0),
       m_dynamicPage(1),
       m_dynamicAppend(false),
       m_dynamicHasMore(false),
       m_messageCursorId(0),
       m_messageCursorTime(0),
       m_chatBeginTimestamp(0),
       m_messageAppend(false),
       m_messageHasMore(false),
       m_loginPollDelay(0),
      m_loginPollWaiting(false),
      m_videoPlayer(0)
{
    setWindowTitle(QString::fromLatin1("NIKINIKI"));
    setAutoFillBackground(false);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    setAttribute(Qt::WA_QuitOnClose, true);
    qApp->installEventFilter(this);
    // Raster children of QGLWidget have no valid paint engine on Symbian
    // Qt 4.7. Qt::Tool keeps the editor in its own native window while this
    // parent association makes it a transient belonging to the main window,
    // so it cannot outlive or float above the player window group.
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setWindowFlags(
        Qt::Tool | Qt::FramelessWindowHint);
    m_searchEdit->setAttribute(Qt::WA_QuitOnClose, false);
    m_searchEdit->setObjectName(QString::fromLatin1("wiliwiliSearchEdit"));
    m_searchEdit->setPlaceholderText(
        QString::fromUtf8("输入关键词，@开头搜索用户，按回车搜索"));
    m_searchEdit->setStyleSheet(QString::fromLatin1(
        "QLineEdit { background: #fff7ff; color: #281d32; "
        "border: 2px solid #fb7299; border-radius: 9px; "
        "padding: 5px 10px; font-size: 17px; "
        "selection-background-color: #fb7299; }"));
    m_searchEdit->installEventFilter(this);
    m_searchEdit->hide();
    m_homeScreen.setCards(buildHomeFixture());
    m_homeScreen.setNetworkStatus(QString::fromLatin1("BI:WAIT"));
    m_memory = PlatformMetrics::sampleMemory();
    QSettings startupSettings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    m_contentImageLimit = startupSettings.value(
        QString::fromLatin1("ui/content_images"), 14).toInt() > 0
        ? 14 : 0;
    m_playbackMode = qBound(
        static_cast<int>(VideoPlayerWidget::UrlStreamingPlayback),
        startupSettings.value(
            QString::fromLatin1("player/playback_mode"),
            static_cast<int>(
                VideoPlayerWidget::UrlStreamingPlayback)).toInt(),
        static_cast<int>(VideoPlayerWidget::DownloadThenPlayback));
    m_decoderMode = qBound(
        static_cast<int>(VideoPlayerWidget::AutomaticDecoder),
        startupSettings.value(
            QString::fromLatin1("player/decoder_mode"),
            static_cast<int>(
                VideoPlayerWidget::AutomaticDecoder)).toInt(),
        static_cast<int>(VideoPlayerWidget::SoftwareOnlyDecoder));
    m_sectionScreen.setImageLoadingEnabled(m_contentImageLimit > 0);
    m_sectionScreen.setPlaybackPreferences(
        m_playbackMode, m_decoderMode);
    // Native HTTP callbacks finish asynchronously, but their results were
    // consumed only once per second. A 250 ms pump starts the next thumbnail
    // promptly; memory sampling remains throttled to once per second below.
    m_metricsTimerId = startTimer(250);
    m_startupTimerId = startTimer(100);
    scheduleForegroundRestore();
    qDebug() << "WW:CONSTRUCTOR_DONE";
}

WiliwiliWidget::~WiliwiliWidget()
{
    if (qApp)
        qApp->removeEventFilter(this);
    if (m_metricsTimerId)
        killTimer(m_metricsTimerId);
    if (m_startupTimerId)
        killTimer(m_startupTimerId);
    if (m_foregroundTimerId)
        killTimer(m_foregroundTimerId);
    if (m_uiActionTimerId)
        killTimer(m_uiActionTimerId);
    if (m_searchFocusTimerId)
        killTimer(m_searchFocusTimerId);
    if (m_searchEdit) {
        m_searchEdit->removeEventFilter(this);
        delete m_searchEdit;
        m_searchEdit = 0;
    }
    if (m_cjkFontFile) {
        m_cjkFontFile->close();
        delete m_cjkFontFile;
        m_cjkFontFile = 0;
    }
    delete m_videoPlayer;
    m_videoPlayer = 0;

    if (m_context || m_yuvProgram)
        makeCurrent();
    if (m_context) {
        int index;
        for (index = 0; index < m_dynamicImageHandles.size(); ++index)
            nvgDeleteImage(m_context, m_dynamicImageHandles.at(index));
        m_dynamicImageHandles.clear();
        for (index = 0; index < m_contentImageHandles.size(); ++index)
            nvgDeleteImage(m_context, m_contentImageHandles.at(index));
        m_contentImageHandles.clear();
        nvgDeleteGLES2(m_context);
        m_context = 0;
    }
    destroyYuvRenderer();
}

void WiliwiliWidget::initializeGL()
{
    qDebug() << "WW:GL_BEGIN";
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.035f, 0.025f, 0.055f, 1.0f);

    m_yuvRendererReady = initializeYuvRenderer();

    m_context = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!m_context) {
        qDebug() << "WW:GL_CONTEXT_FAILED";
        return;
    }

    qDebug() << "WW:GL_READY";
}

void WiliwiliWidget::resizeGL(int width, int height)
{
    glViewport(0, 0, width, height);
    positionSearchEditor();
    // Native landscape owns an independent top-level player window. Never
    // mutate that window tree from the retained QGLWidget's resize callback;
    // workAreaResized() commits the transition after screen geometry settles.
}

void WiliwiliWidget::paintGL()
{
    const bool playerForeground = playerOwnsForeground();
    QTime paintClock;
    if (playerForeground)
        paintClock.start();
    if (playerForeground)
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    else
        glClearColor(0.035f, 0.025f, 0.055f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    // During native MMF playback the main EGL surface remains alive but does
    // not redraw the browsing UI. This avoids competing with the video and the
    // single transparent player overlay for composition bandwidth.
    if (playerForeground) {
        drawYuvFrame();
        ++m_frameCount;
        const qint64 paintElapsed = paintClock.elapsed();
        m_yuvLastPaintGlMilliseconds = paintElapsed;
        m_yuvPaintGlMilliseconds += paintElapsed;
        return;
    }
    if (!m_hasActivated && QApplication::activeWindow() == this) {
        m_hasActivated = true;
        qDebug() << "WW:ACTIVE_ON_FIRST_FRAME";
    }
    if (!m_context || !m_uiResourcesReady) {
        ++m_frameCount;
        if (!m_hasPainted)
            qDebug() << "WW:FIRST_FRAME";
        m_hasPainted = true;
        return;
    }

    const float screenWidth = static_cast<float>(width());
    const float screenHeight = static_cast<float>(height());
    nvgBeginFrame(m_context, screenWidth, screenHeight, 1.0f);
    if (m_currentScreen == DetailScreenView) {
        m_detailScreen.draw(m_context, screenWidth, screenHeight);
    } else if (m_currentScreen == ContentScreenView) {
        m_contentScreen.draw(m_context, screenWidth, screenHeight);
    } else {
        const NavigationRail::Section section = m_navigation.selected();
        if (section == NavigationRail::HomeSection) {
            m_homeScreen.draw(
                m_context,
                screenWidth,
                screenHeight,
                m_memory,
                m_frameCount);
        } else if (section == NavigationRail::AccountSection) {
            m_loginScreen.draw(m_context, screenWidth, screenHeight);
        } else {
            m_sectionScreen.draw(m_context, screenWidth, screenHeight);
        }
        m_navigation.draw(m_context, screenWidth, screenHeight);
    }
    nvgEndFrame(m_context);
    ++m_frameCount;
    m_hasPainted = true;
}

void WiliwiliWidget::updateGL()
{
    // On this Qt 4.7.4 Symbian build, video-frame updateGL() calls reach the
    // GL paint/swap path directly; QGLWidget::event() only sees occasional
    // window repaint events. Measure the actual updateGL call used by the YUV
    // presenter so the timing includes QGLWidget's internal context and swap
    // work rather than an unrelated paint event.
    const bool measurePresentation = playerOwnsForeground();
    QTime presentClock;
    if (measurePresentation) {
        presentClock.start();
        m_yuvLastPaintGlMilliseconds = 0;
    }
    QGLWidget::updateGL();
    if (measurePresentation) {
        const qint64 presentElapsed = presentClock.elapsed();
        ++m_yuvPresentCallCount;
        m_yuvPresentCallMilliseconds += presentElapsed;
        const qint64 paintElapsed = m_yuvLastPaintGlMilliseconds;
        if (paintElapsed >= 0 && presentElapsed > paintElapsed)
            m_yuvSwapMilliseconds += presentElapsed - paintElapsed;
    }
}

bool WiliwiliWidget::initializeYuvRenderer()
{
    static const char vertexSource[] =
        "attribute vec2 a_position;\n"
        "attribute vec2 a_texCoord;\n"
        "varying vec2 v_texCoord;\n"
        "void main() {\n"
        "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "  v_texCoord = a_texCoord;\n"
        "}\n";
    static const char fragmentSource[] =
        "precision mediump float;\n"
        "varying vec2 v_texCoord;\n"
        "uniform sampler2D u_y;\n"
        "uniform sampler2D u_u;\n"
        "uniform sampler2D u_v;\n"
        "uniform float u_fullRange;\n"
        "void main() {\n"
        "  float y = texture2D(u_y, v_texCoord).r;\n"
        "  float u = texture2D(u_u, v_texCoord).r - 0.5;\n"
        "  float v = texture2D(u_v, v_texCoord).r - 0.5;\n"
        "  vec3 rgb;\n"
        "  if (u_fullRange > 0.5) {\n"
        "    rgb = vec3(y + 1.5748 * v,\n"
        "               y - 0.1873 * u - 0.4681 * v,\n"
        "               y + 1.8556 * u);\n"
        "  } else {\n"
        "    float limitedY = 1.164383 * (y - 0.0627451);\n"
        "    rgb = vec3(limitedY + 1.792741 * v,\n"
        "               limitedY - 0.213249 * u - 0.532909 * v,\n"
        "               limitedY + 2.112402 * u);\n"
        "  }\n"
        "  gl_FragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);\n"
        "}\n";

    const GLuint vertex = compileYuvShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragment = compileYuvShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertex || !fragment) {
        if (vertex)
            glDeleteShader(vertex);
        if (fragment)
            glDeleteShader(fragment);
        qDebug() << "WW:GLES_YUV_UNAVAILABLE" << "compile";
        return false;
    }
    m_yuvProgram = glCreateProgram();
    glAttachShader(m_yuvProgram, vertex);
    glAttachShader(m_yuvProgram, fragment);
    glLinkProgram(m_yuvProgram);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(m_yuvProgram, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLchar log[512];
        GLsizei length = 0;
        glGetProgramInfoLog(
            m_yuvProgram, sizeof(log) - 1, &length, log);
        log[length >= 0 && length < static_cast<GLsizei>(sizeof(log))
            ? length : static_cast<GLsizei>(sizeof(log) - 1)] = '\0';
        qDebug() << "WW:GLES_YUV_SHADER_ERROR" << "link"
                 << QByteArray(reinterpret_cast<const char *>(log));
        destroyYuvRenderer();
        return false;
    }

    m_yuvPositionLocation = glGetAttribLocation(m_yuvProgram, "a_position");
    m_yuvTexCoordLocation = glGetAttribLocation(m_yuvProgram, "a_texCoord");
    m_yuvSamplerYLocation = glGetUniformLocation(m_yuvProgram, "u_y");
    m_yuvSamplerULocation = glGetUniformLocation(m_yuvProgram, "u_u");
    m_yuvSamplerVLocation = glGetUniformLocation(m_yuvProgram, "u_v");
    m_yuvFullRangeLocation =
        glGetUniformLocation(m_yuvProgram, "u_fullRange");
    if (m_yuvPositionLocation < 0 || m_yuvTexCoordLocation < 0 ||
        m_yuvSamplerYLocation < 0 || m_yuvSamplerULocation < 0 ||
        m_yuvSamplerVLocation < 0 || m_yuvFullRangeLocation < 0) {
        qDebug() << "WW:GLES_YUV_UNAVAILABLE" << "locations";
        destroyYuvRenderer();
        return false;
    }

    GLuint textures[6] = { 0, 0, 0, 0, 0, 0 };
    glGenTextures(6, textures);
    m_yuvTextureY = textures[0];
    m_yuvTextureU = textures[1];
    m_yuvTextureV = textures[2];
    m_yuvTextureYAlt = textures[3];
    m_yuvTextureUAlt = textures[4];
    m_yuvTextureVAlt = textures[5];
    int index;
    for (index = 0; index < 6; ++index) {
        glBindTexture(GL_TEXTURE_2D, textures[index]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    if (!m_yuvTextureY || !m_yuvTextureU || !m_yuvTextureV ||
        !m_yuvTextureYAlt || !m_yuvTextureUAlt || !m_yuvTextureVAlt ||
        glGetError() != GL_NO_ERROR) {
        qDebug() << "WW:GLES_YUV_UNAVAILABLE" << "textures";
        destroyYuvRenderer();
        return false;
    }
    m_yuvTextureSet = 0;
    qDebug() << "WW:GLES_YUV_READY" << "3PLANE_LUMINANCE_BT709_PINGPONG";
    return true;
}

void WiliwiliWidget::destroyYuvRenderer()
{
    GLuint textures[6] = {
        m_yuvTextureY, m_yuvTextureU, m_yuvTextureV,
        m_yuvTextureYAlt, m_yuvTextureUAlt, m_yuvTextureVAlt
    };
    if (textures[0] || textures[1] || textures[2] ||
        textures[3] || textures[4] || textures[5])
        glDeleteTextures(6, textures);
    if (m_yuvProgram)
        glDeleteProgram(m_yuvProgram);
    m_yuvProgram = 0;
    m_yuvTextureY = 0;
    m_yuvTextureU = 0;
    m_yuvTextureV = 0;
    m_yuvTextureYAlt = 0;
    m_yuvTextureUAlt = 0;
    m_yuvTextureVAlt = 0;
    m_yuvTextureSet = 0;
    m_yuvTextureWidth = 0;
    m_yuvTextureHeight = 0;
    m_yuvUploadedSerial = -1;
    m_yuvRendererReady = false;
    m_yuvFrame = Yuv420Frame();
}

void WiliwiliWidget::drawYuvFrame()
{
    if (!m_yuvRendererReady || !m_yuvFrame.isValid())
        return;

    const int chromaWidth = (m_yuvFrame.width + 1) / 2;
    const int chromaHeight = (m_yuvFrame.height + 1) / 2;
    if (m_yuvUploadedSerial != m_yuvFrame.serial) {
        // uploadMs covers this complete transaction: pixel-store setup,
        // three plane uploads, texture binds/activations and glGetError.
        // The per-plane clocks below deliberately begin at each active/bind
        // pair, so uploadOtherMs exposes the remaining driver/setup cost.
        QTime uploadClock;
        uploadClock.start();
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        // Texture names are created once in initializeYuvRenderer(). Only a
        // size change uses glTexImage2D; steady-state frames update the
        // inactive texture set with glTexSubImage2D. Keeping two complete
        // sets avoids a synchronous driver stall when the previous draw is
        // still reading from its textures.
        const bool allocate =
            m_yuvTextureWidth != m_yuvFrame.width ||
            m_yuvTextureHeight != m_yuvFrame.height;
        const GLuint yTextures[2] = {
            m_yuvTextureY, m_yuvTextureYAlt
        };
        const GLuint uTextures[2] = {
            m_yuvTextureU, m_yuvTextureUAlt
        };
        const GLuint vTextures[2] = {
            m_yuvTextureV, m_yuvTextureVAlt
        };
        // A resize initializes both sets. Otherwise alternate the destination
        // from the set used by the last successfully uploaded frame.
        const int textureSet = allocate
            ? 0
            : (m_yuvUploadedSerial < 0 ? 0 : 1 - m_yuvTextureSet);
        const int spareSet = 1 - textureSet;
        if (allocate) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, yTextures[spareSet]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                m_yuvFrame.width, m_yuvFrame.height, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE,
                m_yuvFrame.yPlane.constData());
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, uTextures[spareSet]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                chromaWidth, chromaHeight, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE,
                m_yuvFrame.uPlane.constData());
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, vTextures[spareSet]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                chromaWidth, chromaHeight, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE,
                m_yuvFrame.vPlane.constData());
        }
        QTime yUploadClock;
        yUploadClock.start();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, yTextures[textureSet]);
        if (allocate) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                m_yuvFrame.width, m_yuvFrame.height, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE,
                m_yuvFrame.yPlane.constData());
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                m_yuvFrame.width, m_yuvFrame.height,
                GL_LUMINANCE, GL_UNSIGNED_BYTE,
                m_yuvFrame.yPlane.constData());
        }
        const qint64 yUploadElapsed = yUploadClock.elapsed();
        QTime uUploadClock;
        uUploadClock.start();
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, uTextures[textureSet]);
        if (allocate) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                chromaWidth, chromaHeight, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE,
                m_yuvFrame.uPlane.constData());
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                chromaWidth, chromaHeight,
                GL_LUMINANCE, GL_UNSIGNED_BYTE,
                m_yuvFrame.uPlane.constData());
        }
        const qint64 uUploadElapsed = uUploadClock.elapsed();
        QTime vUploadClock;
        vUploadClock.start();
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, vTextures[textureSet]);
        if (allocate) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
                chromaWidth, chromaHeight, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE,
                m_yuvFrame.vPlane.constData());
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                chromaWidth, chromaHeight,
                GL_LUMINANCE, GL_UNSIGNED_BYTE,
                m_yuvFrame.vPlane.constData());
        }
        const qint64 vUploadElapsed = vUploadClock.elapsed();
        const GLenum uploadError = glGetError();
        const qint64 uploadElapsed = uploadClock.elapsed();
        m_yuvUploadMilliseconds += uploadElapsed;
        m_yuvUploadYMilliseconds += yUploadElapsed;
        m_yuvUploadUMilliseconds += uUploadElapsed;
        m_yuvUploadVMilliseconds += vUploadElapsed;
        const qint64 planeElapsed = yUploadElapsed + uUploadElapsed +
            vUploadElapsed;
        if (uploadElapsed > planeElapsed)
            m_yuvUploadOtherMilliseconds += uploadElapsed - planeElapsed;
        if (uploadError != GL_NO_ERROR) {
            qDebug() << "WW:GLES_YUV_UPLOAD_ERROR" << uploadError
                     << m_yuvFrame.width << m_yuvFrame.height;
            return;
        }
        m_yuvTextureSet = textureSet;
        m_yuvTextureWidth = m_yuvFrame.width;
        m_yuvTextureHeight = m_yuvFrame.height;
        m_yuvUploadedSerial = m_yuvFrame.serial;
        ++m_yuvUploadedCount;
        if (m_yuvUploadedCount == 1) {
            qDebug() << "WW:GLES_YUV_UPLOAD_FIRST"
                     << m_yuvUploadedCount << m_yuvFrame.serial
                     << m_yuvFrame.pts;
        }
    }

    const float logicalWidth = static_cast<float>(width());
    const float logicalHeight = static_cast<float>(height());
    const float sourceAspect = static_cast<float>(m_yuvFrame.width) /
        static_cast<float>(m_yuvFrame.height);
    const float targetAspect = logicalWidth / logicalHeight;
    float fittedWidth = logicalWidth;
    float fittedHeight = logicalHeight;
    if (sourceAspect > targetAspect)
        fittedHeight = logicalWidth / sourceAspect;
    else
        fittedWidth = logicalHeight * sourceAspect;
    const float xScale = fittedWidth / logicalWidth;
    const float yScale = fittedHeight / logicalHeight;
    const GLfloat positions[] = {
        -xScale, -yScale, xScale, -yScale,
        -xScale, yScale, xScale, yScale
    };
    static const GLfloat textureCoords[] = {
        0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    QTime drawClock;
    drawClock.start();
    const GLuint activeY = m_yuvTextureSet == 0
        ? m_yuvTextureY : m_yuvTextureYAlt;
    const GLuint activeU = m_yuvTextureSet == 0
        ? m_yuvTextureU : m_yuvTextureUAlt;
    const GLuint activeV = m_yuvTextureSet == 0
        ? m_yuvTextureV : m_yuvTextureVAlt;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, activeY);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, activeU);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, activeV);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glUseProgram(m_yuvProgram);
    glUniform1i(m_yuvSamplerYLocation, 0);
    glUniform1i(m_yuvSamplerULocation, 1);
    glUniform1i(m_yuvSamplerVLocation, 2);
    glUniform1f(m_yuvFullRangeLocation,
                m_yuvFrame.fullRange ? 1.0f : 0.0f);
    glEnableVertexAttribArray(m_yuvPositionLocation);
    glEnableVertexAttribArray(m_yuvTexCoordLocation);
    glVertexAttribPointer(m_yuvPositionLocation, 2, GL_FLOAT, GL_FALSE,
                          0, positions);
    glVertexAttribPointer(m_yuvTexCoordLocation, 2, GL_FLOAT, GL_FALSE,
                          0, textureCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(m_yuvPositionLocation);
    glDisableVertexAttribArray(m_yuvTexCoordLocation);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(0);
    m_yuvDrawMilliseconds += drawClock.elapsed();
}

void WiliwiliWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_foregroundTimerId) {
        if (playerOwnsForeground()) {
            killTimer(m_foregroundTimerId);
            m_foregroundTimerId = 0;
            m_foregroundAttemptCount = 0;
            qDebug() << "WW:MAIN_FOREGROUND_TIMER_SUPPRESSED";
            event->accept();
            return;
        }
        if (m_searchEdit && m_searchEdit->isVisible()) {
            killTimer(m_foregroundTimerId);
            m_foregroundTimerId = 0;
            qDebug() << "WW:FOREGROUND_PAUSED_FOR_SEARCH";
            event->accept();
            return;
        }
        // AppArc can create the process without making the Qt OpenGL window
        // active. Waiting for the first paint in that state deadlocks: the
        // hidden window never paints, so it is never raised. Perform the Qt
        // activation from a timer (the event loop is now alive), then hide
        // Avkon chrome only after activation and a real frame.
        if (!isVisible())
            showMaximized();

        const bool activeNow = platformWindowGroupHasFocus() &&
            (QApplication::activeWindow() == this || m_hasActivated);
        if (!activeNow) {
            if (m_foregroundAttemptCount < 12) {
                ++m_foregroundAttemptCount;
                showMaximized();
                raise();
                activateWindow();
                QApplication::setActiveWindow(this);
                setFocus(Qt::ActiveWindowFocusReason);
                if (m_foregroundAttemptCount == 4)
                    requestPlatformForeground();
                if (m_foregroundAttemptCount == 1 ||
                    m_foregroundAttemptCount == 6) {
                    qDebug() << "WW:FOREGROUND_REQUEST"
                             << m_foregroundAttemptCount;
                }
            }
            event->accept();
            return;
        }

        if (!m_hasPainted) {
            updateGL();
            event->accept();
            return;
        }

        if (!m_chromeHidden)
            bringApplicationToForeground();
        killTimer(m_foregroundTimerId);
        m_foregroundTimerId = 0;
        qDebug() << "WW:FOREGROUND_READY";
        event->accept();
        return;
    }

    if (event->timerId() == m_uiActionTimerId) {
        killTimer(m_uiActionTimerId);
        m_uiActionTimerId = 0;
        const PendingUiAction action = m_pendingUiAction;
        m_pendingUiAction = NoPendingUiAction;
        if (action == SearchPendingUiAction)
            promptSearch();
        else if (action == CommentPendingUiAction)
            promptComment();
        else if (action == PrivateMessagePendingUiAction)
            promptPrivateMessage();
        event->accept();
        return;
    }

    if (event->timerId() == m_searchFocusTimerId) {
        killTimer(m_searchFocusTimerId);
        m_searchFocusTimerId = 0;
        const bool searchBelongsHere =
            m_searchEdit && m_searchEdit->isVisible() &&
            m_currentScreen == TopLevelScreenView &&
            m_navigation.selected() == NavigationRail::HomeSection &&
            !playerOwnsForeground();
        if (!searchBelongsHere) {
            m_searchFocusPhase = 0;
            if (m_searchEdit && m_searchEdit->isVisible())
                hideSearchEditor();
            event->accept();
            return;
        }

        m_searchEdit->raise();
        m_searchEdit->activateWindow();
        QApplication::setActiveWindow(m_searchEdit);
        m_searchEdit->setFocus(Qt::ActiveWindowFocusReason);
        if (m_searchFocusPhase == 0) {
            m_searchFocusPhase = 1;
            m_searchFocusTimerId = startTimer(140);
            qDebug() << "WW:SEARCH_EDITOR_ACTIVATED"
                     << m_searchEdit->hasFocus();
        } else {
            m_searchFocusPhase = 0;
            m_searchEdit->setEditFocus(true);
            m_searchEdit->selectAll();
            QEvent requestPanel(QEvent::RequestSoftwareInputPanel);
            QApplication::sendEvent(m_searchEdit, &requestPanel);
            qDebug() << "WW:SEARCH_SIP_REQUESTED"
                     << m_searchEdit->hasFocus();
        }
        event->accept();
        return;
    }

    if (event->timerId() == m_startupTimerId) {
        if (!m_hasPainted || !m_context) {
            event->accept();
            return;
        }

        if (m_startupPhase == 0) {
            loadUiResources();
            m_startupPhase = 1;
            qDebug() << "WW:UI_READY" << m_fontId;
            updateGL();
            qDebug() << "WW:STARTUP_UI_PRESENTED" << m_fontId;
            event->accept();
            return;
        }

        if (m_startupPhase == 1) {
            // The bundled subset has already produced a real UI frame. Start
            // normal work now; the complete 8 MB font is an optional fallback
            // and must not hold the network or event loop behind cold storage.
            beginCjkFallbackLoad();
            m_startupPhase = 2;
#if defined(WILIWILI_ENABLE_APP_LANDSCAPE_WINDOW_PROBE) || \
    defined(WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE)
        // Keep the layer-2 experiment limited to the retained QGL main
        // window and its controlled native probe windows. No network callback
        // may introduce event-loop/window timing into either diagnosis build.
        m_networkStage = NetworkComplete;
#ifdef WILIWILI_ENABLE_APP_LANDSCAPE_WINDOW_PROBE
        qDebug() << "WW:APP_LANDSCAPE_PROBE_NETWORK_SKIPPED";
#else
        qDebug() << "WW:DIRECT_PROBE_NETWORK_SKIPPED";
#endif
#else
            qDebug() << "WW:NETWORK_START";
            startBilibiliFeed();
#endif
            if (!playerOwnsForeground())
                updateGL();
            event->accept();
            return;
        }

        // Do not add storage I/O to MMF/FFmpeg startup or active playback.
        // The open file and partial buffer remain valid and resume after the
        // persistent player returns to the main window.
        if (playerOwnsForeground() || !pumpCjkFallbackLoad()) {
            event->accept();
            return;
        }

        killTimer(m_startupTimerId);
        m_startupTimerId = 0;
        qDebug() << "WW:CJK_READY" << m_cjkFontId;
        if (!playerOwnsForeground())
            updateGL();
        event->accept();
        return;
    }

    if (event->timerId() == m_metricsTimerId) {
        if (++m_metricsTickCount >= 4) {
            m_metricsTickCount = 0;
            m_memory = PlatformMetrics::sampleMemory();
        }

        if (playerOwnsForeground() && m_videoPlayer) {
            if (++m_softTelemetryTickCount >= 20) {
                m_softTelemetryTickCount = 0;
                const QString telemetry =
                    m_videoPlayer->softPlaybackTelemetry(
                        m_yuvPresentedCount, m_yuvUploadedCount,
                        m_yuvUploadMilliseconds,
                        m_yuvFrame.isValid() ? m_yuvFrame.pts : -1);
                if (!telemetry.isEmpty()) {
                    // The decoder string is emitted once per existing 5 s
                    // telemetry tick. Keep all GLES measurements cumulative
                    // and append them here so no per-frame logging is added.
                    const QString glesTelemetry = QString::fromLatin1(
                        " yUploadMs=%1 uUploadMs=%2 vUploadMs=%3 "
                        "uploadOtherMs=%4 drawMs=%5 swapMs=%6 "
                        "makeCurrentMs=-1 presentCallMs=%7 paintGLMs=%8 "
                        "presentCalls=%9")
                        .arg(m_yuvUploadYMilliseconds)
                        .arg(m_yuvUploadUMilliseconds)
                        .arg(m_yuvUploadVMilliseconds)
                        .arg(m_yuvUploadOtherMilliseconds)
                        .arg(m_yuvDrawMilliseconds)
                        .arg(m_yuvSwapMilliseconds)
                        .arg(m_yuvPresentCallMilliseconds)
                        .arg(m_yuvPaintGlMilliseconds)
                        .arg(static_cast<qulonglong>(
                            m_yuvPresentCallCount));
                    qDebug() << telemetry + glesTelemetry;
                }
            }
        } else {
            m_softTelemetryTickCount = 0;
        }

        if (m_networkStage != NetworkComplete && m_transport.poll()) {
            const NativeTransport::Result &result = m_transport.result();
            if (m_networkStage == FetchingHome) {
                const QString transportResult =
                    probeResultText(QString::fromLatin1("BI"));
                qDebug() << "Bilibili home response" << transportResult
                         << result.body.size() << result.errorText;

                if (result.httpStatus == 200) {
                    int apiCode = -9999;
                    QString parseError;
                    bool parsed = false;
                    QVector<RecommendVideoResultCompat> cards;
                    const HomeScreen::Category category =
                        m_homeScreen.category();
                    if (category == HomeScreen::RecommendCategory) {
                        parsed = BilibiliHomeParser::parseRecommend(
                            result.body, &cards,
                            &apiCode, &parseError);
                    } else if (category == HomeScreen::PopularCategory) {
                        parsed = BilibiliHomeParser::parseHotsAll(
                            result.body, &cards,
                            &apiCode, &parseError);
                    } else if (category == HomeScreen::BangumiCategory) {
                        parsed = BilibiliHomeParser::parseBangumi(
                            result.body, &cards,
                            &apiCode, &parseError);
                    } else {
                        parsed = BilibiliHomeParser::parseLive(
                            result.body, &cards,
                            &apiCode, &parseError);
                    }
                    if (parsed) {
                        const bool append = m_homeAppend &&
                            category == HomeScreen::RecommendCategory;
                        int firstThumbnail = 0;
                        int added = cards.size();
                        if (append) {
                            firstThumbnail = m_liveCards.size();
                            added = appendUniqueHomeCards(&m_liveCards, cards);
                            m_homeScreen.appendCards(cards);
                        } else {
                            clearDynamicImages();
                            m_liveCards = cards;
                            m_homeScreen.setCards(m_liveCards);
                            m_cardImageHandles.clear();
                        }
                        int cardIndex;
                        for (cardIndex = append ? firstThumbnail : 0;
                             cardIndex < m_liveCards.size();
                             ++cardIndex) {
                            m_cardImageHandles.append(
                                m_cardPlaceholderHandle);
                        }
                        m_thumbnailIndex = append ? firstThumbnail : 0;
                        m_thumbnailSuccessCount = 0;
                        if (category == HomeScreen::RecommendCategory) {
                            m_homeFreshIndex = m_homeRequestedFreshIndex;
                            m_homeSessionChanged = false;
                            m_homeCanLoadMore = added > 0;
                            m_homeScreen.setCanLoadMore(m_homeCanLoadMore);
                        } else {
                            m_homeCanLoadMore = false;
                            m_homeScreen.setCanLoadMore(false);
                        }
                        m_homeAppend = false;
                        m_networkStage = FetchingThumbnail;
                        startNextThumbnail();
                    } else {
                        qDebug() << "Bilibili home parse failed"
                                 << apiCode << parseError;
                        m_homeScreen.setNetworkStatus(
                            category == HomeScreen::RecommendCategory
                                ? QString::fromUtf8("推荐响应无法识别")
                                : QString::fromLatin1("BI:H200 API%1")
                                      .arg(apiCode));
                        m_networkStage = NetworkComplete;
                    }
                } else {
                    m_homeScreen.setNetworkStatus(transportResult);
                    m_networkStage = NetworkComplete;
                }
            } else if (m_networkStage == FetchingThumbnail) {
                int imageHandle = -1;
                if (result.httpStatus == 200 &&
                    !result.body.isEmpty() && m_context) {
                    makeCurrent();
                    imageHandle = nvgCreateImageMem(
                        m_context,
                        0,
                        reinterpret_cast<unsigned char *>(
                            const_cast<char *>(result.body.constData())),
                        result.body.size());
                    doneCurrent();
                }
                if (imageHandle > 0) {
                    m_dynamicImageHandles.append(imageHandle);
                    m_homeScreen.setCardImage(
                        m_thumbnailIndex, imageHandle);
                    if (m_thumbnailIndex >= 0 &&
                        m_thumbnailIndex < m_cardImageHandles.size()) {
                        m_cardImageHandles[m_thumbnailIndex] = imageHandle;
                    }
                    ++m_thumbnailSuccessCount;
                } else {
                    qDebug() << "Thumbnail failed" << m_thumbnailIndex
                             << result.httpStatus << result.networkError
                             << result.body.size();
                }
                ++m_thumbnailIndex;
                startNextThumbnail();
            } else if (m_networkStage == FetchingDetail) {
                const QString transportResult =
                    probeResultText(QString::fromLatin1("DETAIL"));
                if (result.httpStatus == 200) {
                    VideoDetailCompat detail;
                    int apiCode = -9999;
                    QString parseError;
                    if (BilibiliDetailParser::parseVideoDetail(
                            result.body,
                            &detail,
                            &apiCode,
                            &parseError)) {
                        m_detailScreen.setVideo(detail);
                        m_detailScreen.setNetworkStatus(
                            QString::fromLatin1("DETAIL:H200 P%1")
                                .arg(detail.pages.size()));
                    } else {
                        qDebug() << "Bilibili detail parse failed"
                                 << apiCode << parseError;
                        m_detailScreen.setNetworkStatus(
                            QString::fromLatin1("DETAIL:H200 API%1")
                                .arg(apiCode));
                    }
                } else {
                    m_detailScreen.setNetworkStatus(transportResult);
                }
                m_networkStage = NetworkComplete;
            } else if (m_networkStage == FetchingPlayback) {
                PlaybackSourceCompat source;
                int apiCode = -9999;
                QString parseError;
                if (result.httpStatus == 200 &&
                    BilibiliPlaybackParser::parsePlaybackSource(
                        result.body, &source, &apiCode, &parseError)) {
                    source.videoWidth = m_playbackVideoWidth;
                    source.videoHeight = m_playbackVideoHeight;
                    const bool wasQualitySwitch =
                        m_playbackQualitySwitch;
                    m_detailScreen.setNetworkStatus(
                        QString::fromLatin1("PLAY Q%1 %2")
                            .arg(source.quality).arg(source.format));
                    openVideoPlayback(source);
                    m_playbackQualitySwitch = false;

                    if (wasQualitySwitch) {
                        m_networkStage = NetworkComplete;
                    } else {
                        const QString danmakuEndpoint = QString::fromLatin1(
                            "https://api.bilibili.com/x/v1/dm/list.so?oid=%1")
                            .arg(m_playbackCid);
                        m_networkStage = FetchingDanmaku;
                        if (!m_transport.startGet(
                                QUrl(danmakuEndpoint).toEncoded(),
                                30000, 4 * 1024 * 1024,
                                LoginSession::cookieHeader())) {
                            m_networkStage = NetworkComplete;
                        }
                    }
                    qDebug() << "WW:PLAYBACK_READY"
                             << source.quality << source.format
                             << source.backupUrls.size()
                             << source.videoWidth << source.videoHeight;
                } else {
                    m_detailScreen.setNetworkStatus(
                        result.httpStatus == 200
                            ? QString::fromUtf8("播放地址不可用 API %1")
                                .arg(apiCode)
                            : probeResultText(QString::fromLatin1("PLAY")));
                    m_networkStage = NetworkComplete;
                    m_playbackQualitySwitch = false;
                    qDebug() << "WW:PLAYBACK_FAILED"
                             << result.httpStatus << apiCode << parseError;
                }
            } else if (m_networkStage == FetchingLivePlayback) {
                PlaybackSourceCompat source;
                int apiCode = -9999;
                QString parseError;
                if (result.httpStatus == 200 &&
                    BilibiliPlaybackParser::parseLivePlaybackSource(
                        result.body, &source, &apiCode, &parseError)) {
                    const bool wasQualitySwitch =
                        m_playbackQualitySwitch;
                    m_homeScreen.setNetworkStatus(
                        QString::fromLatin1("LIVE Q%1 %2")
                            .arg(source.quality).arg(source.format));
                    openVideoPlayback(source);
                    m_playbackQualitySwitch = false;
                    m_networkStage = NetworkComplete;
                    qDebug() << "WW:LIVE_PLAYBACK_READY"
                             << source.quality << source.format
                             << source.backupUrls.size()
                             << wasQualitySwitch;
                } else {
                    const bool terminalRoomState =
                        parseError.contains(QString::fromUtf8("尚未开播")) ||
                        parseError.contains(QString::fromUtf8("正在轮播")) ||
                        parseError.contains(QString::fromUtf8("已被封禁"));
                    if (terminalRoomState) {
                        m_homeScreen.setNetworkStatus(parseError);
                        m_networkStage = NetworkComplete;
                        m_playbackQualitySwitch = false;
                        qDebug() << "WW:LIVE_ROOM_UNAVAILABLE"
                                 << apiCode << parseError;
                    } else {
                        const QString legacyEndpoint = QString::fromLatin1(
                            "https://api.live.bilibili.com/room/v1/Room/"
                            "playUrl?cid=%1&platform=web&quality=%2")
                            .arg(m_liveRoomId)
                            .arg(m_requestedPlaybackQuality);
                        m_networkStage = FetchingLegacyLivePlayback;
                        if (!m_transport.startGet(
                                QUrl(legacyEndpoint).toEncoded(), 30000,
                                1024 * 1024,
                                LoginSession::cookieHeader())) {
                            m_homeScreen.setNetworkStatus(
                                QString::fromUtf8("直播兼容线路启动失败"));
                            m_networkStage = NetworkComplete;
                            m_playbackQualitySwitch = false;
                        }
                        qDebug() << "WW:LIVE_FALLBACK"
                                 << result.httpStatus << apiCode << parseError;
                    }
                }
            } else if (m_networkStage == FetchingLegacyLivePlayback) {
                PlaybackSourceCompat source;
                int apiCode = -9999;
                QString parseError;
                if (result.httpStatus == 200 &&
                    BilibiliPlaybackParser::parseLegacyLivePlaybackSource(
                        result.body, m_liveRoomId,
                        &source, &apiCode, &parseError)) {
                    m_homeScreen.setNetworkStatus(
                        QString::fromLatin1("LIVE Q%1 FLV")
                            .arg(source.quality));
                    openVideoPlayback(source);
                    m_playbackQualitySwitch = false;
                    qDebug() << "WW:LIVE_LEGACY_READY"
                             << source.quality << source.backupUrls.size();
                } else {
                    m_homeScreen.setNetworkStatus(
                        result.httpStatus == 200
                            ? QString::fromUtf8("直播地址不可用 API %1")
                                .arg(apiCode)
                            : probeResultText(
                                QString::fromLatin1("LIVE")));
                    m_playbackQualitySwitch = false;
                    qDebug() << "WW:LIVE_PLAYBACK_FAILED"
                             << result.httpStatus << apiCode << parseError;
                }
                m_networkStage = NetworkComplete;
            } else if (m_networkStage == FetchingDanmaku) {
                QVector<DanmakuItemCompat> danmaku;
                QString parseError;
                if (result.httpStatus == 200 &&
                    BilibiliPlaybackParser::parseDanmaku(
                        result.body, &danmaku, &parseError)) {
                    if (m_videoPlayer)
                        m_videoPlayer->setDanmaku(danmaku);
                    qDebug() << "WW:DANMAKU_READY" << danmaku.size();
                } else {
                    qDebug() << "WW:DANMAKU_FAILED"
                             << result.httpStatus << parseError
                             << result.body.left(12).toHex()
                             << result.body.size();
                }
                m_networkStage = NetworkComplete;
            } else if (m_networkStage == FetchingQrToken) {
                QString qrUrl;
                QString qrKey;
                QString parseError;
                if (result.httpStatus == 200 &&
                    BilibiliLoginParser::parseQrToken(
                        result.body, &qrUrl, &qrKey, &parseError)) {
                    m_qrKey = qrKey;
                    m_loginScreen.setQrContent(qrUrl);
                    m_networkStage = NetworkComplete;
                    scheduleLoginPoll(1500);
                    qDebug() << "WW:QR_READY" << qrKey.left(8);
                } else {
                    const QString qrError =
                        probeResultText(QString::fromLatin1("QR"));
                    m_loginScreen.setLoginStatus(
                        qrError + QString::fromUtf8(" 请求失败，点击刷新"),
                        true);
                    m_networkStage = NetworkComplete;
                    qDebug() << "WW:QR_TOKEN_FAILED"
                             << result.httpStatus << result.networkError
                             << result.errorText << parseError
                             << result.body.left(160);
                }
            } else if (m_networkStage == PollingQrLogin) {
                QrLoginPollCompat poll;
                QString parseError;
                if (result.httpStatus == 200 &&
                    BilibiliLoginParser::parseQrPoll(
                        result.body, &poll, &parseError)) {
                    if (poll.code == 86101) {
                        m_loginScreen.setLoginStatus(
                            QString::fromUtf8("等待扫码..."));
                        m_networkStage = NetworkComplete;
                        scheduleLoginPoll(3000);
                    } else if (poll.code == 86090) {
                        m_loginScreen.setLoginStatus(
                            QString::fromUtf8("已扫码，请在手机上确认"));
                        m_networkStage = NetworkComplete;
                        scheduleLoginPoll(1000);
                    } else if (poll.code == 86038) {
                        m_loginScreen.setLoginStatus(
                            QString::fromUtf8("二维码已过期"), true);
                        m_networkStage = NetworkComplete;
                    } else if (poll.code == 0) {
                        // Mirror upstream get_login_info_v2 exactly: keep the
                        // UUID used by this successful poll, generate the new
                        // authenticated buvid3, and append r.cookies only.
                        const QByteArray cookies =
                            LoginSession::cookiesFromQrResponse(
                                result.setCookieHeader, m_qrPollUuid);
                        if (!acceptLoginCookies(
                                cookies, poll.refreshToken)) {
                            m_loginScreen.setLoginStatus(
                                QString::fromUtf8(
                                    "登录响应缺少完整凭据，请刷新二维码"),
                                true);
                            m_networkStage = NetworkComplete;
                        }
                    } else {
                        m_loginScreen.setLoginStatus(
                            poll.message.isEmpty()
                                ? QString::fromUtf8("登录失败，请刷新")
                                : poll.message,
                            true);
                        m_networkStage = NetworkComplete;
                    }
                    qDebug() << "WW:QR_POLL" << poll.code;
                } else {
                    m_loginScreen.setLoginStatus(
                        probeResultText(QString::fromLatin1("POLL")) +
                        QString::fromUtf8(" 轮询失败，正在重试..."));
                    m_networkStage = NetworkComplete;
                    scheduleLoginPoll(3000);
                    qDebug() << "WW:QR_POLL_FAILED"
                             << result.httpStatus << parseError;
                }
            } else if (m_networkStage == FetchingProfile ||
                       m_networkStage == FetchingProfileFallback) {
                const bool isFallback =
                    m_networkStage == FetchingProfileFallback;
                LoginProfileCompat profile;
                QString parseError;
                int apiCode = -9999;
                if (result.httpStatus == 200 &&
                    BilibiliLoginParser::parseProfile(
                        result.body, &profile, &apiCode, &parseError)) {
                    acceptProfile(profile);
                } else if (!isFallback) {
                    qDebug() << "WW:PROFILE_UPSTREAM_FAILED"
                             << result.httpStatus << apiCode << parseError;
                    // Upstream wiliwili reads /x/space/myinfo first. Keep
                    // /nav only as a compatibility validator/fallback.
                    const QString endpoint = QString::fromLatin1(
                        "https://api.bilibili.com/x/web-interface/nav");
                    m_networkStage = FetchingProfileFallback;
                    if (!m_transport.startGet(
                            QUrl(endpoint).toEncoded(), 30000,
                            256 * 1024, LoginSession::cookieHeader())) {
                        m_loginScreen.setLoginStatus(
                            QString::fromUtf8("账号验证备用线路无法启动"),
                            true);
                        m_networkStage = NetworkComplete;
                    }
                } else {
                    QString status;
                    if (apiCode == -101 ||
                        parseError.contains(
                            QString::fromUtf8("未登录"))) {
                        LoginSession::clear();
                        m_sectionScreen.setLoggedIn(false);
                        status = QString::fromUtf8(
                            "会话未被服务器接受，请刷新二维码 (-101)");
                    } else if (result.httpStatus != 200) {
                        status = QString::fromUtf8("账号验证网络失败 (HTTP %1)")
                            .arg(result.httpStatus);
                    } else {
                        QString reason = parseError.trimmed();
                        if (reason.size() > 28)
                            reason = reason.left(28);
                        status = reason.isEmpty()
                            ? QString::fromUtf8("账号资料格式无法识别")
                            : QString::fromUtf8("账号验证失败：") + reason;
                    }
                    m_loginScreen.setLoginStatus(
                        status, true);
                    qDebug() << "WW:PROFILE_FAILED"
                             << result.httpStatus << apiCode << parseError;
                    m_networkStage = NetworkComplete;
                }
            } else if (m_networkStage == FetchingProfileStats) {
                int following = 0;
                int follower = 0;
                QString parseError;
                if (result.httpStatus == 200 &&
                    BilibiliLoginParser::parseProfileStats(
                        result.body, &following, &follower, &parseError)) {
                    LoginProfileCompat profile = m_loginScreen.profile();
                    profile.following = following;
                    profile.follower = follower;
                    m_loginScreen.setProfile(profile);
                    qDebug() << "WW:PROFILE_STATS_READY"
                             << following << follower;
                } else {
                    qDebug() << "WW:PROFILE_STATS_FAILED"
                             << result.httpStatus << parseError;
                }
                m_networkStage = NetworkComplete;
            } else if (m_networkStage == FetchingHomeWbiKeys) {
                QString parseError;
                if (result.httpStatus == 200 &&
                    BilibiliWbi::parseMixinKey(
                        result.body, &m_wbiMixinKey, &parseError)) {
                    qDebug() << "WW:HOME_WBI_READY" << m_wbiMixinKey.size();
                    startBilibiliFeed(m_homeAppend);
                } else {
                    qDebug() << "WW:HOME_WBI_FAILED"
                             << result.httpStatus << parseError;
                    m_homeAppend = false;
                    m_homeCanLoadMore = false;
                    m_homeScreen.setCanLoadMore(false);
                    m_homeScreen.setNetworkStatus(
                        QString::fromUtf8("推荐签名准备失败，请稍后重试"));
                    m_networkStage = NetworkComplete;
                }
            } else if (m_networkStage == FetchingWbiKeys) {
                QString parseError;
                if (result.httpStatus == 200 &&
                    BilibiliWbi::parseMixinKey(
                        result.body, &m_wbiMixinKey, &parseError)) {
                    qDebug() << "WW:WBI_READY" << m_wbiMixinKey.size();
                    startContentTransport();
                } else {
                    qDebug() << "WW:WBI_FAILED"
                             << result.httpStatus << parseError;
                    if (m_contentMode == SearchContentMode ||
                        m_contentMode == SearchUsersContentMode) {
                        m_contentScreen.setStatus(
                            QString::fromUtf8("搜索签名准备失败，请稍后重试"));
                        m_networkStage = NetworkComplete;
                        updateGL();
                        event->accept();
                        return;
                    }
                    // Attempt the endpoint unsigned as a compatibility
                    // fallback; public endpoints sometimes still accept it.
                    m_wbiMixinKey.clear();
                    m_networkStage = FetchingContent;
                    if (!m_transport.startGet(
                            QUrl(m_contentEndpoint).toEncoded(), 30000,
                            1024 * 1024, m_contentCookies)) {
                        m_contentScreen.setStatus(
                            QString::fromLatin1("WBI:INIT"));
                        m_networkStage = NetworkComplete;
                    }
                }
            } else if (m_networkStage == FetchingFavoriteForAction) {
                QVector<ContentItemCompat> folders;
                int apiCode = -9999;
                QString parseError;
                const bool parsed = result.httpStatus == 200 &&
                    BilibiliContentParser::parseFavoriteFolders(
                        result.body, &folders, &apiCode, &parseError);
                m_networkStage = NetworkComplete;
                if (parsed && !folders.isEmpty()) {
                    QByteArray form;
                    appendFormField(&form, "rid", QString::number(
                        m_detailScreen.video().aid));
                    appendFormField(&form, "type", QString::fromLatin1("2"));
                    appendFormField(&form, "add_media_ids", QString::number(
                        folders.first().numericId));
                    appendFormField(&form, "del_media_ids", QString());
                    startPostAction(
                        FavoritePendingAction,
                        QString::fromLatin1(
                            "https://api.bilibili.com/x/v3/fav/resource/deal"),
                        form,
                        QString::fromUtf8("正在加入默认收藏夹..."));
                } else {
                    m_detailScreen.setNetworkStatus(
                        parsed ? QString::fromUtf8("请先在 B 站建立收藏夹")
                               : QString::fromLatin1("FAV API%1 %2")
                                     .arg(apiCode).arg(parseError));
                    m_pendingAction = NoPendingAction;
                }
            } else if (m_networkStage == PostingAction) {
                int apiCode = -9999;
                QString message;
                const PendingAction completedAction = m_pendingAction;
                const bool succeeded = result.httpStatus == 200 &&
                    BilibiliContentParser::parseActionResult(
                        result.body, &apiCode, &message);
                m_networkStage = NetworkComplete;
                m_pendingAction = NoPendingAction;
                if (succeeded) {
                    QString successText;
                    if (completedAction == LikePendingAction)
                        successText = QString::fromUtf8("已点赞");
                    else if (completedAction == CoinPendingAction)
                        successText = QString::fromUtf8("投币成功");
                    else if (completedAction == FavoritePendingAction)
                        successText = QString::fromUtf8("已加入收藏夹");
                    else if (completedAction == WatchLaterPendingAction)
                        successText = QString::fromUtf8("已加入稍后再看");
                    else if (completedAction == FollowPendingAction)
                        successText = QString::fromUtf8("关注成功");
                    else if (completedAction == ChatMessagePendingAction)
                        successText = QString::fromUtf8("私信发送成功");
                    else
                        successText = QString::fromUtf8("发送成功");

                    if ((completedAction == CommentPendingAction ||
                         completedAction == ChatMessagePendingAction) &&
                        m_currentScreen == ContentScreenView) {
                        m_contentScreen.setStatus(successText);
                        refreshContent();
                    } else if (m_currentScreen == ContentScreenView) {
                        m_contentScreen.setStatus(successText);
                    } else {
                        m_detailScreen.setNetworkStatus(successText);
                    }
                } else {
                    const QString failure = result.httpStatus == 200
                        ? QString::fromLatin1("API %1 / %2")
                              .arg(apiCode).arg(message)
                        : probeResultText(QString::fromLatin1("POST"));
                    if (m_currentScreen == ContentScreenView)
                        m_contentScreen.setStatus(failure);
                    else
                        m_detailScreen.setNetworkStatus(failure);
                    qDebug() << "WW:POST_FAILED" << completedAction
                             << result.httpStatus << apiCode << message;
                }
            } else if (m_networkStage == FetchingContent) {
                QVector<ContentItemCompat> items;
                int apiCode = -9999;
                int searchPageCount = 0;
                quint64 nextHistoryCursorMax = 0;
                quint64 nextHistoryCursorViewAt = 0;
                bool historyHasMore = false;
                QString parseError;
                bool parsed = false;
                if (result.httpStatus == 200) {
                    if (m_contentMode == SearchContentMode) {
                        parsed = BilibiliContentParser::parseSearchVideos(
                            result.body, &items, &apiCode, &parseError,
                            &searchPageCount);
                    } else if (m_contentMode == SearchUsersContentMode) {
                        parsed = BilibiliContentParser::parseSearchUsers(
                            result.body, &items, &apiCode, &parseError,
                            &searchPageCount);
                    } else if (m_contentMode == CommentsContentMode) {
                        parsed = BilibiliContentParser::parseComments(
                            result.body, &items, &apiCode, &parseError);
                    } else if (m_contentMode ==
                               CommentRepliesContentMode) {
                        parsed = BilibiliContentParser::parseCommentReplies(
                            result.body, &items, &apiCode, &parseError);
                    } else if (m_contentMode == HistoryContentMode) {
                        parsed = BilibiliContentParser::parseHistory(
                            result.body, &items, &apiCode, &parseError,
                            &nextHistoryCursorMax,
                            &nextHistoryCursorViewAt,
                            &historyHasMore);
                    } else if (m_contentMode == FavoritesContentMode) {
                        parsed = BilibiliContentParser::parseFavoriteFolders(
                            result.body, &items, &apiCode, &parseError);
                    } else if (m_contentMode == FavoriteVideosContentMode) {
                        parsed = BilibiliContentParser::parseFavoriteResources(
                            result.body, &items, &apiCode, &parseError);
                    } else if (m_contentMode == WatchLaterContentMode) {
                        parsed = BilibiliContentParser::parseWatchLater(
                            result.body, &items, &apiCode, &parseError);
                    } else if (m_contentMode == UserVideosContentMode) {
                        parsed = BilibiliContentParser::parseUserVideos(
                            result.body, &items, &apiCode, &parseError);
                    } else if (m_contentMode == FollowingContentMode) {
                        parsed = BilibiliContentParser::parseFollowing(
                            result.body, &items, &apiCode, &parseError);
                    } else if (m_contentMode == ChatMessagesContentMode) {
                        parsed = BilibiliContentParser::parseChatMessages(
                            result.body, &items, &apiCode, &parseError);
                    }
                }
                if (parsed) {
                    const bool append = m_contentAppend;
                    const int previousCount =
                        m_contentScreen.items().size();
                    int added = items.size();
                    if (append) {
                        QVector<ContentItemCompat> probe =
                            m_contentScreen.items();
                        added = appendUniqueContentItems(&probe, items);
                        m_contentScreen.appendItems(items);
                    } else {
                        m_contentScreen.setItems(items);
                        QVector<int> imageSlots;
                        int slot;
                        for (slot = 0; slot < items.size(); ++slot)
                            imageSlots.append(-1);
                        m_contentScreen.setItemImages(imageSlots);
                    }
                    bool hasMore = false;
                    if (m_contentMode == HistoryContentMode) {
                        m_historyCursorMax = nextHistoryCursorMax;
                        m_historyCursorViewAt = nextHistoryCursorViewAt;
                        hasMore = historyHasMore;
                    } else if (!m_contentPageTemplate.isEmpty()) {
                        if ((m_contentMode == SearchContentMode ||
                             m_contentMode == SearchUsersContentMode) &&
                            searchPageCount > 0) {
                            hasMore = m_contentPage < searchPageCount;
                        } else if (m_contentPageSize > 0) {
                            hasMore = items.size() >= m_contentPageSize;
                        }
                    }
                    m_contentCanLoadMore = hasMore && (append ? added > 0
                                                               : !items.isEmpty());
                    m_contentScreen.setCanLoadMore(m_contentCanLoadMore);
                    m_contentScreen.setStatus(
                        m_contentScreen.items().isEmpty()
                            ? QString::fromUtf8("暂无内容")
                            : append
                                ? QString::fromUtf8("已加载 %1 条")
                                      .arg(m_contentScreen.items().size())
                                : QString::fromLatin1("H200 / %1 ITEMS")
                                      .arg(m_contentScreen.items().size()));
                    qDebug() << "WW:CONTENT_READY"
                              << static_cast<int>(m_contentMode)
                              << m_contentScreen.items().size()
                              << "append" << append;
                    m_contentImageIndex = append ? previousCount : 0;
                    m_contentAppend = false;
                    // A Symbian software-input session can recreate the EGL
                    // surface. Uploading the first search thumbnail shortly
                    // afterwards was the reproducible two-second crash. Keep
                    // search rows text-only and avoid that unsafe GL upload.
                    if (m_contentMode == SearchContentMode ||
                        m_contentMode == SearchUsersContentMode ||
                        m_contentMode == CommentsContentMode ||
                        m_contentMode == CommentRepliesContentMode) {
                        m_networkStage = NetworkComplete;
                    } else {
                        m_networkStage = FetchingContentThumbnail;
                        startNextContentThumbnail();
                    }
                } else {
                    if ((m_contentMode == CommentsContentMode ||
                         m_contentMode == CommentRepliesContentMode) &&
                        apiCode == -352 &&
                        !m_commentLegacyFallback) {
                        m_commentLegacyFallback = true;
                        if (m_contentMode == CommentsContentMode) {
                            const int legacySort =
                                m_commentMode == 3 ? 2 : 0;
                            m_contentEndpoint = QString::fromLatin1(
                                "https://api.bilibili.com/x/v2/reply"
                                "?type=1&pn=1&ps=20&sort=%1&nohot=0&oid=%2")
                                .arg(legacySort).arg(m_contentSubjectId);
                        } else {
                            m_contentEndpoint = QString::fromLatin1(
                                "https://api.bilibili.com/x/v2/reply/reply"
                                "?type=1&oid=%1&root=%2&pn=1&ps=20")
                                .arg(m_detailScreen.video().aid)
                                .arg(m_contentSubjectId);
                        }
                        m_contentScreen.setStatus(
                            QString::fromUtf8("主评论接口风控，正在兼容加载..."));
                        qDebug() << "WW:COMMENT_LEGACY_FALLBACK"
                                 << static_cast<int>(m_contentMode)
                                 << m_commentMode;
                        startContentTransport();
                        updateGL();
                        event->accept();
                        return;
                    }
                    if (!m_contentAppend)
                        m_contentScreen.setItems(items);
                    m_contentCanLoadMore = false;
                    m_contentScreen.setCanLoadMore(false);
                    const bool search = m_contentMode == SearchContentMode ||
                        m_contentMode == SearchUsersContentMode;
                    m_contentScreen.setStatus(
                        search && (parseError ==
                                   QString::fromLatin1("search risk response") ||
                                   parseError == QString::fromLatin1(
                                       "missing search result array"))
                            ? QString::fromUtf8(
                                "搜索响应被服务器拦截，请稍后重试")
                            : result.httpStatus == 200
                            ? QString::fromLatin1("API %1 / %2")
                                  .arg(apiCode).arg(parseError)
                            : probeResultText(QString::fromLatin1("CONTENT")));
                    qDebug() << "WW:CONTENT_FAILED"
                             << static_cast<int>(m_contentMode)
                              << result.httpStatus << apiCode << parseError;
                    m_contentAppend = false;
                    m_networkStage = NetworkComplete;
                }
            } else if (m_networkStage == FetchingContentThumbnail) {
                int imageHandle = -1;
                if (result.httpStatus == 200 &&
                    !result.body.isEmpty() && m_context) {
                    makeCurrent();
                    imageHandle = nvgCreateImageMem(
                        m_context, 0,
                        reinterpret_cast<unsigned char *>(
                            const_cast<char *>(result.body.constData())),
                        result.body.size());
                    doneCurrent();
                }
                if (imageHandle > 0) {
                    m_contentImageHandles.append(imageHandle);
                    m_contentScreen.setItemImage(
                        m_contentImageIndex, imageHandle);
                }
                ++m_contentImageIndex;
                startNextContentThumbnail();
            } else if (m_networkStage == FetchingNetworkDiagnostic) {
                QString mixin;
                QString parseError;
                const bool apiReady = result.httpStatus == 200 &&
                    BilibiliWbi::parseMixinKey(
                        result.body, &mixin, &parseError);
                m_sectionScreen.setStatus(apiReady
                    ? QString::fromLatin1("TLS OK / API H200 / JSON OK")
                    : result.httpStatus == 200
                        ? QString::fromLatin1("API H200 / JSON %1")
                              .arg(parseError)
                        : probeResultText(QString::fromLatin1("NET")));
                m_networkStage = NetworkComplete;
            } else if (m_networkStage == FetchingDynamic ||
                       m_networkStage == FetchingMessages) {
                const bool dynamic =
                    m_networkStage == FetchingDynamic;
                QVector<ContentItemCompat> sectionItems;
                int apiCode = -9999;
                QString parseError;
                QString nextDynamicOffset;
                quint64 nextMessageId = 0;
                quint64 nextMessageTime = 0;
                quint64 nextChatTimestamp = 0;
                bool hasMore = false;
                bool parsed = false;
                if (result.httpStatus == 200) {
                    if (dynamic) {
                        parsed = BilibiliSectionParser::parseDynamic(
                            result.body, &sectionItems,
                            &apiCode, &parseError,
                            &nextDynamicOffset, &hasMore);
                    } else if (m_messageType == 3) {
                        parsed = BilibiliSectionParser::parseChatSessions(
                            result.body, &sectionItems,
                            &apiCode, &parseError,
                            &nextChatTimestamp, &hasMore);
                    } else {
                        parsed = BilibiliSectionParser::parseMessages(
                            result.body, &sectionItems,
                            &apiCode, &parseError,
                            &nextMessageId, &nextMessageTime, &hasMore);
                    }
                }
                if (parsed) {
                    const bool append = dynamic
                        ? m_dynamicAppend : m_messageAppend;
                    if (dynamic)
                        qDebug() << "WW:DYNAMIC_READY" << result.httpStatus
                                 << apiCode << sectionItems.size()
                                 << hasMore << nextDynamicOffset.size();
                    if (append)
                        m_sectionScreen.appendItems(sectionItems);
                    else
                        m_sectionScreen.setItems(sectionItems);
                    if (dynamic) {
                        m_dynamicOffset = nextDynamicOffset;
                        m_dynamicHasMore = hasMore &&
                            !m_dynamicOffset.isEmpty();
                        if (!sectionItems.isEmpty())
                            ++m_dynamicPage;
                        m_dynamicAppend = false;
                        m_sectionScreen.setCanLoadMore(m_dynamicHasMore);
                    } else {
                        if (m_messageType == 3) {
                            m_chatBeginTimestamp = nextChatTimestamp;
                            m_messageHasMore = hasMore &&
                                m_chatBeginTimestamp != 0;
                        } else {
                            m_messageCursorId = nextMessageId;
                            m_messageCursorTime = nextMessageTime;
                            m_messageHasMore = hasMore &&
                                m_messageCursorId != 0 &&
                                m_messageCursorTime != 0;
                        }
                        m_messageAppend = false;
                        m_sectionScreen.setCanLoadMore(m_messageHasMore);
                    }
                    m_sectionScreen.setStatus(
                        sectionItems.isEmpty()
                            ? QString::fromUtf8("暂时没有新内容")
                            : append
                                ? QString::fromUtf8("已加载更多 %1 条")
                                      .arg(sectionItems.size())
                                : QString::fromLatin1("SYNC H200 / %1 ITEMS")
                                      .arg(sectionItems.size()));
                } else {
                    if (!(dynamic ? m_dynamicAppend : m_messageAppend))
                        m_sectionScreen.clearItems();
                    if (dynamic) {
                        m_dynamicHasMore = false;
                        m_dynamicAppend = false;
                    } else {
                        m_messageHasMore = false;
                        m_messageAppend = false;
                    }
                    m_sectionScreen.setCanLoadMore(false);
                    m_sectionScreen.setStatus(
                        result.httpStatus == 200
                            ? QString::fromLatin1("API %1").arg(apiCode)
                            : probeResultText(dynamic
                                ? QString::fromLatin1("DYN")
                                : QString::fromLatin1("MSG")));
                    qDebug() << "WW:SECTION_FAILED" << dynamic
                             << result.httpStatus << apiCode << parseError;
                }
                m_networkStage = NetworkComplete;
            }
        }

        if (m_loginPollWaiting &&
            m_loginPollClock.elapsed() >= m_loginPollDelay &&
            m_networkStage == NetworkComplete &&
            m_currentScreen == TopLevelScreenView &&
            m_navigation.selected() == NavigationRail::AccountSection) {
            pollQrLogin();
        }
        if (!playerOwnsForeground())
            updateGL();
        event->accept();
        return;
    }
    QGLWidget::timerEvent(event);
}

void WiliwiliWidget::loadUiResources()
{
    if (!m_context || m_uiResourcesReady)
        return;

    makeCurrent();

    // The GB2312-plus bundled subset is the deterministic first-frame font.
    // The complete deployed font is attached later as a fallback, in chunks,
    // so a cold FAT read cannot turn the UI thread into a long black screen.
    const QString bundledFontPath =
        QString::fromLatin1(":/assets/switch_font.ttf");
    m_fontId = loadNanoVgFontResource(
        m_context, "ui", bundledFontPath);
    if (m_fontId >= 0) {
        qDebug() << "WW:PRIMARY_FONT_READY_BUNDLED" << bundledFontPath;
    } else {
        // A missing compiled resource indicates a broken package. Preserve a
        // last-resort synchronous path so the interface can still render.
        const QStringList fontPaths = installedCjkFontPaths();
        int fontPathIndex;
        for (fontPathIndex = 0;
             fontPathIndex < fontPaths.size(); ++fontPathIndex) {
            const QString path = fontPaths.at(fontPathIndex);
            m_fontId = loadNanoVgFontWithQt(
                m_context, "ui", path, &m_fontData);
            if (m_fontId >= 0) {
                m_cjkFontId = m_fontId;
                qDebug() << "WW:PRIMARY_FONT_READY_INSTALLED_FALLBACK"
                         << path << m_fontData.size();
                break;
            }
        }
    }

    QResource logoResource(QString::fromLatin1(":/assets/nikiniki_icon.png"));
    if (logoResource.isValid()) {
        m_logoHandle = nvgCreateImageMem(
            m_context,
            0,
            const_cast<unsigned char *>(logoResource.data()),
            static_cast<int>(logoResource.size()));
    }

    QResource cardResource(QString::fromLatin1(":/assets/video_card_bg.png"));
    if (cardResource.isValid()) {
        m_cardPlaceholderHandle = nvgCreateImageMem(
            m_context,
            0,
            const_cast<unsigned char *>(cardResource.data()),
            static_cast<int>(cardResource.size()));
    }

    m_homeScreen.initialize(
        m_fontId,
        m_logoHandle,
        m_cardPlaceholderHandle);
    m_detailScreen.initialize(m_fontId, m_cardPlaceholderHandle);
    m_contentScreen.initialize(m_fontId);
    m_loginScreen.initialize(m_fontId);
    m_navigation.initialize(m_fontId, m_logoHandle);
    m_sectionScreen.initialize(m_fontId);
    m_uiResourcesReady = true;

    doneCurrent();
}

void WiliwiliWidget::beginCjkFallbackLoad()
{
    if (!m_context || m_cjkFontId >= 0 || m_cjkFontFile)
        return;

    const QStringList fontPaths = installedCjkFontPaths();
    int pathIndex;
    for (pathIndex = 0;
         pathIndex < fontPaths.size(); ++pathIndex) {
        QFile *file = new QFile(fontPaths.at(pathIndex));
        if (!file->open(QIODevice::ReadOnly)) {
            delete file;
            continue;
        }
        const qint64 size = file->size();
        if (size <= 0 || size > 32 * 1024 * 1024) {
            file->close();
            delete file;
            continue;
        }
        m_cjkFontData.resize(static_cast<int>(size));
        m_cjkFontFile = file;
        m_cjkFontExpectedBytes = size;
        m_cjkFontBytesRead = 0;
        m_cjkFontLoadClock.start();
        qDebug() << "WW:CJK_FONT_LOAD_BEGIN"
                 << file->fileName() << size << 256 * 1024;
        return;
    }
    qDebug() << "WW:CJK_FONT_LOAD_UNAVAILABLE";
}

bool WiliwiliWidget::pumpCjkFallbackLoad()
{
    if (m_cjkFontId >= 0 || !m_cjkFontFile)
        return true;

    const qint64 remaining =
        m_cjkFontExpectedBytes - m_cjkFontBytesRead;
    const qint64 chunkBytes = remaining < 256 * 1024
        ? remaining : 256 * 1024;
    const qint64 bytesRead = m_cjkFontFile->read(
        m_cjkFontData.data() + static_cast<int>(m_cjkFontBytesRead),
        chunkBytes);
    if (bytesRead <= 0) {
        qDebug() << "WW:CJK_FONT_LOAD_FAILED"
                 << m_cjkFontFile->fileName()
                 << m_cjkFontBytesRead << m_cjkFontExpectedBytes;
        m_cjkFontFile->close();
        delete m_cjkFontFile;
        m_cjkFontFile = 0;
        m_cjkFontData.clear();
        return true;
    }

    m_cjkFontBytesRead += bytesRead;
    if (m_cjkFontBytesRead < m_cjkFontExpectedBytes)
        return false;

    const QString fontPath = m_cjkFontFile->fileName();
    m_cjkFontFile->close();
    delete m_cjkFontFile;
    m_cjkFontFile = 0;

    QTime registrationClock;
    registrationClock.start();
    makeCurrent();
    m_cjkFontId = nvgCreateFontMem(
        m_context,
        "cjk",
        reinterpret_cast<unsigned char *>(m_cjkFontData.data()),
        m_cjkFontData.size(),
        0);
    int fallbackAdded = 1;
    if (m_cjkFontId >= 0 && m_fontId >= 0)
        fallbackAdded = nvgAddFallbackFontId(
            m_context, m_fontId, m_cjkFontId);
    doneCurrent();

    if (m_cjkFontId < 0) {
        m_cjkFontData.clear();
        qDebug() << "WW:CJK_FONT_REGISTER_FAILED" << fontPath;
    } else if (!fallbackAdded) {
        qDebug() << "WW:CJK_FONT_FALLBACK_LINK_FAILED" << fontPath;
    } else {
        qDebug() << "WW:CJK_FONT_LOAD_READY"
                 << fontPath << m_cjkFontData.size()
                 << m_cjkFontLoadClock.elapsed()
                 << registrationClock.elapsed();
    }
    return true;
}

void WiliwiliWidget::clearDynamicImages()
{
    if (!m_context || m_dynamicImageHandles.isEmpty())
        return;
    makeCurrent();
    int index;
    for (index = 0; index < m_dynamicImageHandles.size(); ++index)
        nvgDeleteImage(m_context, m_dynamicImageHandles.at(index));
    doneCurrent();
    m_dynamicImageHandles.clear();
}

void WiliwiliWidget::clearContentImages()
{
    if (m_context && !m_contentImageHandles.isEmpty()) {
        makeCurrent();
        int index;
        for (index = 0; index < m_contentImageHandles.size(); ++index)
            nvgDeleteImage(m_context, m_contentImageHandles.at(index));
        doneCurrent();
    }
    m_contentImageHandles.clear();
    ++m_contentImageGeneration;
    QVector<int> emptySlots;
    int slot;
    for (slot = 0; slot < m_contentScreen.items().size(); ++slot)
        emptySlots.append(-1);
    m_contentScreen.setItemImages(emptySlots);
}

void WiliwiliWidget::startBilibiliFeed(bool append, bool manualRefresh)
{
    const HomeScreen::Category category = m_homeScreen.category();
    const bool resumingWbi =
        m_networkStage == FetchingHomeWbiKeys && !m_wbiMixinKey.isEmpty();
    if (append && !resumingWbi &&
        (category != HomeScreen::RecommendCategory || !m_homeCanLoadMore)) {
        return;
    }
    m_transport.cancel();
    QString endpoint;
    QString status;
    const QByteArray cookies = LoginSession::requestCookieHeader();
    if (!resumingWbi)
        m_homeAppend = append && category == HomeScreen::RecommendCategory;
    if (category == HomeScreen::RecommendCategory) {
        if (!resumingWbi) {
            if (m_homeAppend) {
                m_homeRequestedFreshIndex = m_homeFreshIndex + 1;
                m_homeRequestedFreshType = 4;
            } else if (manualRefresh) {
                // Upstream resets the manual-refresh counter to one and
                // uses fresh_type=3. It is deliberately different from
                // continuous scrolling (fresh_type=4).
                m_homeRequestedFreshIndex = 1;
                m_homeRequestedFreshType = 3;
            } else if (m_homeSessionChanged || m_homeFreshIndex <= 0) {
                m_homeRequestedFreshIndex = 1;
                m_homeRequestedFreshType = 0;
            } else {
                m_homeRequestedFreshIndex = m_homeFreshIndex + 1;
                m_homeRequestedFreshType = 3;
            }
        }
        endpoint = QString::fromLatin1("https:") +
            QString::fromStdString(nikiniki::BilibiliEndpoint::Recommend) +
            QString::fromLatin1(
                "?fresh_idx=%1&ps=12&feed_version=V1&fresh_type=%2"
                "&plat=1&x_num=3&y_num=4")
                .arg(m_homeRequestedFreshIndex)
                .arg(m_homeRequestedFreshType);
        status = m_homeAppend
            ? QString::fromLatin1("RCMD:MORE")
            : QString::fromLatin1("RCMD:LOAD");
        m_homeCanLoadMore = false;
        m_homeScreen.setCanLoadMore(false);
        if (m_wbiMixinKey.isEmpty()) {
            m_networkStage = FetchingHomeWbiKeys;
            m_homeScreen.setNetworkStatus(QString::fromLatin1("RCMD:WBI"));
            const QString navEndpoint = QString::fromLatin1(
                "https://api.bilibili.com/x/web-interface/nav");
            if (!m_transport.startGet(
                    QUrl(navEndpoint).toEncoded(), 30000,
                    384 * 1024, cookies)) {
                m_homeScreen.setNetworkStatus(
                    QString::fromUtf8("推荐签名准备失败"));
                m_networkStage = NetworkComplete;
            }
            return;
        }
        endpoint = BilibiliWbi::signUrl(endpoint, m_wbiMixinKey);
    } else if (category == HomeScreen::PopularCategory) {
        endpoint = QString::fromLatin1("https:") +
            QString::fromStdString(nikiniki::BilibiliEndpoint::HotsAll) +
            QString::fromLatin1(
                "?pn=1&ps=12&web_location=bilibili-electron");
        status = QString::fromLatin1("HOT:LOAD");
    } else if (category == HomeScreen::BangumiCategory) {
        endpoint = QString::fromLatin1("https:") +
            QString::fromStdString(nikiniki::BilibiliEndpoint::Bangumi);
        status = QString::fromLatin1("BANGUMI:LOAD");
    } else {
        endpoint = QString::fromLatin1(
            "https://api.live.bilibili.com/"
            "xlive/web-interface/v1/webMain/getList?platform=web");
        status = QString::fromLatin1("LIVE:LOAD");
    }
    m_homeScreen.setNetworkStatus(status);
    m_networkStage = FetchingHome;
    if (!m_transport.startGet(
            QUrl(endpoint).toEncoded(), 30000, 512 * 1024, cookies)) {
        m_homeScreen.setNetworkStatus(QString::fromLatin1("BI:INIT"));
        m_networkStage = NetworkComplete;
    }
}

void WiliwiliWidget::startNextThumbnail()
{
    while (m_thumbnailIndex < m_liveCards.size()) {
        const QString endpoint = thumbnailUrl(
            m_liveCards.at(m_thumbnailIndex).pic);
        if (!endpoint.isEmpty() &&
            m_transport.startGet(
                QUrl(endpoint).toEncoded(), 12000, 512 * 1024)) {
            m_homeScreen.setNetworkStatus(
                QString::fromLatin1("BI:H200 P%1/%2")
                    .arg(m_thumbnailIndex)
                    .arg(m_liveCards.size()));
            return;
        }
        ++m_thumbnailIndex;
    }

    m_homeScreen.setNetworkStatus(
        QString::fromLatin1("BI:H200 LIVE%1 P%2")
            .arg(m_liveCards.size())
            .arg(m_thumbnailSuccessCount));
    m_networkStage = NetworkComplete;
}

void WiliwiliWidget::startNextContentThumbnail()
{
    const QVector<ContentItemCompat> &items = m_contentScreen.items();
    const int maximum = qMin(items.size(), m_contentImageLimit);
    while (m_contentImageIndex < maximum) {
        const QString endpoint = thumbnailUrl(
            items.at(m_contentImageIndex).picture);
        if (!endpoint.isEmpty() &&
            m_transport.startGet(
                QUrl(endpoint).toEncoded(), 12000, 512 * 1024)) {
            return;
        }
        ++m_contentImageIndex;
    }
    m_networkStage = NetworkComplete;
}

void WiliwiliWidget::startVideoDetail(int index)
{
    if (index < 0 || index >= m_liveCards.size())
        return;
    hideSearchEditor();

    const RecommendVideoResultCompat &card = m_liveCards.at(index);
    if (card.kind == LiveHomeCard) {
        startLivePlayback(card);
        return;
    }
    if (card.kind != VideoHomeCard ||
        !card.bvid.startsWith(QString::fromLatin1("BV"))) {
        m_homeScreen.setNetworkStatus(
            QString::fromLatin1("SEASON %1").arg(card.id));
        return;
    }

    m_transport.cancel();
    m_selectedVideoIndex = index;
    NavigationEntry homeEntry;
    homeEntry.screen = TopLevelScreenView;
    homeEntry.restoreHome = true;
    pushNavigation(homeEntry);
    m_currentScreen = DetailScreenView;

    m_detailScreen.setVideo(videoDetailFromCard(card));
    const int imageHandle = index < m_cardImageHandles.size()
        ? m_cardImageHandles.at(index)
        : m_cardPlaceholderHandle;
    m_detailScreen.setImageHandle(imageHandle);
    m_detailScreen.setNetworkStatus(QString::fromLatin1("DETAIL:LOAD"));

    const QString endpoint = QString::fromLatin1("https:") +
        QString::fromStdString(nikiniki::BilibiliEndpoint::Detail) +
        QString::fromLatin1("?bvid=") +
        QString::fromLatin1(QUrl::toPercentEncoding(card.bvid));
    m_networkStage = FetchingDetail;
    if (!m_transport.startGet(
            QUrl(endpoint).toEncoded(), 30000, 768 * 1024)) {
        m_detailScreen.setNetworkStatus(QString::fromLatin1("DETAIL:INIT"));
        m_networkStage = NetworkComplete;
    }
}

void WiliwiliWidget::startVideoDetail(const ContentItemCompat &item)
{
    if (item.id.isEmpty() && item.numericId == 0)
        return;
    hideSearchEditor();
    m_transport.cancel();
    m_selectedVideoIndex = -1;
    if (m_currentScreen == ContentScreenView) {
        NavigationEntry contentEntry;
        contentEntry.screen = ContentScreenView;
        contentEntry.contentMode = m_contentMode;
        pushNavigation(contentEntry);
    } else {
        NavigationEntry topEntry;
        topEntry.screen = TopLevelScreenView;
        topEntry.restoreHome = false;
        pushNavigation(topEntry);
    }
    m_currentScreen = DetailScreenView;

    VideoDetailCompat detail;
    detail.bvid = item.id;
    detail.aid = item.numericId;
    detail.title = item.title;
    detail.description = item.description;
    detail.pic = item.picture;
    detail.owner.name = item.subtitle;
    m_detailScreen.setVideo(detail);
    m_detailScreen.setImageHandle(m_cardPlaceholderHandle);
    m_detailScreen.setNetworkStatus(QString::fromLatin1("DETAIL:LOAD"));

    QString endpoint = QString::fromLatin1("https:") +
        QString::fromStdString(nikiniki::BilibiliEndpoint::Detail);
    if (!item.id.isEmpty()) {
        endpoint += QString::fromLatin1("?bvid=") +
            QString::fromLatin1(QUrl::toPercentEncoding(item.id));
    } else {
        endpoint += QString::fromLatin1("?aid=%1").arg(item.numericId);
    }
    m_networkStage = FetchingDetail;
    if (!m_transport.startGet(
            QUrl(endpoint).toEncoded(), 30000, 768 * 1024,
            LoginSession::cookieHeader())) {
        m_detailScreen.setNetworkStatus(QString::fromLatin1("DETAIL:INIT"));
        m_networkStage = NetworkComplete;
    }
}

void WiliwiliWidget::navigateBack()
{
    if (m_navigationHistory.isEmpty()) {
        // Defensive fallback: no recorded history, behave like a top-level
        // back key (home rail first, then exit).
        if (m_navigation.selected() != NavigationRail::HomeSection) {
            selectSection(NavigationRail::HomeSection);
        } else {
            shutdownAndQuit();
        }
        return;
    }
    const NavigationEntry entry = popNavigation();
    m_transport.cancel();
    switch (entry.screen) {
    case DetailScreenView:
        m_detailScreen.restoreState(entry.detailState);
        m_selectedVideoIndex = entry.selectedVideoIndex;
        m_currentScreen = DetailScreenView;
        m_networkStage = NetworkComplete;
        break;
    case ContentScreenView: {
        // A child detail/content request overwrites both shared screen models.
        // Restore the list and its hidden parent detail together: comment
        // replies still read the parent video's aid after the list is shown.
        const bool contentImagesStillValid =
            entry.contentImageGeneration == m_contentImageGeneration;
        if (!contentImagesStillValid)
            clearContentImages();
        m_detailScreen.restoreState(entry.detailState);
        m_selectedVideoIndex = entry.selectedVideoIndex;
        m_contentMode = entry.contentMode;
        m_contentSubjectId = entry.contentSubjectId;
        m_commentMode = entry.commentMode;
        m_commentLegacyFallback = entry.commentLegacyFallback;
        m_contentEndpoint = entry.contentEndpoint;
        m_contentCookies = entry.contentCookies;
        m_contentPageTemplate = entry.contentPageTemplate;
        m_contentPage = entry.contentPage;
        m_contentPageSize = entry.contentPageSize;
        m_contentCanLoadMore = entry.contentCanLoadMore;
        m_historyCursorMax = entry.historyCursorMax;
        m_historyCursorViewAt = entry.historyCursorViewAt;
        m_searchKeyword = entry.searchKeyword;
        m_searchUsers = entry.searchUsers;
        m_chatBeginTimestamp = entry.chatBeginTimestamp;
        m_contentAppend = false;
        m_contentScreen.restoreState(
            entry.contentState, contentImagesStillValid);
        m_currentScreen = ContentScreenView;
        m_contentImageIndex = contentImagesStillValid
            ? entry.contentImageIndex : 0;
        if (m_contentImageLimit > 0 &&
            m_contentMode != SearchContentMode &&
            m_contentMode != SearchUsersContentMode &&
            m_contentMode != CommentsContentMode &&
            m_contentMode != CommentRepliesContentMode &&
            !m_contentScreen.items().isEmpty()) {
            m_networkStage = FetchingContentThumbnail;
            startNextContentThumbnail();
        } else {
            m_networkStage = NetworkComplete;
        }
        break;
    }
    case TopLevelScreenView:
    default:
        if (entry.section == NavigationRail::HomeSection) {
            showHome();
        } else {
            m_currentScreen = TopLevelScreenView;
            m_navigation.setSelected(entry.section);
            m_selectedVideoIndex = entry.selectedVideoIndex;
            m_networkStage = NetworkComplete;
        }
        break;
    }
    qDebug() << "WW:NAV_RESTORE_STATE"
             << static_cast<int>(entry.screen)
             << static_cast<int>(entry.section)
             << static_cast<int>(entry.contentMode)
             << entry.contentState.items.size()
             << entry.detailState.video.pages.size()
             << (entry.contentImageGeneration == m_contentImageGeneration)
             << m_navigationHistory.size();
    updateGL();
}

void WiliwiliWidget::pushNavigation(const NavigationEntry &entry)
{
    NavigationEntry snapshot = entry;
    if (entry.screen != m_currentScreen) {
        qDebug() << "WW:NAV_SOURCE_CORRECTED"
                 << static_cast<int>(entry.screen)
                 << static_cast<int>(m_currentScreen);
    }
    // The source of a push is always the page currently on screen. Do not let
    // a caller accidentally label a content list as a top-level destination.
    snapshot.screen = m_currentScreen;
    snapshot.section = entry.restoreHome
        ? NavigationRail::HomeSection : m_navigation.selected();
    snapshot.detailState = m_detailScreen.state();
    snapshot.selectedVideoIndex = m_selectedVideoIndex;
    if (snapshot.screen == ContentScreenView) {
        snapshot.contentMode = m_contentMode;
        snapshot.contentState = m_contentScreen.state();
        snapshot.contentSubjectId = m_contentSubjectId;
        snapshot.commentMode = m_commentMode;
        snapshot.commentLegacyFallback = m_commentLegacyFallback;
        snapshot.contentEndpoint = m_contentEndpoint;
        snapshot.contentCookies = m_contentCookies;
        snapshot.contentPageTemplate = m_contentPageTemplate;
        snapshot.contentPage = m_contentPage;
        snapshot.contentPageSize = m_contentPageSize;
        snapshot.contentCanLoadMore = m_contentCanLoadMore;
        snapshot.contentImageGeneration = m_contentImageGeneration;
        snapshot.contentImageIndex = m_contentImageIndex;
        snapshot.historyCursorMax = m_historyCursorMax;
        snapshot.historyCursorViewAt = m_historyCursorViewAt;
        snapshot.searchKeyword = m_searchKeyword;
        snapshot.searchUsers = m_searchUsers;
        snapshot.chatBeginTimestamp = m_chatBeginTimestamp;
    }
    // Defensive cap: correct push/pop keeps the stack shallow. If a bug ever
    // grows it, drop the oldest entry rather than looping forever.
    if (m_navigationHistory.size() >= 32)
        m_navigationHistory.remove(0);
    m_navigationHistory.append(snapshot);
    qDebug() << "WW:NAV_PUSH_STATE"
             << static_cast<int>(snapshot.screen)
             << static_cast<int>(snapshot.section)
             << static_cast<int>(snapshot.contentMode)
             << snapshot.contentState.items.size()
             << snapshot.detailState.video.pages.size()
             << m_navigationHistory.size();
}

WiliwiliWidget::NavigationEntry WiliwiliWidget::popNavigation()
{
    if (m_navigationHistory.isEmpty()) {
        NavigationEntry fallback;
        fallback.screen = TopLevelScreenView;
        fallback.restoreHome = true;
        return fallback;
    }
    // Qt 4.7 QVector has no takeLast(); pop with last() + remove().
    const NavigationEntry entry = m_navigationHistory.last();
    m_navigationHistory.remove(m_navigationHistory.size() - 1);
    return entry;
}

void WiliwiliWidget::showHome()
{
    m_transport.cancel();
    m_currentScreen = TopLevelScreenView;
    m_navigation.setSelected(NavigationRail::HomeSection);
    m_selectedVideoIndex = -1;

    if (m_homeSessionChanged) {
        startBilibiliFeed();
    } else if (m_thumbnailIndex < m_liveCards.size()) {
        m_networkStage = FetchingThumbnail;
        startNextThumbnail();
    } else {
        m_networkStage = NetworkComplete;
    }
}

void WiliwiliWidget::selectSection(NavigationRail::Section section)
{
    if (section == NavigationRail::NoSection)
        return;

    m_sectionScreen.setAboutVisible(false);
    if (section != NavigationRail::HomeSection)
        hideSearchEditor();

    m_transport.cancel();
    m_loginPollWaiting = false;
    m_currentScreen = TopLevelScreenView;
    m_navigation.setSelected(section);

    if (section == NavigationRail::HomeSection) {
        if (m_liveCards.isEmpty() || m_homeSessionChanged) {
            startBilibiliFeed();
        } else if (m_thumbnailIndex < m_liveCards.size()) {
            m_networkStage = FetchingThumbnail;
            startNextThumbnail();
        } else {
            m_networkStage = NetworkComplete;
        }
    } else if (section == NavigationRail::AccountSection) {
        if (LoginSession::isLoggedIn()) {
            m_loginScreen.setLoading(
                QString::fromUtf8("正在读取账号信息..."));
            m_sectionScreen.setLoggedIn(true);
            requestProfile();
        } else {
            m_sectionScreen.setLoggedIn(false);
            m_loginScreen.clearProfile();
            startQrLogin();
        }
    } else {
        m_sectionScreen.setSection(section);
        m_sectionScreen.setLoggedIn(LoginSession::isLoggedIn());
        m_sectionScreen.clearItems();
        if (LoginSession::isLoggedIn() &&
            section == NavigationRail::DynamicSection) {
            requestDynamicFeed();
        } else if (LoginSession::isLoggedIn() &&
                   section == NavigationRail::MessagesSection) {
            requestMessages();
        } else {
            m_networkStage = NetworkComplete;
            if (section == NavigationRail::SettingsSection)
                m_sectionScreen.setStatus(QString::fromLatin1("READY"));
            else
                m_sectionScreen.setStatus(
                    LoginSession::isLoggedIn()
                        ? QString::fromLatin1("ACCOUNT CONNECTED")
                        : QString::fromLatin1("OFFLINE / NO ACCOUNT"));
        }
    }
    updateGL();
}

void WiliwiliWidget::startQrLogin()
{
    m_transport.cancel();
    m_qrKey.clear();
    m_qrPollUuid.clear();
    m_loginPollWaiting = false;
    m_loginScreen.setLoading(QString::fromUtf8("正在获取二维码..."));
    const QString endpoint = QString::fromLatin1("https:") +
        QString::fromStdString(nikiniki::BilibiliEndpoint::QrLoginUrlV2) +
        QString::fromLatin1("?source=main_electron_pc");
    m_networkStage = FetchingQrToken;
    if (!m_transport.startGet(
            QUrl(endpoint).toEncoded(), 12000, 128 * 1024)) {
        m_loginScreen.setLoginStatus(
            QString::fromUtf8("无法启动登录请求，点击刷新"), true);
        m_networkStage = NetworkComplete;
    }
}

void WiliwiliWidget::scheduleLoginPoll(int milliseconds)
{
    m_loginPollDelay = qMax(250, milliseconds);
    m_loginPollClock.start();
    m_loginPollWaiting = true;
}

bool WiliwiliWidget::acceptLoginCookies(
    const QByteArray &cookies,
    const QString &refreshToken)
{
    const bool hasSession = cookies.contains("SESSDATA=");
    const bool hasCsrf = cookies.contains("bili_jct=");
    const bool hasUserId = cookies.contains("DedeUserID=");
    qDebug() << "WW:LOGIN_COOKIE_SUMMARY"
             << hasSession
             << hasCsrf
             << hasUserId
             << cookies.size();
    if (!hasSession || !hasCsrf || !hasUserId)
        return false;

    LoginSession::save(cookies, refreshToken);
    m_loginScreen.setLoginStatus(
        QString::fromUtf8("已收到会话，正在验证账号..."));
    m_sectionScreen.setLoggedIn(true);
    m_networkStage = NetworkComplete;
    requestProfile();
    return true;
}

void WiliwiliWidget::pollQrLogin()
{
    if (m_qrKey.isEmpty())
        return;
    m_loginPollWaiting = false;
    const QString endpoint =
        QString::fromLatin1("https:") +
        QString::fromStdString(nikiniki::BilibiliEndpoint::QrLoginInfoV2) +
        QString::fromLatin1("?qrcode_key=") +
        QString::fromLatin1(QUrl::toPercentEncoding(m_qrKey)) +
        QString::fromLatin1("&source=main_electron_pc");
    m_networkStage = PollingQrLogin;
    const QByteArray pollCookies =
        LoginSession::newQrDeviceCookieHeader(&m_qrPollUuid);
    if (!m_transport.startGet(
            QUrl(endpoint).toEncoded(),
            12000,
            128 * 1024,
            pollCookies)) {
        m_networkStage = NetworkComplete;
        scheduleLoginPoll(3000);
    }
}

void WiliwiliWidget::requestProfile()
{
    const QByteArray cookies = LoginSession::cookieHeader();
    if (cookies.isEmpty()) {
        startQrLogin();
        return;
    }
    m_transport.cancel();
    const QString endpoint = QString::fromLatin1("https:") +
        QString::fromStdString(nikiniki::BilibiliEndpoint::MyInfo);
    m_networkStage = FetchingProfile;
    if (!m_transport.startGet(
            QUrl(endpoint).toEncoded(), 30000, 256 * 1024, cookies)) {
        m_loginScreen.setLoginStatus(
            QString::fromUtf8("无法读取账号信息"), true);
        m_networkStage = NetworkComplete;
    }
}

void WiliwiliWidget::startVideoPlayback(int quality, bool qualitySwitch)
{
    hideSearchEditor();
    const VideoDetailCompat &video = m_detailScreen.video();
    const int pageIndex = m_detailScreen.selectedPageIndex();
    if (video.bvid.isEmpty() || video.pages.isEmpty() ||
        pageIndex < 0 || pageIndex >= video.pages.size() ||
        video.pages.at(pageIndex).cid == 0) {
        m_detailScreen.setNetworkStatus(
            QString::fromUtf8("视频分 P 信息尚未就绪"));
        return;
    }
    m_transport.cancel();
    if (quality <= 0) {
        QSettings settings(
            QSettings::IniFormat, QSettings::UserScope,
            QString::fromLatin1("wiliwili"),
            QString::fromLatin1("wiliwili_symbian"));
        quality = settings.value(
            QString::fromLatin1("player/video_quality_061"), 32).toInt();
    }
    m_playbackIsLive = false;
    m_requestedPlaybackQuality = quality;
    m_playbackQualitySwitch = qualitySwitch;
    m_playbackCid = video.pages.at(pageIndex).cid;
    m_playbackVideoWidth = video.pages.at(pageIndex).width;
    m_playbackVideoHeight = video.pages.at(pageIndex).height;
    const QString endpoint = QString::fromLatin1(
        "https://api.bilibili.com/x/player/playurl"
        "?bvid=%1&cid=%2&qn=%3&fnver=0&fnval=0"
        "&fourk=1&platform=html5&high_quality=1")
        .arg(QString::fromLatin1(QUrl::toPercentEncoding(video.bvid)))
        .arg(m_playbackCid)
        .arg(quality);
    if (!qualitySwitch) {
        m_detailScreen.setNetworkStatus(
            QString::fromUtf8("正在获取兼容播放地址..."));
    }
    m_networkStage = FetchingPlayback;
    if (!m_transport.startGet(
            QUrl(endpoint).toEncoded(), 30000,
            1024 * 1024, LoginSession::cookieHeader())) {
        m_detailScreen.setNetworkStatus(
            QString::fromUtf8("无法启动播放地址请求"));
        m_networkStage = NetworkComplete;
        m_playbackQualitySwitch = false;
    }
}

void WiliwiliWidget::startLivePlayback(
    const RecommendVideoResultCompat &card)
{
    hideSearchEditor();
    if (card.id == 0) {
        m_homeScreen.setNetworkStatus(QString::fromUtf8("直播间号不可用"));
        return;
    }
    m_liveRoomId = card.id;
    m_liveTitle = card.title;
    if (!card.owner.name.isEmpty())
        m_liveTitle += QString::fromUtf8(" · ") + card.owner.name;

    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    const int quality = settings.value(
        QString::fromLatin1("player/live_quality"), 150).toInt();
    requestLivePlayback(quality, false);
}

void WiliwiliWidget::requestLivePlayback(
    int quality, bool qualitySwitch)
{
    if (m_liveRoomId == 0)
        return;
    m_transport.cancel();
    m_playbackIsLive = true;
    m_requestedPlaybackQuality = quality > 0 ? quality : 150;
    m_playbackQualitySwitch = qualitySwitch;
    const QString endpoint = QString::fromLatin1(
        "https://api.live.bilibili.com/xlive/web-room/v2/index/"
        "getRoomPlayInfo?room_id=%1&no_playurl=0&mask=1&qn=%2"
        "&platform=web&protocol=0,1&format=0,1,2&codec=0"
        "&dolby=5&ptype=8&panorama=1")
        .arg(m_liveRoomId)
        .arg(m_requestedPlaybackQuality);
    if (!qualitySwitch) {
        m_homeScreen.setNetworkStatus(
            QString::fromUtf8("正在获取直播 Q%1...")
                .arg(m_requestedPlaybackQuality));
    }
    m_networkStage = FetchingLivePlayback;
    if (!m_transport.startGet(
            QUrl(endpoint).toEncoded(), 30000, 2 * 1024 * 1024,
            LoginSession::cookieHeader())) {
        m_homeScreen.setNetworkStatus(
            QString::fromUtf8("无法启动直播地址请求"));
        m_networkStage = NetworkComplete;
        m_playbackQualitySwitch = false;
    }
}

void WiliwiliWidget::videoPlayerRequestQuality(int quality)
{
    if (quality <= 0)
        return;
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    settings.setValue(
        m_playbackIsLive
            ? QString::fromLatin1("player/live_quality")
            : QString::fromLatin1("player/video_quality_061"),
        quality);
    if (m_playbackIsLive)
        requestLivePlayback(quality, true);
    else
        startVideoPlayback(quality, true);
}

void WiliwiliWidget::videoPlayerDidClose()
{
    videoPlayerClearSoftwareVideo();
    qDebug() << "WW:PLAYER_CLOSED_RESTORE";
    if (m_networkStage == FetchingPlayback ||
        m_networkStage == FetchingLivePlayback ||
        m_networkStage == FetchingLegacyLivePlayback ||
        m_networkStage == FetchingDanmaku) {
        m_transport.cancel();
        m_networkStage = NetworkComplete;
    }
    m_playbackQualitySwitch = false;
    // Keep the complete native playback graph alive until application exit.
    // Recreating a QWidget/CCoeControl/RWindow after the first session is the
    // remaining device-only crash path after persistent-overlay reuse failed.
    // closePlayer() has already stopped MMF, detached the overlay and hidden
    // both playback widgets; retaining this pointer makes the next entry reuse
    // the same controller, video host and MMF observer.
    qDebug() << "WW:PLAYER_SESSION_RETAINED"
             << static_cast<void *>(m_videoPlayer);
    m_hasActivated = false;
    showFullScreen();
    raise();
    activateWindow();
    setFocus(Qt::ActiveWindowFocusReason);
    updateGL();
    scheduleForegroundRestore();
}

bool WiliwiliWidget::videoPlayerCanPresentYuv420() const
{
    return m_yuvRendererReady && m_yuvProgram != 0 &&
        m_yuvTextureY != 0 && m_yuvTextureU != 0 && m_yuvTextureV != 0 &&
        m_yuvTextureYAlt != 0 && m_yuvTextureUAlt != 0 &&
        m_yuvTextureVAlt != 0;
}

void WiliwiliWidget::videoPlayerPresentYuv420(
    const Yuv420Frame &frame)
{
    if (!videoPlayerCanPresentYuv420() || !frame.isValid())
        return;
    m_yuvFrame = frame;
    ++m_yuvPresentedCount;
    if (!m_yuvFirstFrameLogged) {
        m_yuvFirstFrameLogged = true;
        qDebug() << "WW:GLES_YUV_FIRST_FRAME"
                 << frame.width << frame.height
                 << "native-orientation" << frame.pts << frame.serial;
    }
    updateGL();
}

void WiliwiliWidget::videoPlayerClearSoftwareVideo()
{
    if (m_yuvPresentedCount || m_yuvUploadedCount) {
        qDebug() << "WW:GLES_YUV_SUMMARY"
                 << m_yuvPresentedCount << m_yuvUploadedCount
                 << m_yuvUploadedSerial << m_yuvUploadMilliseconds;
    }
    m_yuvFrame = Yuv420Frame();
    m_yuvUploadedSerial = -1;
    m_yuvTextureSet = 0;
    m_yuvPresentedCount = 0;
    m_yuvUploadedCount = 0;
    m_yuvUploadMilliseconds = 0;
    m_yuvUploadYMilliseconds = 0;
    m_yuvUploadUMilliseconds = 0;
    m_yuvUploadVMilliseconds = 0;
    m_yuvUploadOtherMilliseconds = 0;
    m_yuvDrawMilliseconds = 0;
    m_yuvSwapMilliseconds = 0;
    m_yuvPresentCallMilliseconds = 0;
    m_yuvPaintGlMilliseconds = 0;
    m_yuvLastPaintGlMilliseconds = 0;
    m_yuvPresentCallCount = 0;
    m_softTelemetryTickCount = 0;
    m_yuvFirstFrameLogged = false;
    if (playerOwnsForeground())
        updateGL();
}

void WiliwiliWidget::openVideoPlayback(
    const PlaybackSourceCompat &source)
{
    hideSearchEditor();
    if (!m_videoPlayer) {
        m_videoPlayer = new VideoPlayerWidget(this, this);
        qDebug() << "WW:PLAYER_SESSION_OBJECT_NEW"
                 << static_cast<void *>(m_videoPlayer);
    }
    m_videoPlayer->setPlaybackPreferences(
        m_playbackMode, m_decoderMode);
    suspendForegroundRestoreForPlayer();
    m_hasActivated = false;
    if (!m_playbackQualitySwitch)
        m_videoPlayer->setDanmaku(QVector<DanmakuItemCompat>());
    QString playerTitle = m_playbackIsLive
        ? m_liveTitle : m_detailScreen.video().title;
    if (!m_playbackIsLive) {
        const int pageIndex = m_detailScreen.selectedPageIndex();
        if (pageIndex >= 0 &&
            pageIndex < m_detailScreen.video().pages.size() &&
            m_detailScreen.video().pages.size() > 1) {
            playerTitle += QString::fromUtf8(" · P%1 %2")
                .arg(pageIndex + 1)
                .arg(m_detailScreen.video().pages.at(pageIndex).part);
        }
    }
    m_videoPlayer->openSource(
        source,
        playerTitle,
        LoginSession::cookieHeader());
}

#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
void WiliwiliWidget::scheduleDevVideoDirectProbe()
{
    hideSearchEditor();
    if (!m_videoPlayer) {
        m_videoPlayer = new VideoPlayerWidget(this, this);
        qDebug() << "WW:DIRECT_PROBE_PLAYER_NEW"
                 << static_cast<void *>(m_videoPlayer);
    }
    suspendForegroundRestoreForPlayer();
    m_hasActivated = false;
    // Match the proven app-shell probe delay: the main QGL window has painted
    // and hidden Avkon chrome before the player begins the 640x360 work-area
    // state machine.
    QTimer::singleShot(
        1500, m_videoPlayer, SLOT(startDevVideoDirectProbe()));
    qDebug() << "WW:DIRECT_PROBE_SCHEDULED" << 1500;
}
#endif

void WiliwiliWidget::acceptProfile(const LoginProfileCompat &profile)
{
    if (LoginSession::cookieValue("DedeUserID").isEmpty()) {
        QByteArray cookies = LoginSession::cookieHeader();
        if (!cookies.isEmpty())
            cookies += "; ";
        cookies += "DedeUserID=";
        cookies += QByteArray::number(profile.mid);
        LoginSession::save(cookies, LoginSession::refreshToken());
        qDebug() << "WW:LOGIN_UID_RECOVERED" << profile.mid;
    }
    const bool accountChanged = !m_loginScreen.hasProfile() ||
        m_loginScreen.profile().mid != profile.mid;
    m_loginScreen.setProfile(profile);
    m_loginScreen.setLoginStatus(QString::fromUtf8("登录成功"));
    m_sectionScreen.setLoggedIn(true);
    m_homeSessionChanged = m_homeSessionChanged || accountChanged;
    qDebug() << "WW:PROFILE_READY" << profile.mid;
    const QString endpoint = QString::fromLatin1(
        "https://api.bilibili.com/x/relation/stat?vmid=%1")
        .arg(profile.mid);
    m_networkStage = FetchingProfileStats;
    if (!m_transport.startGet(
            QUrl(endpoint).toEncoded(), 30000,
            128 * 1024, LoginSession::requestCookieHeader())) {
        m_networkStage = NetworkComplete;
    }
}

void WiliwiliWidget::requestDynamicFeed(bool append)
{
    if (!LoginSession::isLoggedIn()) {
        m_networkStage = NetworkComplete;
        m_sectionScreen.setStatus(QString::fromLatin1("LOGIN REQUIRED"));
        return;
    }
    if (append && !m_dynamicHasMore)
        return;
    m_transport.cancel();
    if (!append) {
        m_dynamicPage = 1;
        m_dynamicOffset.clear();
        m_dynamicHasMore = false;
        m_sectionScreen.clearItems();
    }
    m_dynamicAppend = append;
    m_sectionScreen.setCanLoadMore(false);
    m_sectionScreen.setStatus(append
        ? QString::fromUtf8("正在加载更多动态...")
        : QString::fromUtf8("正在读取动态..."));
    // The old desktop feed endpoint is still present in the upstream API
    // table, but current accounts may receive an empty/skeleton response from
    // it.  Use the live cursor-based mixed feed instead.  It returns video, image
    // and text dynamics in one list; BV items still retain their direct
    // detail/player route.
    const QString features = QString::fromLatin1(
        "itemOpusStyle,opusBigCover,forwardListHidden,ugcDelete");
    // Both the feature list and the cursor are ASCII-safe here.  Keep them
    // literal and pass the encoded bytes directly; wrapping a pre-encoded
    // query in QUrl would turn '%' into '%25' on Qt 4, as happened in search.
    const QString endpointText = QString::fromLatin1("https:") +
        QString::fromStdString(nikiniki::BilibiliEndpoint::DynamicFeedAll) +
        QString::fromLatin1(
            "?type=all&platform=web&web_location=333.1387&features=%1&offset=%2")
            .arg(features)
            .arg(m_dynamicOffset);
    const QByteArray endpoint = endpointText.toLatin1();
    qDebug() << "WW:DYNAMIC_REQUEST" << m_dynamicPage << append
             << m_dynamicOffset.size();
    qDebug() << "WW:DYNAMIC_WIRE" << endpoint.contains("%25")
             << endpoint.size();
    m_networkStage = FetchingDynamic;
    if (!m_transport.startGet(
            endpoint, 30000, 768 * 1024,
            LoginSession::requestCookieHeader())) {
        m_sectionScreen.setStatus(QString::fromLatin1("DYN:INIT"));
        m_networkStage = NetworkComplete;
    }
}

void WiliwiliWidget::requestMessages(int type, bool append)
{
    if (!LoginSession::isLoggedIn()) {
        m_networkStage = NetworkComplete;
        m_sectionScreen.setStatus(QString::fromLatin1("LOGIN REQUIRED"));
        return;
    }
    const int requestedType = qBound(0, type, 3);
    if (append && requestedType == m_messageType && !m_messageHasMore)
        return;
    m_transport.cancel();
    if (!append || requestedType != m_messageType) {
        m_messageCursorId = 0;
        m_messageCursorTime = 0;
        m_chatBeginTimestamp = 0;
        m_messageHasMore = false;
        m_sectionScreen.clearItems();
    }
    m_messageType = requestedType;
    m_messageAppend = append;
    m_sectionScreen.setMessageTab(m_messageType);
    m_sectionScreen.setCanLoadMore(false);
    m_sectionScreen.setStatus(append
        ? QString::fromUtf8("正在加载更多消息...")
        : QString::fromUtf8("正在读取消息..."));
    QString endpoint;
    if (m_messageType == 3) {
        endpoint = QString::fromLatin1("https:") +
            QString::fromStdString(nikiniki::BilibiliEndpoint::ChatSessions) +
            QString::fromLatin1("?begin_ts=%1&mobi_app=web")
                .arg(m_chatBeginTimestamp);
    } else if (m_messageType == 1) {
        endpoint = QString::fromLatin1("https:") +
            QString::fromStdString(nikiniki::BilibiliEndpoint::MsgFeedAt) +
            QString::fromLatin1("?id=%1&at_time=%2")
                .arg(m_messageCursorId).arg(m_messageCursorTime);
    } else if (m_messageType == 2) {
        endpoint = QString::fromLatin1("https:") +
            QString::fromStdString(nikiniki::BilibiliEndpoint::MsgFeedLike) +
            QString::fromLatin1("?id=%1&like_time=%2")
                .arg(m_messageCursorId).arg(m_messageCursorTime);
    } else {
        endpoint = QString::fromLatin1("https:") +
            QString::fromStdString(nikiniki::BilibiliEndpoint::MsgFeedReply) +
            QString::fromLatin1("?id=%1&reply_time=%2")
                .arg(m_messageCursorId).arg(m_messageCursorTime);
    }
    m_networkStage = FetchingMessages;
    if (!m_transport.startGet(
            QUrl(endpoint).toEncoded(), 30000, 512 * 1024,
            LoginSession::requestCookieHeader())) {
        m_sectionScreen.setStatus(QString::fromLatin1("MSG:INIT"));
        m_networkStage = NetworkComplete;
    }
}

void WiliwiliWidget::loadMoreSection()
{
    if (m_navigation.selected() == NavigationRail::DynamicSection)
        requestDynamicFeed(true);
    else if (m_navigation.selected() == NavigationRail::MessagesSection)
        requestMessages(m_messageType, true);
}

void WiliwiliWidget::openSectionItem(int index)
{
    const QString id = m_sectionScreen.itemId(index);
    const QString title = m_sectionScreen.itemTitle(index);
    const QString subtitle = m_sectionScreen.itemSubtitle(index);
    const QString description = m_sectionScreen.itemDescription(index);
    const QString bvid = bvidFromReference(id);
    if (!bvid.isEmpty()) {
        ContentItemCompat item;
        item.kind = VideoContentItem;
        item.id = bvid;
        startVideoDetail(item);
        return;
    }
    if (m_navigation.selected() == NavigationRail::MessagesSection &&
        m_messageType == 3) {
        NavigationEntry chatEntry;
        chatEntry.screen = TopLevelScreenView;
        chatEntry.restoreHome = false;
        pushNavigation(chatEntry);
        requestChatMessages(id.toULongLong(), title);
        return;
    }

    // A non-video dynamic or inbox source still deserves a readable landing
    // page.  The full dynamic-detail/comment API is intentionally left out of
    // this lightweight Symbian pass, but the selected event is no longer a
    // dead card or falsely marked read.
    ContentItemCompat item;
    item.kind = TextContentItem;
    item.id = QString::fromLatin1("section-item");
    item.title = description.isEmpty() ? title : description;
    item.subtitle = title;
    if (!subtitle.isEmpty())
        item.subtitle += QString::fromUtf8(" · ") + subtitle;
    NavigationEntry topEntry;
    topEntry.screen = TopLevelScreenView;
    topEntry.restoreHome = false;
    pushNavigation(topEntry);
    m_contentMode = SectionItemContentMode;
    m_contentEndpoint.clear();
    m_contentPageTemplate.clear();
    m_contentCanLoadMore = false;
    m_contentScreen.setTitle(
        m_navigation.selected() == NavigationRail::DynamicSection
            ? QString::fromUtf8("动态内容") : QString::fromUtf8("消息内容"));
    m_contentScreen.setHeaderAction(QString());
    QVector<ContentItemCompat> items;
    items.append(item);
    m_contentScreen.setItems(items);
    m_contentScreen.setCanLoadMore(false);
    m_contentScreen.setStatus(QString());
    m_currentScreen = ContentScreenView;
    m_networkStage = NetworkComplete;
}

void WiliwiliWidget::requestNetworkDiagnostic()
{
    m_transport.cancel();
    m_sectionScreen.setStatus(QString::fromLatin1("TLS / API TESTING..."));
    m_networkStage = FetchingNetworkDiagnostic;
    const QString endpoint = QString::fromLatin1(
        "https://api.bilibili.com/x/web-interface/nav");
    if (!m_transport.startGet(
            QUrl(endpoint).toEncoded(), 30000,
            384 * 1024, LoginSession::cookieHeader())) {
        m_sectionScreen.setStatus(QString::fromLatin1("NET:INIT"));
        m_networkStage = NetworkComplete;
    }
}

void WiliwiliWidget::toggleContentImages()
{
    m_contentImageLimit = m_contentImageLimit > 0 ? 0 : 14;
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    settings.setValue(QString::fromLatin1("ui/content_images"),
                      m_contentImageLimit);
    settings.sync();
    m_sectionScreen.setImageLoadingEnabled(m_contentImageLimit > 0);
    m_sectionScreen.setStatus(
        m_contentImageLimit > 0
            ? QString::fromUtf8("列表缩略图已开启")
            : QString::fromUtf8("列表缩略图已关闭"));
}

void WiliwiliWidget::setPlaybackMode(int mode)
{
    m_playbackMode = qBound(
        static_cast<int>(VideoPlayerWidget::UrlStreamingPlayback), mode,
        static_cast<int>(VideoPlayerWidget::DownloadThenPlayback));
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    settings.setValue(
        QString::fromLatin1("player/playback_mode"), m_playbackMode);
    settings.sync();
    m_sectionScreen.setPlaybackPreferences(
        m_playbackMode, m_decoderMode);
    m_sectionScreen.setPreferencePage(
        SectionScreen::NoPreferencePage);
    m_sectionScreen.setStatus(
        m_playbackMode == VideoPlayerWidget::UrlStreamingPlayback
            ? QString::fromUtf8("播放方式：流式播放（OpenUrlL）")
            : m_playbackMode ==
                  VideoPlayerWidget::OpenFileStreamingPlayback
            ? QString::fromUtf8("播放方式：OpenFileL 边下边播")
            : QString::fromUtf8("播放方式：下载后播放"));
}

void WiliwiliWidget::setDecoderMode(int mode)
{
    m_decoderMode = qBound(
        static_cast<int>(VideoPlayerWidget::AutomaticDecoder), mode,
        static_cast<int>(VideoPlayerWidget::SoftwareOnlyDecoder));
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    settings.setValue(
        QString::fromLatin1("player/decoder_mode"), m_decoderMode);
    settings.sync();
    m_sectionScreen.setPlaybackPreferences(
        m_playbackMode, m_decoderMode);
    m_sectionScreen.setPreferencePage(
        SectionScreen::NoPreferencePage);
    m_sectionScreen.setStatus(
        m_decoderMode == VideoPlayerWidget::AutomaticDecoder
            ? QString::fromUtf8("解码方式：自动选择")
            : m_decoderMode == VideoPlayerWidget::HardwareOnlyDecoder
            ? QString::fromUtf8("解码方式：全程硬解")
            : QString::fromUtf8("解码方式：全程软解"));
}

void WiliwiliWidget::promptSearch()
{
    // The native editor is part of the home page only. A delayed tap/action
    // must never resurrect it over details, results, account pages or video.
    if (m_currentScreen != TopLevelScreenView ||
        m_navigation.selected() != NavigationRail::HomeSection ||
        playerOwnsForeground()) {
        qDebug() << "WW:SEARCH_EDITOR_REJECTED_OFF_HOME";
        hideSearchEditor();
        return;
    }
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    const QString previous =
        settings.value(QString::fromLatin1("search/last_keyword")).toString();
    if (!m_searchEdit)
        return;
    if (m_foregroundTimerId) {
        killTimer(m_foregroundTimerId);
        m_foregroundTimerId = 0;
    }
    m_searchEdit->setText(previous);
    positionSearchEditor();
    m_searchEdit->show();
    m_searchEdit->raise();
    // Belle ignores RequestSoftwareInputPanel when it is sent in the same
    // event turn in which a tool window is shown. Activate in two delayed
    // phases, then request the software keyboard.
    if (m_searchFocusTimerId)
        killTimer(m_searchFocusTimerId);
    m_searchFocusPhase = 0;
    m_searchFocusTimerId = startTimer(80);
    qDebug() << "WW:SEARCH_EDITOR_SHOWN" << m_searchEdit->geometry();
}

void WiliwiliWidget::submitSearch(
    const QString &rawInput, bool pushHistory)
{
    QString input = rawInput.trimmed();
    hideSearchEditor();
    if (input.isEmpty())
        return;
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    const bool searchUsers = input.startsWith(QLatin1Char('@'));
    if (searchUsers)
        input = input.mid(1).trimmed();
    if (input.isEmpty())
        return;
    const QString keyword = input;
    m_searchKeyword = keyword;
    m_searchUsers = searchUsers;
    settings.setValue(QString::fromLatin1("search/last_keyword"), keyword);
    settings.setValue(QString::fromLatin1("search/type"),
                      searchUsers ? 1 : 0);
    settings.sync();

    // Match upstream wiliwili's WBI search endpoint and PC parameter set.
    // The old non-WBI compatibility route can return a valid 30-item payload
    // whose contents are recommendations unrelated to the supplied keyword.
    const QString pageTemplate = QString::fromLatin1(
        "https://api.bilibili.com/x/web-interface/wbi/search/type"
        "?__refresh__=true&_extra=&context=&page={page}&page_size=20"
        "&order=totalrank&duration=&from_source=&from_spmid=333.337"
        "&platform=pc&device=mac&highlight=1&single_column=0&keyword=") +
        QString::fromLatin1(QUrl::toPercentEncoding(keyword)) +
        QString::fromLatin1("&category_id=&search_type=") +
        (searchUsers ? QString::fromLatin1("bili_user")
                     : QString::fromLatin1("video")) +
        QString::fromLatin1("&dynamic_offset=0");
    QString endpoint = pageTemplate;
    endpoint.replace(QString::fromLatin1("{page}"),
                     QString::fromLatin1("1"));
    qDebug() << "WW:SEARCH_SUBMIT"
             << keyword.size()
             << QCryptographicHash::hash(
                    keyword.toUtf8(), QCryptographicHash::Md5)
                    .toHex().left(8)
             << (searchUsers ? 1 : 0);
    if (pushHistory) {
        NavigationEntry searchEntry;
        searchEntry.screen = TopLevelScreenView;
        searchEntry.restoreHome = false;
        pushNavigation(searchEntry);
    }
    requestContent(
        searchUsers ? SearchUsersContentMode : SearchContentMode,
        searchUsers ? QString::fromUtf8("搜索用户")
                    : QString::fromUtf8("搜索视频"),
        keyword,
        endpoint,
        LoginSession::requestCookieHeader(),
        QString::fromUtf8("改搜"),
        false,
        pageTemplate,
        1,
        20);
    m_contentScreen.setSecondaryHeaderAction(
        searchUsers ? QString::fromUtf8("视频")
                    : QString::fromUtf8("用户"));
}

void WiliwiliWidget::hideSearchEditor()
{
    if (m_searchFocusTimerId) {
        killTimer(m_searchFocusTimerId);
        m_searchFocusTimerId = 0;
    }
    m_searchFocusPhase = 0;
    if (!m_searchEdit || !m_searchEdit->isVisible())
        return;
    QEvent closePanel(QEvent::CloseSoftwareInputPanel);
    QApplication::sendEvent(m_searchEdit, &closePanel);
    m_searchEdit->setEditFocus(false);
    m_searchEdit->clearFocus();
    m_searchEdit->hide();
    showFullScreen();
    raise();
    activateWindow();
    QApplication::setActiveWindow(this);
    setFocus(Qt::OtherFocusReason);
    updateGL();
}

void WiliwiliWidget::positionSearchEditor()
{
    if (!m_searchEdit)
        return;
    const int left = 10;
    const int editorWidth = qMax(160, width() - 20);
    const QPoint globalTopLeft = mapToGlobal(QPoint(left, 8));
    m_searchEdit->setGeometry(
        globalTopLeft.x(), globalTopLeft.y(), editorWidth, 42);
}

void WiliwiliWidget::requestContent(
    int mode,
    const QString &title,
    const QString &subtitle,
    const QString &endpoint,
    const QByteArray &cookies,
    const QString &headerAction,
    bool append,
    const QString &pageTemplate,
    int page,
    int pageSize)
{
    m_transport.cancel();
    m_contentMode = static_cast<ContentMode>(mode);
    m_contentEndpoint = endpoint;
    m_contentCookies = LoginSession::requestCookieHeader();
    if (m_contentCookies.isEmpty())
        m_contentCookies = cookies;
    m_contentAppend = append;
    m_contentCanLoadMore = false;
    m_contentPageTemplate = pageTemplate;
    m_contentPage = qMax(1, page);
    m_contentPageSize = qMax(0, pageSize);
    m_currentScreen = ContentScreenView;
    if (!append) {
        clearContentImages();
        m_contentScreen.setTitle(title, subtitle);
        m_contentScreen.setHeaderAction(headerAction);
        m_contentScreen.setItems(QVector<ContentItemCompat>());
    }
    m_contentScreen.setCanLoadMore(false);
    m_contentScreen.setStatus(append
        ? QString::fromUtf8("正在加载更多...")
        : QString::fromUtf8("正在加载..."));
    if (BilibiliWbi::requiresSigning(endpoint) &&
        m_wbiMixinKey.isEmpty()) {
        m_networkStage = FetchingWbiKeys;
        const QString navEndpoint = QString::fromLatin1(
            "https://api.bilibili.com/x/web-interface/nav");
        if (!m_transport.startGet(
                QUrl(navEndpoint).toEncoded(), 30000,
                384 * 1024, m_contentCookies)) {
            m_contentScreen.setStatus(QString::fromLatin1("WBI:INIT"));
            m_networkStage = NetworkComplete;
        }
    } else {
        startContentTransport();
    }
    updateGL();
}

void WiliwiliWidget::refreshContent()
{
    if (m_contentEndpoint.isEmpty())
        return;
    m_transport.cancel();
    clearContentImages();
    m_contentAppend = false;
    m_contentCanLoadMore = false;
    m_contentScreen.setCanLoadMore(false);
    if (m_contentMode == HistoryContentMode) {
        m_historyCursorMax = 0;
        m_historyCursorViewAt = 0;
        m_contentEndpoint = historyCursorEndpoint(0, 0);
    } else if (!m_contentPageTemplate.isEmpty()) {
        m_contentPage = 1;
        m_contentEndpoint = m_contentPageTemplate;
        m_contentEndpoint.replace(QString::fromLatin1("{page}"),
                                  QString::fromLatin1("1"));
    }
    m_contentScreen.setItems(QVector<ContentItemCompat>());
    m_contentScreen.setStatus(QString::fromUtf8("正在刷新..."));
    if (BilibiliWbi::requiresSigning(m_contentEndpoint) &&
        m_wbiMixinKey.isEmpty()) {
        m_networkStage = FetchingWbiKeys;
        const QString navEndpoint = QString::fromLatin1(
            "https://api.bilibili.com/x/web-interface/nav");
        if (!m_transport.startGet(
                QUrl(navEndpoint).toEncoded(), 30000,
                384 * 1024, m_contentCookies)) {
            m_contentScreen.setStatus(QString::fromLatin1("WBI:INIT"));
            m_networkStage = NetworkComplete;
        }
    } else {
        startContentTransport();
    }
}

void WiliwiliWidget::loadMoreContent()
{
    if (!m_contentCanLoadMore || m_networkStage != NetworkComplete) {
        return;
    }
    m_transport.cancel();
    if (m_contentMode == HistoryContentMode) {
        if (m_historyCursorMax == 0 || m_historyCursorViewAt == 0)
            return;
        m_contentEndpoint = historyCursorEndpoint(
            m_historyCursorMax, m_historyCursorViewAt);
    } else {
        if (m_contentPageTemplate.isEmpty())
            return;
        ++m_contentPage;
        m_contentEndpoint = m_contentPageTemplate;
        m_contentEndpoint.replace(QString::fromLatin1("{page}"),
                                  QString::number(m_contentPage));
    }
    m_contentAppend = true;
    m_contentCanLoadMore = false;
    m_contentScreen.setCanLoadMore(false);
    m_contentScreen.setStatus(QString::fromUtf8("正在加载更多..."));
    if (BilibiliWbi::requiresSigning(m_contentEndpoint) &&
        m_wbiMixinKey.isEmpty()) {
        m_networkStage = FetchingWbiKeys;
        const QString navEndpoint = QString::fromLatin1(
            "https://api.bilibili.com/x/web-interface/nav");
        if (!m_transport.startGet(
                QUrl(navEndpoint).toEncoded(), 30000,
                384 * 1024, m_contentCookies)) {
            m_contentScreen.setStatus(QString::fromUtf8("无法准备加载更多"));
            m_networkStage = NetworkComplete;
        }
    } else {
        startContentTransport();
    }
}

void WiliwiliWidget::startContentTransport()
{
    QString endpoint = m_contentEndpoint;
    if (BilibiliWbi::requiresSigning(endpoint) &&
        !m_wbiMixinKey.isEmpty()) {
        endpoint = BilibiliWbi::signUrl(endpoint, m_wbiMixinKey);
    }
    m_networkStage = FetchingContent;
    const bool isSearchRequest =
        (m_contentMode == SearchContentMode ||
         m_contentMode == SearchUsersContentMode);
    const NativeTransport::RequestProfile profile = isSearchRequest
            ? NativeTransport::WebSearchRequestProfile
            : NativeTransport::DefaultRequestProfile;
    // signUrl() produces an ASCII, already-percent-encoded canonical query.
    // Passing that query through QUrl(QString).toEncoded() on Qt 4.7 can
    // turn the '%' in a non-ASCII keyword into '%25' again. The server then
    // searches for the literal encoded byte sequence instead of the entered
    // Chinese text, which looks like an unrelated-result response. Keep the
    // signed search URL byte-exact through the Symbian HTTP stack.
    const QByteArray wireEndpoint = isSearchRequest
        ? endpoint.toLatin1() : QUrl(endpoint).toEncoded();
    if (isSearchRequest) {
        const int marker = wireEndpoint.indexOf("keyword=");
        const int valueStart = marker < 0 ? -1 : marker + 8;
        const int valueEnd = valueStart < 0
            ? -1 : wireEndpoint.indexOf('&', valueStart);
        const QByteArray wireKeyword = valueStart < 0
            ? QByteArray() : wireEndpoint.mid(
                valueStart,
                valueEnd < 0 ? -1 : valueEnd - valueStart);
        const QByteArray expectedHash = QCryptographicHash::hash(
            m_searchKeyword.toUtf8(), QCryptographicHash::Md5)
            .toHex().left(8);
        const QByteArray wireHash = QCryptographicHash::hash(
            QUrl::fromPercentEncoding(wireKeyword).toUtf8(),
            QCryptographicHash::Md5).toHex().left(8);
        qDebug() << "WW:SEARCH_WIRE"
                 << (wireHash == expectedHash)
                 << wireKeyword.contains("%25")
                 << wireKeyword.size();
    }
    if (!m_transport.startGet(
            wireEndpoint, 30000,
            1024 * 1024, m_contentCookies, profile)) {
        m_contentScreen.setStatus(QString::fromLatin1("CONTENT:INIT"));
        m_networkStage = NetworkComplete;
    }
}

void WiliwiliWidget::requestComments()
{
    const VideoDetailCompat &video = m_detailScreen.video();
    if (video.aid == 0)
        return;
    m_contentSubjectId = video.aid;
    m_commentLegacyFallback = false;
    const QString endpoint = QString::fromLatin1(
        "https://api.bilibili.com/x/v2/reply/main"
        "?mode=%1&next=0&oid=%2&plat=1&type=1")
        .arg(m_commentMode).arg(video.aid);
    requestContent(
        CommentsContentMode,
        QString::fromUtf8("视频评论"),
        m_commentMode == 3 ? QString::fromUtf8("当前：最热评论")
                           : QString::fromUtf8("当前：最新评论"),
        endpoint,
        LoginSession::cookieHeader(),
        m_commentMode == 3 ? QString::fromUtf8("切最新")
                           : QString::fromUtf8("切最热"));
    m_contentScreen.setSecondaryHeaderAction(
        LoginSession::isLoggedIn() ? QString::fromUtf8("写评论")
                                   : QString());
}

void WiliwiliWidget::requestCommentReplies(
    quint64 rootRpid, const QString &author)
{
    const VideoDetailCompat &video = m_detailScreen.video();
    if (video.aid == 0 || rootRpid == 0)
        return;
    m_contentSubjectId = rootRpid;
    m_commentLegacyFallback = false;
    const QString endpoint = QString::fromLatin1(
        "https://api.bilibili.com/x/v2/reply/detail"
        "?csrf=%1&next=0&oid=%2&root=%3&type=1")
        .arg(QString::fromLatin1(QUrl::toPercentEncoding(
                 QString::fromLatin1(
                     LoginSession::cookieValue("bili_jct")))))
        .arg(video.aid).arg(rootRpid);
    requestContent(
        CommentRepliesContentMode,
        QString::fromUtf8("评论回复"),
        author.left(18), endpoint,
        LoginSession::cookieHeader());
}

void WiliwiliWidget::requestUserVideos(quint64 mid, const QString &name)
{
    if (mid == 0)
        return;
    m_contentSubjectId = mid;
    const quint64 ownMid = QString::fromLatin1(
        LoginSession::cookieValue("DedeUserID")).toULongLong();
    const QString pageTemplate = QString::fromLatin1(
        "https://api.bilibili.com/x/space/arc/search"
        "?pn={page}&ps=20&order=pubdate&mid=%1").arg(mid);
    QString endpoint = pageTemplate;
    endpoint.replace(QString::fromLatin1("{page}"),
                     QString::fromLatin1("1"));
    requestContent(
        UserVideosContentMode,
        name.isEmpty() ? QString::fromUtf8("用户投稿") : name,
        QString::fromLatin1("UID %1").arg(mid),
        endpoint,
        LoginSession::requestCookieHeader(),
        LoginSession::isLoggedIn() && mid != ownMid
            ? QString::fromUtf8("关注") : QString(),
        false,
        pageTemplate,
        1,
        20);
}

void WiliwiliWidget::requestAccountContent(int mode)
{
    if (!LoginSession::isLoggedIn()) {
        selectSection(NavigationRail::AccountSection);
        return;
    }
    const QByteArray cookies = LoginSession::requestCookieHeader();
    const QString mid = QString::fromLatin1(
        LoginSession::cookieValue("DedeUserID"));
    if (mode == HistoryContentMode) {
        m_historyCursorMax = 0;
        m_historyCursorViewAt = 0;
        requestContent(mode, QString::fromUtf8("历史记录"),
            QString::fromUtf8("最近观看"),
            historyCursorEndpoint(0, 0), cookies);
    } else if (mode == FavoritesContentMode) {
        const QString pageTemplate = QString::fromLatin1(
            "https://api.bilibili.com/x/v3/fav/folder/created/list"
            "?pn={page}&ps=20&up_mid=%1").arg(mid);
        QString endpoint = pageTemplate;
        endpoint.replace(QString::fromLatin1("{page}"),
                         QString::fromLatin1("1"));
        requestContent(mode, QString::fromUtf8("我的收藏"),
            QString::fromUtf8("收藏夹"),
            endpoint, cookies, QString(), false, pageTemplate, 1, 20);
    } else if (mode == WatchLaterContentMode) {
        requestContent(mode, QString::fromUtf8("稍后再看"),
            QString::fromUtf8("待看列表"),
            QString::fromLatin1(
                "https://api.bilibili.com/x/v2/history/toview/web"), cookies);
    } else if (mode == UserVideosContentMode) {
        requestUserVideos(mid.toULongLong(), QString::fromUtf8("我的投稿"));
    } else if (mode == FollowingContentMode) {
        const QString pageTemplate = QString::fromLatin1(
            "https://api.bilibili.com/x/relation/followings"
            "?vmid=%1&pn={page}&ps=20&order=desc").arg(mid);
        QString endpoint = pageTemplate;
        endpoint.replace(QString::fromLatin1("{page}"),
                         QString::fromLatin1("1"));
        requestContent(mode, QString::fromUtf8("我的关注"),
            QString::fromUtf8("关注的 UP 主"),
            endpoint, cookies, QString(), false, pageTemplate, 1, 20);
    }
}

void WiliwiliWidget::startPostAction(
    int action,
    const QString &endpoint,
    const QByteArray &formBody,
    const QString &progressText)
{
    if (!LoginSession::isLoggedIn()) {
        if (m_currentScreen == ContentScreenView)
            m_contentScreen.setStatus(QString::fromUtf8("请先登录"));
        else
            m_detailScreen.setNetworkStatus(QString::fromUtf8("请先登录后操作"));
        return;
    }
    const QByteArray csrf = LoginSession::cookieValue("bili_jct");
    if (csrf.isEmpty()) {
        if (m_currentScreen == ContentScreenView)
            m_contentScreen.setStatus(QString::fromUtf8("登录凭据缺少 CSRF，请重新登录"));
        else
            m_detailScreen.setNetworkStatus(
                QString::fromUtf8("凭据缺少 bili_jct，请重新登录"));
        return;
    }

    QByteArray completedForm = formBody;
    appendFormField(
        &completedForm, "csrf", QString::fromLatin1(csrf));
    m_transport.cancel();
    m_pendingAction = static_cast<PendingAction>(action);
    m_networkStage = PostingAction;
    if (m_currentScreen == ContentScreenView)
        m_contentScreen.setStatus(progressText);
    else
        m_detailScreen.setNetworkStatus(progressText);
    if (!m_transport.startPost(
            QUrl(endpoint).toEncoded(), completedForm,
            30000, 256 * 1024, LoginSession::cookieHeader())) {
        m_networkStage = NetworkComplete;
        m_pendingAction = NoPendingAction;
        if (m_currentScreen == ContentScreenView)
            m_contentScreen.setStatus(QString::fromLatin1("POST:INIT"));
        else
            m_detailScreen.setNetworkStatus(QString::fromLatin1("POST:INIT"));
    }
}

void WiliwiliWidget::postVideoAction(int action)
{
    const VideoDetailCompat &video = m_detailScreen.video();
    if (video.aid == 0) {
        m_detailScreen.setNetworkStatus(QString::fromUtf8("视频 AV 号不可用"));
        return;
    }
    if (action == DetailScreen::FavoriteAction) {
        requestFavoriteFolderForAction();
        return;
    }

    QByteArray form;
    appendFormField(&form, "aid", QString::number(video.aid));
    if (action == DetailScreen::LikeAction) {
        appendFormField(&form, "like", QString::fromLatin1("1"));
        startPostAction(
            LikePendingAction,
            QString::fromLatin1(
                "https://api.bilibili.com/x/web-interface/archive/like"),
            form, QString::fromUtf8("正在点赞..."));
    } else if (action == DetailScreen::CoinAction) {
        appendFormField(&form, "multiply", QString::fromLatin1("1"));
        appendFormField(&form, "select_like", QString::fromLatin1("0"));
        startPostAction(
            CoinPendingAction,
            QString::fromLatin1(
                "https://api.bilibili.com/x/web-interface/coin/add"),
            form, QString::fromUtf8("正在投币..."));
    } else if (action == DetailScreen::WatchLaterAction) {
        startPostAction(
            WatchLaterPendingAction,
            QString::fromLatin1(
                "https://api.bilibili.com/x/v2/history/toview/add"),
            form, QString::fromUtf8("正在加入稍后再看..."));
    }
}

void WiliwiliWidget::requestFavoriteFolderForAction()
{
    if (!LoginSession::isLoggedIn()) {
        m_detailScreen.setNetworkStatus(QString::fromUtf8("请先登录后收藏"));
        return;
    }
    const VideoDetailCompat &video = m_detailScreen.video();
    const QByteArray mid = LoginSession::cookieValue("DedeUserID");
    if (video.aid == 0 || mid.isEmpty()) {
        m_detailScreen.setNetworkStatus(QString::fromUtf8("收藏参数不完整"));
        return;
    }
    m_transport.cancel();
    m_pendingAction = FavoritePendingAction;
    m_networkStage = FetchingFavoriteForAction;
    m_detailScreen.setNetworkStatus(QString::fromUtf8("正在读取收藏夹..."));
    const QString endpoint = QString::fromLatin1(
        "https://api.bilibili.com/x/v3/fav/folder/created/list-all"
        "?up_mid=%1&type=2&rid=%2")
        .arg(QString::fromLatin1(mid)).arg(video.aid);
    if (!m_transport.startGet(
            QUrl(endpoint).toEncoded(), 30000, 256 * 1024,
            LoginSession::cookieHeader())) {
        m_networkStage = NetworkComplete;
        m_pendingAction = NoPendingAction;
        m_detailScreen.setNetworkStatus(QString::fromLatin1("FAV:INIT"));
    }
}

void WiliwiliWidget::promptComment()
{
    if (!LoginSession::isLoggedIn() ||
        m_contentMode != CommentsContentMode ||
        m_contentSubjectId == 0) {
        m_contentScreen.setStatus(QString::fromUtf8("请先登录后评论"));
        return;
    }
    bool accepted = false;
    const QString comment = QInputDialog::getText(
        this,
        QString::fromUtf8("发表评论"),
        QString::fromUtf8("评论内容"),
        QLineEdit::Normal,
        QString(),
        &accepted).trimmed();
    scheduleForegroundRestore();
    if (!accepted || comment.isEmpty())
        return;
    QByteArray form;
    appendFormField(&form, "oid", QString::number(m_contentSubjectId));
    appendFormField(&form, "type", QString::fromLatin1("1"));
    appendFormField(&form, "message", comment.left(1000));
    appendFormField(&form, "plat", QString::fromLatin1("1"));
    startPostAction(
        CommentPendingAction,
        QString::fromLatin1("https://api.bilibili.com/x/v2/reply/add"),
        form, QString::fromUtf8("正在发送评论..."));
}

void WiliwiliWidget::requestChatMessages(
    quint64 talkerId, const QString &name)
{
    if (talkerId == 0 || !LoginSession::isLoggedIn())
        return;
    m_contentSubjectId = talkerId;
    const QString endpoint = QString::fromLatin1(
        "https://api.vc.bilibili.com/svr_sync/v1/svr_sync/fetch_session_msgs"
        "?sender_device_id=1&talker_id=%1&session_type=1"
        "&begin_seqno=0&size=30&mobi_app=web").arg(talkerId);
    requestContent(
        ChatMessagesContentMode,
        name.isEmpty() ? QString::fromUtf8("私信会话") : name,
        QString::fromLatin1("UID %1").arg(talkerId),
        endpoint,
        LoginSession::cookieHeader(),
        QString::fromUtf8("发私信"));
}

void WiliwiliWidget::promptPrivateMessage()
{
    if (!LoginSession::isLoggedIn() ||
        m_contentMode != ChatMessagesContentMode ||
        m_contentSubjectId == 0)
        return;
    bool accepted = false;
    const QString message = QInputDialog::getText(
        this,
        QString::fromUtf8("发送私信"),
        QString::fromUtf8("消息内容"),
        QLineEdit::Normal,
        QString(),
        &accepted).trimmed();
    scheduleForegroundRestore();
    if (!accepted || message.isEmpty())
        return;
    const QByteArray sender = LoginSession::cookieValue("DedeUserID");
    if (sender.isEmpty()) {
        m_contentScreen.setStatus(QString::fromUtf8("登录凭据缺少 UID"));
        return;
    }
    const QString receiver = QString::number(m_contentSubjectId);
    QByteArray form;
    appendFormField(&form, "msg[msg_type]", QString::fromLatin1("1"));
    appendFormField(&form, "msg[content]", message.left(1000));
    appendFormField(&form, "msg[sender_uid]", QString::fromLatin1(sender));
    appendFormField(&form, "msg[receiver_id]", receiver);
    appendFormField(&form, "msg[receiver_type]", QString::fromLatin1("1"));
    appendFormField(&form, "msg[msg_status]", QString::fromLatin1("0"));
    appendFormField(&form, "msg[timestamp]", QString::number(
        QDateTime::currentDateTime().toTime_t()));
    appendFormField(&form, "msg[dev_id]", QString::fromLatin1("0"));
    appendFormField(&form, "msg[new_face_version]", QString::fromLatin1("1"));
    const QString endpoint = QString::fromLatin1(
        "https://api.vc.bilibili.com/web_im/v1/web_im/send_msg"
        "?w_sender_uid=%1&w_receiver_id=%2")
        .arg(QString::fromLatin1(sender)).arg(receiver);
    startPostAction(
        ChatMessagePendingAction,
        endpoint,
        form,
        QString::fromUtf8("正在发送私信..."));
}

void WiliwiliWidget::postFollowUser()
{
    if (m_contentMode != UserVideosContentMode ||
        m_contentSubjectId == 0)
        return;
    QByteArray form;
    appendFormField(&form, "fid", QString::number(m_contentSubjectId));
    appendFormField(&form, "act", QString::fromLatin1("1"));
    appendFormField(&form, "re_src", QString::fromLatin1("11"));
    startPostAction(
        FollowPendingAction,
        QString::fromLatin1("https://api.bilibili.com/x/relation/modify"),
        form, QString::fromUtf8("正在关注..."));
}

void WiliwiliWidget::openContentItem(int index)
{
    const QVector<ContentItemCompat> &items = m_contentScreen.items();
    if (index < 0 || index >= items.size())
        return;
    const ContentItemCompat item = items.at(index);
    if (m_contentMode == CommentsContentMode &&
        item.id == QString::fromLatin1("comment-root") &&
        item.numericId != 0) {
        NavigationEntry repliesEntry;
        repliesEntry.screen = ContentScreenView;
        repliesEntry.contentMode = CommentsContentMode;
        pushNavigation(repliesEntry);
        requestCommentReplies(
            item.numericId, item.subtitle.section(' ', 0, 0));
    } else if (m_contentMode == CommentRepliesContentMode) {
        // Keep taps inside the thread; Back reloads the root-comment list.
        return;
    } else if (item.kind == VideoContentItem) {
        startVideoDetail(item);
    } else if (item.kind == FolderContentItem &&
               item.numericId != 0) {
        NavigationEntry folderEntry;
        folderEntry.screen = ContentScreenView;
        folderEntry.contentMode = FavoritesContentMode;
        pushNavigation(folderEntry);
        const QString pageTemplate = QString::fromLatin1(
            "https://api.bilibili.com/x/v3/fav/resource/list"
            "?media_id=%1&pn={page}&ps=20&order=mtime&type=0")
            .arg(item.numericId);
        QString endpoint = pageTemplate;
        endpoint.replace(QString::fromLatin1("{page}"),
                         QString::fromLatin1("1"));
        requestContent(
            FavoriteVideosContentMode,
            item.title,
            QString::fromUtf8("收藏夹内容"),
            endpoint,
            LoginSession::requestCookieHeader(),
            QString(), false, pageTemplate, 1, 20);
    } else if (item.kind == UserContentItem && item.numericId != 0) {
        NavigationEntry userEntry;
        userEntry.screen = ContentScreenView;
        pushNavigation(userEntry);
        requestUserVideos(item.numericId, item.title);
    } else if (item.kind == TextContentItem && item.secondaryId != 0) {
        NavigationEntry authorEntry;
        authorEntry.screen = ContentScreenView;
        pushNavigation(authorEntry);
        requestUserVideos(item.secondaryId, item.subtitle.section(' ', 0, 0));
    }
}

void WiliwiliWidget::clearAppCache()
{
    // The only on-disk cache in this port is the temporary local MP4 used by
    // OpenFileL playback and the bounded download fallback. Normal close
    // removes it; this option cleans leftovers from crashes/force exits.
    qint64 freedBytes = 0;
    const QString cachePath = QDir::tempPath() +
        QString::fromLatin1("/wiliwili_player_cache.mp4");
    const QFileInfo cacheInfo(cachePath);
    if (cacheInfo.exists() && cacheInfo.isFile()) {
        freedBytes = cacheInfo.size();
        QFile::remove(cachePath);
        qDebug() << "WW:CACHE_CLEARED" << freedBytes << cachePath;
    }
    m_sectionScreen.setStatus(
        freedBytes > 0
            ? QString::fromUtf8("缓存已清理（%1 KB）")
                  .arg(freedBytes / 1024)
            : QString::fromUtf8("无缓存文件"));
    updateGL();
}

QString WiliwiliWidget::thumbnailUrl(const QString &source) const
{
    QString result = source.trimmed();
    if (result.startsWith(QString::fromLatin1("//")))
        result.prepend(QString::fromLatin1("https:"));
    else if (result.startsWith(QString::fromLatin1("http://")))
        result.replace(0, 7, QString::fromLatin1("https://"));
    if (!result.startsWith(QString::fromLatin1("https://")))
        return QString();
    result += QString::fromLatin1("@224w_126h_1c.jpg");
    return result;
}

QString WiliwiliWidget::probeResultText(const QString &tag) const
{
    const NativeTransport::Result &result = m_transport.result();
    if (result.httpStatus > 0)
        return QString::fromLatin1("%1:H%2").arg(tag).arg(result.httpStatus);
    if (m_transport.state() == NativeTransport::TimedOut)
        return tag + QString::fromLatin1(":TO");
    if (m_transport.state() == NativeTransport::ResponseTooLarge)
        return tag + QString::fromLatin1(":BIG");
    return QString::fromLatin1("%1:E%2")
        .arg(tag)
        .arg(result.networkError);
}

void WiliwiliWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_currentScreen == DetailScreenView) {
        m_detailScreen.pointerPress(event->pos());
    } else if (m_currentScreen == ContentScreenView) {
        m_contentScreen.pointerPress(event->pos());
    } else if (m_navigation.contains(event->pos())) {
        m_navigation.pointerPress(event->pos());
    } else if (m_navigation.selected() == NavigationRail::HomeSection) {
        m_homeScreen.pointerPress(event->pos());
    } else if (m_navigation.selected() == NavigationRail::AccountSection) {
        m_loginScreen.pointerPress(event->pos());
    } else {
        m_sectionScreen.pointerPress(event->pos());
    }
    event->accept();
}

void WiliwiliWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_currentScreen == DetailScreenView) {
        m_detailScreen.pointerMove(event->pos());
    } else if (m_currentScreen == ContentScreenView) {
        m_contentScreen.pointerMove(event->pos());
    } else if (m_navigation.selected() == NavigationRail::HomeSection) {
        m_homeScreen.pointerMove(event->pos());
    } else if (m_navigation.selected() != NavigationRail::AccountSection) {
        m_sectionScreen.pointerMove(event->pos());
    }
    updateGL();
    event->accept();
}

void WiliwiliWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_currentScreen == DetailScreenView) {
        const DetailScreen::Action action =
            m_detailScreen.pointerRelease(event->pos());
        if (action == DetailScreen::BackAction) {
            navigateBack();
        } else if (action == DetailScreen::PlayAction) {
            startVideoPlayback();
        } else if (action == DetailScreen::PageAction) {
            m_detailScreen.cyclePage();
        } else if (action == DetailScreen::OwnerAction) {
            const VideoDetailCompat &video = m_detailScreen.video();
            NavigationEntry ownerEntry;
            ownerEntry.screen = DetailScreenView;
            pushNavigation(ownerEntry);
            requestUserVideos(video.owner.mid, video.owner.name);
        } else if (action == DetailScreen::CommentsAction) {
            NavigationEntry commentsEntry;
            commentsEntry.screen = DetailScreenView;
            pushNavigation(commentsEntry);
            requestComments();
        } else if (action == DetailScreen::LikeAction ||
                   action == DetailScreen::CoinAction ||
                   action == DetailScreen::FavoriteAction ||
                   action == DetailScreen::WatchLaterAction) {
            postVideoAction(action);
        }
    } else if (m_currentScreen == ContentScreenView) {
        const int action = m_contentScreen.pointerRelease(event->pos());
        if (action == ContentScreen::BackActionValue) {
            navigateBack();
        } else if (action == ContentScreen::RefreshActionValue) {
            refreshContent();
        } else if (action == ContentScreen::LoadMoreActionValue) {
            loadMoreContent();
        } else if (action == ContentScreen::HeaderActionValue) {
            if (m_contentMode == SearchContentMode ||
                m_contentMode == SearchUsersContentMode)
                scheduleUiAction(SearchPendingUiAction);
            else if (m_contentMode == CommentsContentMode) {
                m_commentMode = m_commentMode == 3 ? 2 : 3;
                requestComments();
            }
            else if (m_contentMode == UserVideosContentMode)
                postFollowUser();
            else if (m_contentMode == ChatMessagesContentMode)
                scheduleUiAction(PrivateMessagePendingUiAction);
        } else if (action ==
                   ContentScreen::SecondaryHeaderActionValue) {
            if ((m_contentMode == SearchContentMode ||
                 m_contentMode == SearchUsersContentMode) &&
                !m_searchKeyword.isEmpty()) {
                submitSearch((m_contentMode == SearchContentMode
                              ? QString::fromLatin1("@") : QString()) +
                             m_searchKeyword, false);
            } else if (m_contentMode == CommentsContentMode)
                scheduleUiAction(CommentPendingUiAction);
        } else if (action >= 0) {
            openContentItem(action);
        }
    } else {
        const NavigationRail::Section section =
            m_navigation.pointerRelease(event->pos());
        if (section != NavigationRail::NoSection) {
            selectSection(section);
        } else if (m_navigation.selected() == NavigationRail::HomeSection) {
            const int activatedIndex =
                m_homeScreen.pointerRelease(event->pos());
            if (HomeScreen::isRefreshAction(activatedIndex)) {
                m_homeScreen.setNetworkStatus(
                    QString::fromLatin1("REFRESH"));
                startBilibiliFeed(false, true);
            } else if (HomeScreen::isLoadMoreAction(activatedIndex)) {
                startBilibiliFeed(true);
            } else if (HomeScreen::isSearchAction(activatedIndex)) {
                scheduleUiAction(SearchPendingUiAction);
            } else if (HomeScreen::isCategoryAction(activatedIndex)) {
                m_homeFreshIndex = 0;
                m_homeSessionChanged = false;
                startBilibiliFeed();
            } else if (activatedIndex >= 0) {
                startVideoDetail(activatedIndex);
            }
        } else if (m_navigation.selected() == NavigationRail::AccountSection) {
            const LoginScreen::Action action =
                m_loginScreen.pointerRelease(event->pos());
            if (action == LoginScreen::RefreshQrAction) {
                startQrLogin();
            } else if (action == LoginScreen::LogoutAction) {
                LoginSession::clear();
                m_homeSessionChanged = true;
                m_homeFreshIndex = 0;
                m_sectionScreen.setLoggedIn(false);
                m_loginScreen.clearProfile();
                startQrLogin();
            } else if (action == LoginScreen::HistoryAction) {
                NavigationEntry accountEntry;
                accountEntry.screen = TopLevelScreenView;
                accountEntry.restoreHome = false;
                pushNavigation(accountEntry);
                requestAccountContent(HistoryContentMode);
            } else if (action == LoginScreen::FavoritesAction) {
                NavigationEntry accountEntry;
                accountEntry.screen = TopLevelScreenView;
                accountEntry.restoreHome = false;
                pushNavigation(accountEntry);
                requestAccountContent(FavoritesContentMode);
            } else if (action == LoginScreen::WatchLaterAction) {
                NavigationEntry accountEntry;
                accountEntry.screen = TopLevelScreenView;
                accountEntry.restoreHome = false;
                pushNavigation(accountEntry);
                requestAccountContent(WatchLaterContentMode);
            } else if (action == LoginScreen::MyVideosAction) {
                NavigationEntry accountEntry;
                accountEntry.screen = TopLevelScreenView;
                accountEntry.restoreHome = false;
                pushNavigation(accountEntry);
                requestAccountContent(UserVideosContentMode);
            } else if (action == LoginScreen::FollowingAction) {
                NavigationEntry accountEntry;
                accountEntry.screen = TopLevelScreenView;
                accountEntry.restoreHome = false;
                pushNavigation(accountEntry);
                requestAccountContent(FollowingContentMode);
            }
        } else {
            const SectionScreen::Action action =
                m_sectionScreen.pointerRelease(event->pos());
            if (action == SectionScreen::ExitApplicationAction)
                shutdownAndQuit();
            else if (action == SectionScreen::NetworkTestAction)
                requestNetworkDiagnostic();
            else if (action == SectionScreen::ToggleImagesAction)
                toggleContentImages();
            else if (action == SectionScreen::OpenPlaybackModeAction) {
                m_sectionScreen.setPreferencePage(
                    SectionScreen::PlaybackPreferencePage);
                updateGL();
            } else if (action == SectionScreen::OpenDecoderModeAction) {
                m_sectionScreen.setPreferencePage(
                    SectionScreen::DecoderPreferencePage);
                updateGL();
            } else if (action >= SectionScreen::SelectUrlStreamingAction &&
                       action <=
                           SectionScreen::SelectDownloadThenPlaybackAction) {
                setPlaybackMode(
                    static_cast<int>(action) -
                    static_cast<int>(
                        SectionScreen::SelectUrlStreamingAction));
                updateGL();
            } else if (action >=
                           SectionScreen::SelectAutomaticDecoderAction &&
                       action <=
                           SectionScreen::SelectSoftwareOnlyDecoderAction) {
                setDecoderMode(
                    static_cast<int>(action) -
                    static_cast<int>(
                        SectionScreen::SelectAutomaticDecoderAction));
                updateGL();
            } else if (action == SectionScreen::PreferenceBackAction) {
                m_sectionScreen.setPreferencePage(
                    SectionScreen::NoPreferencePage);
                updateGL();
            } else if (action == SectionScreen::ClearCacheAction)
                clearAppCache();
            else if (action == SectionScreen::AboutAction) {
                m_sectionScreen.setAboutVisible(true);
                updateGL();
            } else if (action == SectionScreen::AboutBackAction) {
                m_sectionScreen.setAboutVisible(false);
                updateGL();
            } else if (action == SectionScreen::RefreshAction) {
                if (m_navigation.selected() == NavigationRail::DynamicSection)
                    requestDynamicFeed(false);
                else if (m_navigation.selected() ==
                         NavigationRail::MessagesSection)
                    requestMessages(m_messageType, false);
            } else if (action == SectionScreen::LoadMoreAction) {
                loadMoreSection();
            } else if (action == SectionScreen::ReplyTabAction)
                requestMessages(0);
            else if (action == SectionScreen::AtTabAction)
                requestMessages(1);
            else if (action == SectionScreen::LikeTabAction)
                requestMessages(2);
            else if (action == SectionScreen::ChatTabAction)
                requestMessages(3);
            else if (SectionScreen::isItemAction(action)) {
                const int itemIndex = SectionScreen::itemIndex(action);
                openSectionItem(itemIndex);
            }
        }
    }
    updateGL();
    event->accept();
}

void WiliwiliWidget::keyPressEvent(QKeyEvent *event)
{
    // The GLES software path intentionally hides the native player controller
    // so the already-existing main EGL surface can show through. Focus may
    // consequently return to this QGLWidget; keep all playback keys routed to
    // the retained (hidden, not destroyed) controller.
    if (playerOwnsForeground() && m_videoPlayer) {
        m_videoPlayer->handleForegroundKey(event);
        return;
    }
    const bool isBackKey =
        event->key() == Qt::Key_Back ||
        event->key() == Qt::Key_Escape ||
        event->key() == Qt::Key_Backspace;
    if (isBackKey) {
        if (m_currentScreen == DetailScreenView) {
            navigateBack();
            updateGL();
        } else if (m_currentScreen == ContentScreenView) {
            navigateBack();
            updateGL();
        } else if (m_navigation.selected() ==
                       NavigationRail::SettingsSection &&
                   m_sectionScreen.preferencePageVisible()) {
            m_sectionScreen.setPreferencePage(
                SectionScreen::NoPreferencePage);
            updateGL();
        } else if (m_navigation.selected() ==
                       NavigationRail::SettingsSection &&
                   m_sectionScreen.aboutVisible()) {
            m_sectionScreen.setAboutVisible(false);
            updateGL();
        } else if (m_navigation.selected() != NavigationRail::HomeSection) {
            selectSection(NavigationRail::HomeSection);
        } else {
            shutdownAndQuit();
        }
        event->accept();
        return;
    }
    QGLWidget::keyPressEvent(event);
}

void WiliwiliWidget::closeEvent(QCloseEvent *event)
{
    m_shuttingDown = true;
    m_transport.cancel();
    event->accept();
    QApplication::quit();
}

bool WiliwiliWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_searchEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return ||
            keyEvent->key() == Qt::Key_Enter ||
            keyEvent->key() == Qt::Key_Select) {
            submitSearch(m_searchEdit->text());
            keyEvent->accept();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Back ||
            keyEvent->key() == Qt::Key_Escape) {
            hideSearchEditor();
            keyEvent->accept();
            return true;
        }
    }
    if (!m_shuttingDown) {
        const bool searchBelongsHere =
            m_currentScreen == TopLevelScreenView &&
            m_navigation.selected() == NavigationRail::HomeSection;
        if (m_searchEdit && m_searchEdit->isVisible() &&
            !searchBelongsHere) {
            hideSearchEditor();
        }
        const bool editingSearch = searchBelongsHere &&
            m_searchEdit && m_searchEdit->isVisible();
        const bool applicationActivated =
            event->type() == QEvent::ApplicationActivate;
        const bool mainWindowActivated =
            watched == this && event->type() == QEvent::WindowActivate;
        const bool applicationDeactivated =
            event->type() == QEvent::ApplicationDeactivate;
        if (playerOwnsForeground() &&
            (applicationActivated || mainWindowActivated ||
             applicationDeactivated)) {
            suspendForegroundRestoreForPlayer();
            qDebug() << (applicationDeactivated
                ? "WW:PLAYER_APPLICATION_DEACTIVATE"
                : "WW:PLAYER_APPLICATION_ACTIVATE");
            return QGLWidget::eventFilter(watched, event);
        }
        if (applicationActivated ||
            (watched == this &&
             event->type() == QEvent::WindowActivate)) {
            const bool wasActivated = m_hasActivated;
            qDebug() << "WW:APPLICATION_ACTIVATE";
            m_hasActivated = true;
            if (!wasActivated && !editingSearch)
                scheduleForegroundRestore();
            if (editingSearch) {
                m_searchEdit->raise();
                m_searchEdit->activateWindow();
                QApplication::setActiveWindow(m_searchEdit);
                m_searchEdit->setFocus(Qt::ActiveWindowFocusReason);
                qDebug() << "WW:SEARCH_FOCUS_RETAINED"
                         << m_searchEdit->hasFocus();
            }
            updateGL();
        } else if (event->type() == QEvent::ApplicationDeactivate) {
            qDebug() << "WW:APPLICATION_DEACTIVATE";
            if (!editingSearch)
                m_hasActivated = false;
        }
    }
    return QGLWidget::eventFilter(watched, event);
}

void WiliwiliWidget::bringApplicationToForeground()
{
    if (playerOwnsForeground()) {
        qDebug() << "WW:MAIN_FOREGROUND_BRING_BLOCKED";
        return;
    }
#ifdef Q_OS_SYMBIAN
    CEikonEnv *environment = CEikonEnv::Static();
    if (environment) {
        CAknAppUi *appUi = static_cast<CAknAppUi *>(environment->EikAppUi());
        if (appUi) {
            CEikStatusPane *statusPane = appUi->StatusPane();
            if (statusPane)
                statusPane->MakeVisible(EFalse);
            CEikButtonGroupContainer *buttonGroup = appUi->Cba();
            if (buttonGroup)
                buttonGroup->MakeVisible(EFalse);
            qDebug() << "WW:AVKON_CHROME_HIDDEN";
        }
    }
#endif
    m_chromeHidden = true;
    // Keep the pane objects alive, but let Qt's dynamic fullscreen state hide
    // them and reclaim the exact 360x640 work area. This is the same path that
    // completed 20 portrait/landscape probe cycles on the Nokia 603.
    showFullScreen();
    if (m_searchEdit && m_searchEdit->isVisible()) {
        m_searchEdit->raise();
        m_searchEdit->setFocus(Qt::OtherFocusReason);
    } else {
        setFocus(Qt::ActiveWindowFocusReason);
    }
}

bool WiliwiliWidget::playerOwnsForeground() const
{
#ifdef WILIWILI_ENABLE_APP_LANDSCAPE_WINDOW_PROBE
    if (m_landscapeWindowProbeActive)
        return true;
#endif
    return m_videoPlayer && m_videoPlayer->ownsForeground();
}

#ifdef WILIWILI_ENABLE_APP_LANDSCAPE_WINDOW_PROBE
void WiliwiliWidget::setLandscapeWindowProbeActive(bool active)
{
    if (m_landscapeWindowProbeActive == active)
        return;
    m_landscapeWindowProbeActive = active;
    if (active) {
        suspendForegroundRestoreForPlayer();
        hideSearchEditor();
    } else {
        m_hasActivated = false;
    }
    qDebug() << "WW:APP_LANDSCAPE_PROBE_HOST_ACTIVE" << active;
}
#endif

void WiliwiliWidget::suspendForegroundRestoreForPlayer()
{
    if (m_foregroundTimerId) {
        killTimer(m_foregroundTimerId);
        m_foregroundTimerId = 0;
        qDebug() << "WW:MAIN_FOREGROUND_SUSPENDED";
    }
    m_foregroundAttemptCount = 0;
}

void WiliwiliWidget::scheduleForegroundRestore()
{
    if (playerOwnsForeground()) {
        qDebug() << "WW:MAIN_FOREGROUND_SCHEDULE_BLOCKED";
        return;
    }
    m_foregroundAttemptCount = 0;
    m_chromeHidden = false;
    if (!m_foregroundTimerId)
        m_foregroundTimerId = startTimer(120);
}

void WiliwiliWidget::scheduleUiAction(int action)
{
    if (m_shuttingDown || m_uiActionTimerId)
        return;
    m_pendingUiAction = static_cast<PendingUiAction>(action);
    // Never enter a native modal dialog from QGLWidget::mouseReleaseEvent.
    // Symbian's Qt 4 window integration can otherwise destroy/recreate the
    // EGL surface while the pointer callback is still on the stack.
    m_uiActionTimerId = startTimer(1);
}

void WiliwiliWidget::shutdownAndQuit()
{
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    qDebug() << "WW:SHUTDOWN";
    m_transport.cancel();
    close();
    QApplication::quit();
}

} // namespace wiliwili
