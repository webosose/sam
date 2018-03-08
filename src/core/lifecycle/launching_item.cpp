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

#include "core/lifecycle/launching_item.h"

#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include "core/base/logging.h"
#include "core/package/application_manager.h"

AppLaunchingItem::AppLaunchingItem(const std::string& app_id, AppLaunchRequestType rtype, const pbnjson::JValue& params, LSMessage* lsmsg)
    : m_app_id(app_id)
    , m_pid("")
    , m_requested_app_id(app_id)
    , m_redirected(false)
    , m_rtype(rtype)
    , m_stage(AppLaunchingStage::PRELAUNCH)
    , m_sub_stage(0)
    , m_params(params.duplicate())
    , m_lsmsg(lsmsg)
    , m_show_splash(true)
    , m_show_spinner(true)
    , m_keep_alive(false)
    , m_automatic_launch(false)
    , m_return_token(0)
    , m_return_jmsg(pbnjson::Object())
    , m_err_code(0)
    , m_launch_start_time(0)
    , m_last_input_app(false)
{
    boost::uuids::uuid uid = boost::uuids::random_generator()();
    m_uid = boost::lexical_cast<std::string>(uid);

    LOG_INFO(MSGID_APPLAUNCH, 3, PMLOGKS("app_id", m_app_id.c_str()), PMLOGKS("uid", m_uid.c_str()),
                                 PMLOGKS("action", "created_launching_item"), "");

    if(m_lsmsg != NULL)
        LSMessageRef(m_lsmsg);
}

AppLaunchingItem::~AppLaunchingItem()
{
    LOG_INFO(MSGID_APPLAUNCH, 3, PMLOGKS("app_id", m_app_id.c_str()), PMLOGKS("uid", m_uid.c_str()),
                                 PMLOGKS("action", "removed_launching_item"), "");

    if(m_lsmsg != NULL)
        LSMessageUnref(m_lsmsg);
}

bool AppLaunchingItem::set_redirection(const std::string& target_app_id, const pbnjson::JValue& new_params)
{
    if(target_app_id.empty())
    {
        return false;
    }

    AppDescPtr app_desc = ApplicationManager::instance().getAppById(target_app_id);
    if(app_desc == NULL)
    {
        return false;
    }

    m_app_id        = target_app_id;
    m_params        = new_params;
    m_show_splash   = app_desc->splashOnLaunch();
    m_show_spinner  = app_desc->spinnerOnLaunch();
    m_preload       = "";
    m_keep_alive    = false;
    m_redirected    = true;

    return true;
}
