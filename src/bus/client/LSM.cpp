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

#include "LSM.h"

#include "base/AppDescription.h"
#include "base/LaunchPointList.h"
#include "base/LaunchPoint.h"
#include "base/RunningApp.h"
#include "base/RunningAppList.h"
#include "bus/service/ApplicationManager.h"
#include "util/JValueUtil.h"

bool LSM::isFullscreenWindowType(const JValue& foregroundInfo)
{
    bool windowGroup = false;
    bool windowGroupOwner = false;
    string windowType = "";

    JValueUtil::getValue(foregroundInfo, "windowGroup", windowGroup);
    JValueUtil::getValue(foregroundInfo, "windowGroupOwner", windowGroupOwner);
    JValueUtil::getValue(foregroundInfo, "windowType", windowType);

    if (!windowGroup)
        windowGroupOwner = true;
    if (!windowGroupOwner)
        return false;

    return SAMConf::getInstance().isFullscreenWindowTypes(windowType);
}

LSM::LSM()
    : AbsLunaClient("com.webos.surfacemanager")
{
    setClassName("LSM");
    m_foregroundInfo = pbnjson::Array();
}

LSM::~LSM()
{

}

void LSM::onInitialzed()
{

}

void LSM::onFinalized()
{
    m_registerServiceCategoryCall.cancel();
    m_getRecentsAppListCall.cancel();
    m_getForegroundAppInfoCall.cancel();
}

void LSM::onServerStatusChanged(bool isConnected)
{
    if (isConnected) {
        JValue requestPayload = pbnjson::Object();
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
    Message response(message);
    JValue subscriptionPayload = JDomParser::fromString(response.getPayload());
    Logger::logSubscriptionResponse(getInstance().getClassName(), __FUNCTION__, response, subscriptionPayload);

    if (subscriptionPayload.isNull())
        return true;

    if (!subscriptionPayload.hasKey("/") ||
        !subscriptionPayload["/"].isArray() ||
        subscriptionPayload["/"].arraySize() < 1)
        return true;

    int arraySize = subscriptionPayload["/"].arraySize();
    for (int i = 0; i < arraySize; ++i) {
        string method = subscriptionPayload["/"][i].asString();
        if (method.compare("getForegroundAppInfo") == 0) {
            LSM::getInstance().subscribeGetForegroundAppInfo();
        } else if (method.compare("getRecentsAppList") == 0) {
            LSM::getInstance().subscribeGetRecentsAppList();
        }
    }
    return true;
}

void LSM::subscribeGetForegroundAppInfo()
{
    static string method = string("luna://") + getName() + string("/getForegroundAppInfo");

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
    Message response(message);
    JValue subscriptionPayload = JDomParser::fromString(response.getPayload());
    Logger::logSubscriptionResponse(getInstance().getClassName(), __FUNCTION__, response, subscriptionPayload);

    if (subscriptionPayload.isNull())
        return true;

    JValue orgForegroundAppInfo;
    if (!JValueUtil::getValue(subscriptionPayload, "foregroundAppInfo", orgForegroundAppInfo)) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "Failed to get 'foregroundAppInfo'");
        return true;
    }

    string newFullWindowAppId = "";
    vector<string> newForegroundAppIds;
    JValue newForegroundAppInfo = pbnjson::Array();

    for (int i = 0; i < orgForegroundAppInfo.arraySize(); ++i) {
        string appId;
        int displayId;
        string processId;

        JValueUtil::getValue(orgForegroundAppInfo[i], "appId", appId);
        JValueUtil::getValue(orgForegroundAppInfo[i], "displayId", displayId);
        JValueUtil::getValue(orgForegroundAppInfo[i], "processId", processId);

        RunningAppPtr runningApp = RunningAppList::getInstance().getByAppIdAndDisplayId(appId, displayId);
        if (runningApp == nullptr) {
            Logger::warning(getInstance().getClassName(), __FUNCTION__,
                            Logger::format("Cannot find RunningApp. SAM might be restarted: appId(%s) displayId(%d)", appId.c_str(), displayId));
            continue;
        }

        // TODO This is ambiguous multi display env. We need to find better way
        if (isFullscreenWindowType(orgForegroundAppInfo[i])) {
            newFullWindowAppId = appId;
        }

        runningApp->setProcessId(processId);
        runningApp->setDisplayId(displayId);
        runningApp->setLifeStatus(LifeStatus::LifeStatus_FOREGROUND);
        newForegroundAppInfo.append(orgForegroundAppInfo[i].duplicate());
        newForegroundAppIds.push_back(appId);
    }

    // set background
    for (auto& oldAppId : getInstance().m_foregroundAppIds) {
        bool found = false;
        for (auto& newAppId : newForegroundAppIds) {
            if (oldAppId == newAppId) {
                found = true;
                break;
            }
        }

        if (found == false) {
            RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(oldAppId);
            if (runningApp && runningApp->getLifeStatus() == LifeStatus::LifeStatus_FOREGROUND) {
                runningApp->setLifeStatus(LifeStatus::LifeStatus_BACKGROUND);
            }
        }
    }

    bool extraInfoOnly = false;
    if (getInstance().m_fullWindowAppId == newFullWindowAppId) {
        extraInfoOnly = true;
    }

    getInstance().m_fullWindowAppId = newFullWindowAppId;
    getInstance().m_foregroundAppIds = newForegroundAppIds;
    getInstance().m_foregroundInfo = newForegroundAppInfo;

    ApplicationManager::getInstance().postRunning(nullptr);
    ApplicationManager::getInstance().postGetForegroundAppInfo(extraInfoOnly);
    return true;
}

void LSM::subscribeGetRecentsAppList()
{
    static string method = string("luna://") + getName() + string("/getRecentsAppList");

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
    Message response(message);
    JValue subscriptionPayload = JDomParser::fromString(response.getPayload());
    Logger::logSubscriptionResponse(getInstance().getClassName(), __FUNCTION__, response, subscriptionPayload);

    if (subscriptionPayload.isNull())
        return true;

    LSM::getInstance().EventRecentsAppListChanged(subscriptionPayload);
    return true;
}
