// Copyright (c) 2012-2019 LG Electronics, Inc.
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

#include <bus/client/WAM.h>
#include "bus/service/ApplicationManager.h"
#include "util/Logging.h"

bool WAM::onListRunningApps(LSHandle* handle, LSMessage* lsmsg, void* userData)
{
    pbnjson::JValue subscriptionPayload = JDomParser::fromString(LSMessageGetPayload(lsmsg));

    if (subscriptionPayload.isNull()) {
        LOG_ERROR(MSGID_WAM_RUNNING_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "parsing_fail"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return false;
    }

    pbnjson::JValue runningList = pbnjson::Array();
    if (!subscriptionPayload.hasKey("running") || !subscriptionPayload["running"].isArray()) {
        LOG_ERROR(MSGID_WAM_RUNNING_ERR, 3,
                  PMLOGKS(LOG_KEY_REASON, "invalid_running"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  PMLOGJSON(LOG_KEY_PAYLOAD, subscriptionPayload.stringify().c_str()), "");
    } else {
        LOG_DEBUG("WAM_RUNNING: %s", subscriptionPayload.stringify().c_str());
        runningList = subscriptionPayload["running"];
    }

    WAM::getInstance().EventListRunningAppsChanged(runningList);
    return true;
}

WAM::WAM()
    : AbsLunaClient("com.palm.webappmanager")
{
}

WAM::~WAM()
{
}

void WAM::onInitialze()
{

}

void WAM::onServerStatusChanged(bool isConnected)
{
    static string method = std::string("luna://") + getName() + std::string("/listRunningApps");
    if (isConnected) {
        JValue requestPayload = pbnjson::Object();
        requestPayload.put("includeSysApps", true);
        requestPayload.put("subscribe", true);

        m_listRunningAppsCall = ApplicationManager::getInstance().callMultiReply(
            method.c_str(),
            AbsLunaClient::getSubscriptionPayload().stringify().c_str(),
            onListRunningApps,
            this
        );
    } else {
        if (m_listRunningAppsCall.isActive())
            m_listRunningAppsCall.cancel();
    }
}

