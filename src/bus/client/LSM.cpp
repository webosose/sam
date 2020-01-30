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
    m_getForegroundAppInfoCall.cancel();
}

void LSM::onServerStatusChanged(bool isConnected)
{
    static string method = string("luna://") + getName() + string("/getForegroundAppInfo");

    if (isConnected) {
        m_getForegroundAppInfoCall = ApplicationManager::getInstance().callMultiReply(
            method.c_str(),
            AbsLunaClient::getSubscriptionPayload().stringify().c_str(),
            onGetForegroundAppInfo,
            nullptr
        );
        Logger::logSubscriptionRequest(getClassName(), __FUNCTION__, method, AbsLunaClient::getSubscriptionPayload());
    } else {
        m_getForegroundAppInfoCall.cancel();
    }
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

        // SAM knows its child pid better than LSM.
        // This code is needed specially in container environment.
        if (runningApp->getLaunchPoint()->getAppDesc()->getAppType() == AppType::AppType_Web) {
            runningApp->setProcessId(processId);
        }
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

