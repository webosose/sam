// Copyright (c) 2017-2018 LG Electronics, Inc.
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

#include <bus/AppMgrService.h>
#include <module/ConfigdSubscriber.h>
#include <module/ServiceObserver.h>
#include <pbnjson.hpp>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/LSUtils.h>

ConfigdSubscriber::ConfigdSubscriber() :
        m_tokenConfigInfo(0)
{
}

ConfigdSubscriber::~ConfigdSubscriber()
{
}

void ConfigdSubscriber::initialize()
{
    ServiceObserver::instance().add(WEBOS_SERVICE_CONFIGD, std::bind(&ConfigdSubscriber::onServerStatusChanged, this, std::placeholders::_1));
}

boost::signals2::connection ConfigdSubscriber::subscribeConfigInfo(boost::function<void(pbnjson::JValue)> func)
{
    return notify_config_info.connect(func);
}

void ConfigdSubscriber::onServerStatusChanged(bool connection)
{
    if (connection) {
        requestConfigInfo();
    } else {
        if (0 != m_tokenConfigInfo) {
            (void) LSCallCancel(AppMgrService::instance().serviceHandle(), m_tokenConfigInfo, NULL);
            m_tokenConfigInfo = 0;
        }
    }
}

void ConfigdSubscriber::requestConfigInfo()
{
    if (m_tokenConfigInfo != 0)
        return;
    if (m_configKeys.empty())
        return;

    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue configs = pbnjson::Array();

    payload.put("subscribe", true);
    for (auto& key : m_configKeys) {
        configs.append(key);
    }
    payload.put("configNames", configs);

    std::string method = std::string("luna://") + WEBOS_SERVICE_CONFIGD + std::string("/getConfigs");

    LSErrorSafe lserror;
    if (!LSCall(AppMgrService::instance().serviceHandle(),
                method.c_str(),
                payload.stringify().c_str(),
                configInfoCallback,
                this,
                &m_tokenConfigInfo,
                &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "lscall"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", __FUNCTION__), "err: %s", lserror.message);
    }
}

bool ConfigdSubscriber::configInfoCallback(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    ConfigdSubscriber* subscriber = static_cast<ConfigdSubscriber*>(user_data);
    if (!subscriber)
        return false;

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull())
        return false;

    subscriber->notify_config_info(jmsg);
    return true;
}
