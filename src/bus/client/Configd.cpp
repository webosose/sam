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

#include <bus/client/Configd.h>
#include <bus/service/ApplicationManager.h>
#include <pbnjson.hpp>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/LSUtils.h>

Configd::Configd()
    : AbsLunaClient("com.webos.service.config"),
      m_tokenConfigInfo(0)
{
}

Configd::~Configd()
{
}

void Configd::onInitialze()
{

}

void Configd::onServerStatusChanged(bool isConnected)
{
    if (isConnected) {
        requestConfigInfo();
    } else {
        if (0 != m_tokenConfigInfo) {
            (void) LSCallCancel(ApplicationManager::getInstance().get(), m_tokenConfigInfo, NULL);
            m_tokenConfigInfo = 0;
        }
    }
}

void Configd::requestConfigInfo()
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

    std::string method = std::string("luna://") + getName() + std::string("/getConfigs");

    LSErrorSafe lserror;
    if (!LSCall(ApplicationManager::getInstance().get(),
                method.c_str(),
                payload.stringify().c_str(),
                configInfoCallback,
                this,
                &m_tokenConfigInfo,
                &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 4,
                  PMLOGKS(LOG_KEY_TYPE, "lscall"),
                  PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  PMLOGKS(LOG_KEY_ERRORTEXT, lserror.message), "");
    }
}

bool Configd::configInfoCallback(LSHandle* handle, LSMessage* lsmsg, void* userData)
{
    Configd* subscriber = static_cast<Configd*>(userData);
    if (!subscriber)
        return false;

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull())
        return false;

    subscriber->EventConfigInfo(jmsg);
    return true;
}
