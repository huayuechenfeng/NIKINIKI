#ifndef WILIWILI_SYMBIAN_WIDGET_H
#define WILIWILI_SYMBIAN_WIDGET_H

#include <QtCore/QtGlobal>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QTime>
#include <QtOpenGL/QGLWidget>

#include "platform/platform_metrics.h"
#include "platform/yuv420_frame.h"
#include "network/native_transport.h"
#include "model/playback_source.h"
#include "ui/detail_screen.h"
#include "ui/content_screen.h"
#include "ui/home_screen.h"
#include "ui/login_screen.h"
#include "ui/navigation_rail.h"
#include "ui/section_screen.h"
#include "ui/video_player_widget.h"

class QMouseEvent;
class QKeyEvent;
class QCloseEvent;
class QEvent;
class QFile;
class QObject;
class QTimerEvent;
class QUrl;
class QLineEdit;
class QPushButton;
struct NVGcontext;

namespace wiliwili {

class WiliwiliWidget : public QGLWidget, public VideoPlayerDelegate
{
public:
    explicit WiliwiliWidget(QWidget *parent = 0);
    virtual ~WiliwiliWidget();
    virtual void updateGL();
#ifdef WILIWILI_ENABLE_APP_LANDSCAPE_WINDOW_PROBE
    void setLandscapeWindowProbeActive(bool active);
#endif
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
    void scheduleDevVideoDirectProbe();
#endif

protected:
    virtual void initializeGL();
    virtual void resizeGL(int width, int height);
    virtual void paintGL();
    virtual void timerEvent(QTimerEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void closeEvent(QCloseEvent *event);
    virtual bool eventFilter(QObject *watched, QEvent *event);

private:
    WiliwiliWidget(const WiliwiliWidget &);
    WiliwiliWidget &operator=(const WiliwiliWidget &);

    void startBilibiliFeed(
        bool append = false, bool manualRefresh = false);
    void clearDynamicImages();
    void clearContentImages();
    void clearSectionImages();
    void startNextThumbnail();
    void startNextContentThumbnail();
    void startNextDetailThumbnail();
    void startNextDynamicThumbnail();
    void startVideoDetail(int index);
    void startVideoDetail(const ContentItemCompat &item);
    void showHome();
    void selectSection(NavigationRail::Section section);
    void navigateBack();
    struct NavigationEntry;
    void pushNavigation(const NavigationEntry &entry);
    NavigationEntry popNavigation();
    void startQrLogin();
    void pollQrLogin();
    void requestProfile();
    void startVideoPlayback(int quality = -1, bool qualitySwitch = false);
    void startLivePlayback(const RecommendVideoResultCompat &card);
    void requestLivePlayback(int quality, bool qualitySwitch);
    virtual void videoPlayerRequestQuality(int quality);
    virtual void videoPlayerDidClose();
    virtual bool videoPlayerCanPresentYuv420() const;
    virtual void videoPlayerPresentYuv420(
        const Yuv420Frame &frame);
    virtual void videoPlayerClearSoftwareVideo();
    bool initializeYuvRenderer();
    void destroyYuvRenderer();
    void drawYuvFrame();
    void openVideoPlayback(const PlaybackSourceCompat &source);
    void requestDynamicFeed(bool append = false);
    void requestMessages(int type = 0, bool append = false);
    void requestNetworkDiagnostic();
    void toggleContentImages();
    void setPlaybackMode(int mode);
    void setDecoderMode(int mode);
    void promptSearch();
    void submitSearch(const QString &input, bool pushHistory = true);
    void hideSearchEditor();
    void positionSearchEditor();
    void requestContent(
        int mode,
        const QString &title,
        const QString &subtitle,
        const QString &endpoint,
        const QByteArray &cookies = QByteArray(),
        const QString &headerAction = QString(),
        bool append = false,
        const QString &pageTemplate = QString(),
        int page = 1,
        int pageSize = 0);
    void refreshContent();
    void loadMoreContent();
    void startContentTransport();
    void openContentItem(int index);
    void clearAppCache();
    void requestComments();
    void requestCommentReplies(quint64 rootRpid, const QString &author);
    void requestUserVideos(quint64 mid, const QString &name);
    void requestAccountContent(int mode);
    void postVideoAction(int action);
    void requestFavoriteFolderForAction();
    void promptComment();
    void promptPrivateMessage();
    void postFollowUser();
    void requestChatMessages(quint64 talkerId, const QString &name);
    void loadMoreSection();
    void openSectionItem(int index);
    void openDynamicDetail(int index);
    void startDynamicComments();
    void startPostAction(
        int action,
        const QString &endpoint,
        const QByteArray &formBody,
        const QString &progressText);
    void scheduleLoginPoll(int milliseconds);
    bool acceptLoginCookies(
        const QByteArray &cookies,
        const QString &refreshToken);
    void acceptProfile(const LoginProfileCompat &profile);
    void loadUiResources();
    void beginCjkFallbackLoad();
    bool pumpCjkFallbackLoad();
    void shutdownAndQuit();
    void bringApplicationToForeground();
    bool playerOwnsForeground() const;
    void suspendForegroundRestoreForPlayer();
    void scheduleForegroundRestore();
    void scheduleUiAction(int action);
    QString thumbnailUrl(const QString &source) const;
    QString dynamicImageUrl(const QString &source) const;
    QString probeResultText(const QString &tag) const;

    enum NetworkStage {
        FetchingHome,
        FetchingThumbnail,
        FetchingDetail,
        FetchingDetailThumbnail,
        FetchingPlayback,
        FetchingLivePlayback,
        FetchingLegacyLivePlayback,
        FetchingDanmaku,
        FetchingQrToken,
        PollingQrLogin,
        FetchingProfile,
        FetchingProfileFallback,
        FetchingProfileStats,
        FetchingDynamic,
        FetchingDynamicDetail,
        FetchingDynamicThumbnail,
        FetchingMessages,
        FetchingHomeWbiKeys,
        FetchingWbiKeys,
        FetchingContent,
        FetchingContentThumbnail,
        FetchingNetworkDiagnostic,
        FetchingFavoriteForAction,
        PostingAction,
        NetworkComplete
    };

    enum PendingAction {
        NoPendingAction = 0,
        LikePendingAction,
        CoinPendingAction,
        FavoritePendingAction,
        WatchLaterPendingAction,
        CommentPendingAction,
        FollowPendingAction,
        ChatMessagePendingAction
    };

    enum PendingUiAction {
        NoPendingUiAction = 0,
        SearchPendingUiAction,
        CommentPendingUiAction,
        PrivateMessagePendingUiAction
    };

    enum Screen {
        TopLevelScreenView,
        DetailScreenView,
        ContentScreenView
    };

    enum ContentMode {
        SearchContentMode = 0,
        SearchUsersContentMode,
        CommentsContentMode,
        CommentRepliesContentMode,
        HistoryContentMode,
        FavoritesContentMode,
        FavoriteVideosContentMode,
        WatchLaterContentMode,
        UserVideosContentMode,
        FollowingContentMode,
        ChatMessagesContentMode,
        DynamicDetailContentMode,
        SectionItemContentMode
    };

    struct NavigationEntry {
        NavigationEntry()
            : screen(TopLevelScreenView),
              restoreHome(false),
              section(NavigationRail::HomeSection),
              selectedVideoIndex(-1),
              contentMode(SearchContentMode),
              contentSubjectId(0),
              commentMode(3),
              commentLegacyFallback(false),
              contentPage(1),
              contentPageSize(0),
              contentCanLoadMore(false),
              contentImageGeneration(0),
              contentImageIndex(0),
              historyCursorMax(0),
              historyCursorViewAt(0),
              searchUsers(false),
              chatBeginTimestamp(0)
        {
        }

        Screen screen;
        bool restoreHome;
        NavigationRail::Section section;
        DetailScreen::State detailState;
        int selectedVideoIndex;
        ContentMode contentMode;
        ContentScreen::State contentState;
        quint64 contentSubjectId;
        int commentMode;
        bool commentLegacyFallback;
        QString contentEndpoint;
        QByteArray contentCookies;
        QString contentPageTemplate;
        int contentPage;
        int contentPageSize;
        bool contentCanLoadMore;
        int contentImageGeneration;
        int contentImageIndex;
        quint64 historyCursorMax;
        quint64 historyCursorViewAt;
        QString searchKeyword;
        bool searchUsers;
        quint64 chatBeginTimestamp;
    };

    NVGcontext *m_context;
    int m_fontId;
    int m_cjkFontId;
    QByteArray m_fontData;
    QByteArray m_cjkFontData;
    QFile *m_cjkFontFile;
    qint64 m_cjkFontExpectedBytes;
    qint64 m_cjkFontBytesRead;
    QTime m_cjkFontLoadClock;
    int m_logoHandle;
    int m_cardPlaceholderHandle;
    unsigned int m_yuvProgram;
    unsigned int m_yuvTextureY;
    unsigned int m_yuvTextureU;
    unsigned int m_yuvTextureV;
    // Two complete Y/U/V sets avoid updating a texture that the legacy
    // Symbian GLES driver may still be using for the previous draw.
    unsigned int m_yuvTextureYAlt;
    unsigned int m_yuvTextureUAlt;
    unsigned int m_yuvTextureVAlt;
    int m_yuvTextureSet;
    int m_yuvPositionLocation;
    int m_yuvTexCoordLocation;
    int m_yuvSamplerYLocation;
    int m_yuvSamplerULocation;
    int m_yuvSamplerVLocation;
    int m_yuvFullRangeLocation;
    int m_yuvTextureWidth;
    int m_yuvTextureHeight;
    int m_yuvUploadedSerial;
    quint64 m_yuvPresentedCount;
    quint64 m_yuvUploadedCount;
    qint64 m_yuvUploadMilliseconds;
    qint64 m_yuvUploadYMilliseconds;
    qint64 m_yuvUploadUMilliseconds;
    qint64 m_yuvUploadVMilliseconds;
    qint64 m_yuvUploadOtherMilliseconds;
    qint64 m_yuvDrawMilliseconds;
    qint64 m_yuvSwapMilliseconds;
    qint64 m_yuvPresentCallMilliseconds;
    qint64 m_yuvPaintGlMilliseconds;
    qint64 m_yuvLastPaintGlMilliseconds;
    quint64 m_yuvPresentCallCount;
    bool m_yuvRendererReady;
    bool m_yuvFirstFrameLogged;
    Yuv420Frame m_yuvFrame;
    int m_startupTimerId;
    int m_startupPhase;
    int m_foregroundTimerId;
    int m_foregroundAttemptCount;
    int m_uiActionTimerId;
    PendingUiAction m_pendingUiAction;
    int m_searchFocusTimerId;
    int m_searchFocusPhase;
    int m_metricsTimerId;
    int m_metricsTickCount;
    int m_softTelemetryTickCount;
    quint64 m_frameCount;
    bool m_hasPainted;
    bool m_uiResourcesReady;
    bool m_shuttingDown;
    bool m_hasActivated;
    bool m_chromeHidden;
#ifdef WILIWILI_ENABLE_APP_LANDSCAPE_WINDOW_PROBE
    bool m_landscapeWindowProbeActive;
#endif
    QLineEdit *m_searchEdit;
    QPushButton *m_searchButton;
    NetworkStage m_networkStage;
    int m_homeFreshIndex;
    int m_homeRequestedFreshIndex;
    int m_homeRequestedFreshType;
    bool m_homeAppend;
    bool m_homeCanLoadMore;
    bool m_homeSessionChanged;
    int m_thumbnailIndex;
    int m_thumbnailSuccessCount;
    int m_contentImageIndex;
    int m_detailThumbnailIndex;
    int m_dynamicThumbnailIndex;
    int m_contentImageGeneration;
    int m_contentImageLimit;
    int m_playbackMode;
    int m_decoderMode;
    bool m_contentAppend;
    bool m_contentCanLoadMore;
    int m_contentPage;
    int m_contentPageSize;
    quint64 m_historyCursorMax;
    quint64 m_historyCursorViewAt;
    QString m_contentPageTemplate;
    QString m_searchKeyword;
    bool m_searchUsers;
    int m_selectedVideoIndex;
    quint64 m_playbackCid;
    int m_playbackVideoWidth;
    int m_playbackVideoHeight;
    int m_requestedPlaybackQuality;
    bool m_playbackQualitySwitch;
    bool m_playbackIsLive;
    quint64 m_liveRoomId;
    QString m_liveTitle;
    Screen m_currentScreen;
    QVector<NavigationEntry> m_navigationHistory;
    ContentMode m_contentMode;
    PendingAction m_pendingAction;
    quint64 m_contentSubjectId;
    int m_commentMode;
    bool m_commentLegacyFallback;
    ContentItemCompat m_dynamicDetailItem;
    int m_dynamicDetailImageHandle;
    int m_dynamicCommentType;
    int m_messageType;
    int m_dynamicPage;
    QString m_dynamicOffset;
    bool m_dynamicAppend;
    bool m_dynamicHasMore;
    quint64 m_messageCursorId;
    quint64 m_messageCursorTime;
    quint64 m_chatBeginTimestamp;
    bool m_messageAppend;
    bool m_messageHasMore;
    QString m_contentEndpoint;
    QByteArray m_contentCookies;
    QString m_wbiMixinKey;
    QString m_qrKey;
    QByteArray m_qrPollUuid;
    QTime m_loginPollClock;
    int m_loginPollDelay;
    bool m_loginPollWaiting;
    QVector<RecommendVideoResultCompat> m_liveCards;
    QVector<int> m_cardImageHandles;
    QVector<int> m_dynamicImageHandles;
    QVector<int> m_contentImageHandles;
    QVector<int> m_detailImageHandles;
    QVector<int> m_sectionImageHandles;
    MemorySample m_memory;
    HomeScreen m_homeScreen;
    DetailScreen m_detailScreen;
    ContentScreen m_contentScreen;
    LoginScreen m_loginScreen;
    NavigationRail m_navigation;
    SectionScreen m_sectionScreen;
    NativeTransport m_transport;
    VideoPlayerWidget *m_videoPlayer;
};

} // namespace wiliwili

#endif
