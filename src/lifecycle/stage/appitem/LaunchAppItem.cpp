// Copyright (c) 2012-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <lifecycle/stage/appitem/LaunchAppItem.h>
#include <package/PackageManager.h>
#include <util/Logging.h>


LaunchAppItem::LaunchAppItem(const std::string& appId, const std::string& display, const pbnjson::JValue& params, LSMessage* lsmsg)
    : AppItem(appId, display, ""),
      m_requestedAppId(appId),
      m_stage(AppLaunchingStage::PRELAUNCH),
      m_subStage(0),
      m_params(params.duplicate()),
      m_lsmsg(lsmsg),
      m_showSplash(true),
      m_showSpinner(true),
      m_keepAlive(false),
      m_returnToken(0),
      m_errorCode(0),
      m_launchStartTime(0)
{
    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("appId", getAppId().c_str()),
             PMLOGKS("uid", getUid().c_str()),
             PMLOGKS("action", "created_launching_item"), "");

    if (m_lsmsg != NULL) {
        LSMessageRef(m_lsmsg);
    }
}

LaunchAppItem::~LaunchAppItem()
{
    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("appId", getAppId().c_str()),
             PMLOGKS("uid", getUid().c_str()),
             PMLOGKS("action", "removed_launching_item"), "");

    if (m_lsmsg != NULL) {
        LSMessageUnref(m_lsmsg);
    }
}
