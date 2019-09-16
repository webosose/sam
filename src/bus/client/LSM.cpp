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

#include <bus/client/LSM.h>
#include <bus/service/ApplicationManager.h>
#include <pbnjson.hpp>
#include <util/LSUtils.h>

const string LSM::NAME = "com.webos.surfacemanager";
const string LSM::NAME_GET_FOREGROUND_APP_INFO = "getForegroundAppInfo";
const string LSM::NAME_GET_RECENTS_APP_LIST = "getRecentsAppList";

LSM::LSM()
    : AbsLunaClient(NAME)
{
    setClassName(NAME);
}

LSM::~LSM()
{

}

void LSM::onInitialze()
{

}

void LSM::onServerStatusChanged(bool isConnected)
{
    if (isConnected) {
        pbnjson::JValue requestPayload = pbnjson::Object();
        requestPayload.put("serviceName", "com.webos.surfacemanager");
        requestPayload.put("category", "/");
        requestPayload.put("subscribe", true);

        m_registerServiceCategoryCall = ApplicationManager::getInstance().callMultiReply(
            "luna://com.webos.service.bus/signal/registerServiceCategory",
            requestPayload.stringify().c_str(),
            onServiceCategoryChanged,
            nullptr
        );
    } else {
        m_registerServiceCategoryCall.cancel();
        m_getRecentsAppListCall.cancel();
        m_getForegroundAppInfoCall.cancel();
    }
}

bool LSM::onServiceCategoryChanged(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull())
        return false;

    Logger::debug(LSM::getInstance().getClassName(), __FUNCTION__, "lscall", responsePayload.stringify());
    if (!responsePayload.hasKey("/") || !responsePayload["/"].isArray() || responsePayload["/"].arraySize() < 1)
        return true;

    int arraySize = responsePayload["/"].arraySize();
    for (int i = 0; i < arraySize; ++i) {
        std::string method = responsePayload["/"][i].asString();
        if (method.compare(NAME_GET_FOREGROUND_APP_INFO) == 0) {
            LSM::getInstance().subscribeGetForegroundAppInfo();
        } else if (method.compare(NAME_GET_RECENTS_APP_LIST) == 0) {
            LSM::getInstance().subscribeGetRecentsAppList();
        }
    }
    return true;
}

void LSM::subscribeGetForegroundAppInfo()
{
    static std::string method = std::string("luna://") + getName() + std::string("/getForegroundAppInfo");

    if (m_getForegroundAppInfoCall.isActive())
        return;

    m_getForegroundAppInfoCall = ApplicationManager::getInstance().callMultiReply(
        method.c_str(),
        AbsLunaClient::getSubscriptionPayload().stringify().c_str(),
        onGetForegroundAppInfo,
        nullptr
    );
}

bool LSM::onGetForegroundAppInfo(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull())
        return false;

    LSM::getInstance().EventForegroundAppInfoChanged(responsePayload);
    return true;
}

void LSM::subscribeGetRecentsAppList()
{
    static std::string method = std::string("luna://") + getName() + std::string("/getRecentsAppList");

    if (m_getRecentsAppListCall.isActive())
        return;

    m_getRecentsAppListCall = ApplicationManager::getInstance().callMultiReply(
        method.c_str(),
        AbsLunaClient::getSubscriptionPayload().stringify().c_str(),
        onGetRecentsAppList,
        nullptr
    );
}

bool LSM::onGetRecentsAppList(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull())
        return false;

    LSM::getInstance().EventRecentsAppListChanged(responsePayload);
    return true;
}
