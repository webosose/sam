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
#include <lifecycle/stage/AppLaunchingItem.h>
#include <package/PackageManager.h>
#include <util/Logging.h>


AppLaunchingItem::AppLaunchingItem(const std::string& appId, const pbnjson::JValue& params, LSMessage* lsmsg)
    : m_appId(appId),
      m_pid(""),
      m_requestedAppId(appId),
      m_redirected(false),
      m_stage(AppLaunchingStage::PRELAUNCH),
      m_subStage(0),
      m_params(params.duplicate()),
      m_lsmsg(lsmsg),
      m_showSplash(true),
      m_showSpinner(true),
      m_keepAlive(false),
      m_automaticLaunch(false),
      m_returnToken(0),
      m_returnJmsg(pbnjson::Object()),
      m_errCode(0),
      m_launchStartTime(0),
      m_isLastInputApp(false)
{
    boost::uuids::uuid uid = boost::uuids::random_generator()();
    m_uid = boost::lexical_cast<std::string>(uid);

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", m_appId.c_str()),
             PMLOGKS("uid", m_uid.c_str()),
             PMLOGKS("action", "created_launching_item"), "");

    if (m_lsmsg != NULL) {
        LSMessageRef(m_lsmsg);
    }
}

AppLaunchingItem::~AppLaunchingItem()
{
    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", m_appId.c_str()),
             PMLOGKS("uid", m_uid.c_str()),
             PMLOGKS("action", "removed_launching_item"), "");

    if (m_lsmsg != NULL) {
        LSMessageUnref(m_lsmsg);
    }
}

bool AppLaunchingItem::setRedirection(const std::string& target_app_id, const pbnjson::JValue& new_params)
{
    if (target_app_id.empty()) {
        return false;
    }

    AppDescPtr app_desc = PackageManager::instance().getAppById(target_app_id);
    if (app_desc == NULL) {
        return false;
    }

    m_appId = target_app_id;
    m_params = new_params;
    m_showSplash = app_desc->splashOnLaunch();
    m_showSpinner = app_desc->spinnerOnLaunch();
    m_preload = "";
    m_keepAlive = false;
    m_redirected = true;

    return true;
}
