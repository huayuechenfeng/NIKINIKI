/*
 * NIKINIKI Bilibili endpoint definitions.
 *
 * Copyright (C) 2026 NIKINIKI contributors
 *
 * This focused list is derived from the API mapping in xfangfang/wiliwili
 * commit 88e5876bea9502d06f46a8656e3530684d3aaf7d and was reduced to the
 * endpoints used by the Symbian application on 2026-08-29.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef NIKINIKI_BILIBILI_ENDPOINTS_H
#define NIKINIKI_BILIBILI_ENDPOINTS_H

#include <string>

namespace nikiniki {
namespace BilibiliEndpoint {

const std::string ApiBase  = "//api.bilibili.com";
const std::string VcBase   = "//api.vc.bilibili.com";
const std::string PassBase = "//passport.bilibili.com";

const std::string Recommend       = ApiBase + "/x/web-interface/wbi/index/top/feed/rcmd";
const std::string HotsAll         = ApiBase + "/x/web-interface/popular";
const std::string Bangumi         = ApiBase + "/pgc/page/pc/bangumi/tab";
const std::string Detail          = ApiBase + "/x/web-interface/view";
const std::string DetailAll       = ApiBase + "/x/web-interface/view/detail";
const std::string QrLoginUrlV2    = PassBase + "/x/passport-login/web/qrcode/generate";
const std::string QrLoginInfoV2   = PassBase + "/x/passport-login/web/qrcode/poll";
const std::string MyInfo          = ApiBase + "/x/space/myinfo";
const std::string DynamicFeedAll  = ApiBase + "/x/polymer/web-dynamic/v1/feed/all";
const std::string DynamicDetail   = ApiBase + "/x/polymer/web-dynamic/desktop/v1/detail";
const std::string ChatSessions    = VcBase + "/session_svr/v1/session_svr/new_sessions";
const std::string MsgFeedAt       = ApiBase + "/x/msgfeed/at";
const std::string MsgFeedLike     = ApiBase + "/x/msgfeed/like";
const std::string MsgFeedReply    = ApiBase + "/x/msgfeed/reply";

} // namespace BilibiliEndpoint
} // namespace nikiniki

#endif // NIKINIKI_BILIBILI_ENDPOINTS_H
