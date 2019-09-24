// Copyright (c) 2017-2019 LG Electronics, Inc.
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

#include "base/LaunchPointList.h"
#include "base/AppDescriptionList.h"
#include "base/RunningAppList.h"
#include "bus/client/AppInstallService.h"
#include "bus/service/ApplicationManager.h"
#include <pbnjson.hpp>
#include <util/JValueUtil.h>

bool AppInstallService::onStatus(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = pbnjson::JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull())
        return false;

    if (!responsePayload["details"].isObject())
        return false;

    string packageId = "";
    string id = "";
    string type = "";

    JValueUtil::getValue(responsePayload, "id", id);
    JValueUtil::getValue(responsePayload, "details", "packageId", packageId);
    JValueUtil::getValue(responsePayload, "details", "type", type);

    if ((packageId.empty() && id.empty())) {
        return true;
    }
    std::string appId = packageId.empty() ? id : packageId;
    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);

    int statusValue;
    if (!JValueUtil::getValue(responsePayload, "statusValue", statusValue)) {
        return true;
    }

    AppDescriptionPtr oldAppDesc, newAppDesc;

    switch (statusValue) {
    case 30: // installed
//        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "Installed");
//        AppDescriptionManager::getInstance().onAppInstalled(appId);
//
//        AppDescriptionPtr oldAppDesc = AppDescriptionList::getInstance().getById(appId);
//        if (oldAppDesc != nullptr)
//            oldAppDesc->unlock();
//
//        AppDescriptionPtr newAppDesc = AppDescriptionList::getInstance().create(appId);
//        if (!newAppDesc) {
//            break;
//        }
//
//        // skip if stub type, (stub apps will be loaded on next full scan)
//        if (AppType::AppType_Stub == newAppDesc->getAppType()) {
//            break;
//        }
//
//        // disallow dev apps if current not in dev mode
//        if (AppLocation::AppLocation_Dev == newAppDesc->getTypeByDir() && SettingsImpl::getInstance().m_isDevMode == false) {
//            break;
//        }
//
//        addApp(appId, newAppDesc, AppStatusEvent::AppStatusEvent_Installed);
//
//
//        AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
//        if (!appDesc) {
//            AppDescriptionList::getInstance().add(newAppDesc);
//            ApplicationManager::getInstance().postListApp(newAppDesc, "added", AppDescription::toString(event));
//            break;
//        }
//
//        if (AppDescriptionList::compareVersion(appDesc, newAppDesc) == false) {
//            break;
//        }
//
//        AppDescriptionList::getInstance().add(newAppDesc, true);
//        ApplicationManager::getInstance().postGetAppStatus(appDesc, AppStatusEvent::AppStatusEvent_UpdateCompleted);
//
//        // clear web app cache
//        if (AppLocation::AppLocation_Dev == newAppDesc->getTypeByDir() && AppType::AppType_Web == newAppDesc->getAppType()) {
//            WAM::getInstance().discardCodeCache();
//        }
        break;

    case 22: // install cancelled
    case 23: // download err
    case 24: // install err
        appDesc->scan();
        break;

    case 31: // uninstalled
        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "Uninstalled");
        appDesc->scan();
        break;

    case 25: // uninstall fail
        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "Failed to uninstallation");
        appDesc->scan();
        break;

    case 41: // about to uninstall
        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "AboutToUninstall");
        // TODO 아래가 정말로 호출이 되어야 하나? abouot to uninstall이 뭔지 확인이 필요해 보인다.
        // LaunchPointList::getInstance().removeByAppId(appId);
        break;

    default:
        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "Unknown");
        return true;
    }
    return true;
}

AppInstallService::AppInstallService()
    : AbsLunaClient("com.webos.appInstallService")
{
    setClassName("AppInstallService");
}

AppInstallService::~AppInstallService()
{
}

void AppInstallService::onInitialze()
{

}

void AppInstallService::onServerStatusChanged(bool isConnected)
{
    static string method = std::string("luna://") + getName() + std::string("/status");
    if (isConnected) {
        m_statusCall = ApplicationManager::getInstance().callMultiReply(
            method.c_str(),
            AbsLunaClient::getSubscriptionPayload().stringify().c_str(),
            onStatus,
            this
        );
    } else {
        if (m_statusCall.isActive())
            m_statusCall.cancel();
    }
}

bool AppInstallService::onRemove(LSHandle* sh, LSMessage *message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));

    if (responsePayload.isNull()) {
    }
    return true;
}

Call AppInstallService::remove(const string& appId)
{
    static string method = std::string("luna://") + getName() + std::string("/remove");

    JValue requestPayload = pbnjson::Object();
    requestPayload.put("id", appId);
    requestPayload.put("subscribe", false);

    Call call = ApplicationManager::getInstance().callOneReply(method.c_str(), requestPayload.stringify().c_str(), onRemove, nullptr);
    return call;
}
