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

#include <bus/client/Bootd.h>
#include <bus/service/ApplicationManager.h>
#include <pbnjson.hpp>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/LSUtils.h>

Bootd::Bootd()
    : AbsLunaClient("com.webos.bootManager"),
      m_tokenBootStatus(0)
{
}

Bootd::~Bootd()
{
}

void Bootd::onInitialze()
{

}

void Bootd::onServerStatusChanged(bool isConnected)
{
    if (isConnected) {
        if (m_tokenBootStatus != 0)
             return;

         std::string method = std::string("luna://") + getName() + std::string("/getBootStatus");

         LSErrorSafe lserror;
         if (!LSCall(ApplicationManager::getInstance().get(),
                     method.c_str(),
                     "{\"subscribe\":true}",
                     onGetBootStatus,
                     this,
                     &m_tokenBootStatus,
                     &lserror)) {
             LOG_ERROR(MSGID_LSCALL_ERR, 4,
                       PMLOGKS(LOG_KEY_TYPE, "lscall"),
                       PMLOGJSON(LOG_KEY_PAYLOAD, "{\"subscribe\":true}"),
                       PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                       PMLOGKS(LOG_KEY_ERRORTEXT, lserror.message), "");
         }
    } else {
        if (0 != m_tokenBootStatus) {
            (void) LSCallCancel(ApplicationManager::getInstance().get(), m_tokenBootStatus, NULL);
            m_tokenBootStatus = 0;
        }
    }
}

bool Bootd::onGetBootStatus(LSHandle* handle, LSMessage* lsmsg, void* userData)
{
    Bootd* subscriber = static_cast<Bootd*>(userData);
    if (!subscriber)
        return false;

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull())
        return false;

    if (jmsg.hasKey("bootStatus")) {
        // factory, normal, firstUse
        subscriber->m_bootStatusStr = jmsg["bootStatus"].asString();
    }

    subscriber->EventBootStatusChanged(jmsg);
    return true;
}
