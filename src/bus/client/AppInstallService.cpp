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

bool AppInstallService::onStatus(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    pbnjson::JValue subscriptionPayload = JDomParser::fromString(response.getPayload());
    Logger::logSubscriptionResponse(getInstance().getClassName(), __FUNCTION__, response, subscriptionPayload);

    if (subscriptionPayload.isNull())
        return true;

    int statusValue;
    string id = "";
    string packageId = "";

    JValueUtil::getValue(subscriptionPayload, "id", id);
    JValueUtil::getValue(subscriptionPayload, "details", "packageId", packageId);
    JValueUtil::getValue(subscriptionPayload, "statusValue", statusValue);

    if ((packageId.empty() && id.empty())) {
        return true;
    }
    string appId = packageId.empty() ? id : packageId;
    AppDescriptionPtr appDesc = nullptr;

    switch (statusValue) {
    case 22: // Install operation is cancelled
    case 23: // Install operation is complete with error during download
    case 24: // Install operation is complete with error during installing ipk
    case 25: // Remove operation is complete with error
        // appDesc->scan();
        break;

    case 30: // All install operations are complete successfully
        appDesc = AppDescriptionList::getInstance().getByAppId(appId);
        if (appDesc == nullptr) {
            appDesc = AppDescriptionList::getInstance().create(appId);
            if (appDesc == nullptr) {
                Logger::warning(getInstance().getClassName(), __FUNCTION__, appId, "Failed to create new AppDescription");
                return true;
            }
        }
        appDesc->rescan();
        appDesc->unlock();

        if (!AppDescriptionList::getInstance().isExist(appId)) {
            AppDescriptionList::getInstance().add(appDesc);
        }
        break;

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

    case 31: // uninstalled
        appDesc = AppDescriptionList::getInstance().getByAppId(appId);
        if (appDesc == nullptr) {
            Logger::warning(getInstance().getClassName(), __FUNCTION__, appId, "Cannot find AppDescription for uninstall");
            break;
        }
        if (appDesc->rescan() == false) {
            AppDescriptionList::getInstance().remove(appDesc);
        }
        break;

    case 32: // Need to close app
        appDesc = AppDescriptionList::getInstance().getByAppId(appId);
        if (appDesc) {
            appDesc->lock();
            // 실행되고 있는 앱들도 모두 죽여야 한다.
        }
        break;

    case 41: // Prepare to remove app
        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "AboutToUninstall");
        // TODO 아래가 정말로 호출이 되어야 하나? abouot to uninstall이 뭔지 확인이 필요해 보인다.
        // LaunchPointList::getInstance().removeByAppId(appId);
        break;

    default:
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
    static string method = string("luna://") + getName() + string("/status");
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
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    if (responsePayload.isNull())
        return true;

    return true;
}

Call AppInstallService::remove(const string& appId)
{
    static string method = string("luna://") + getName() + string("/remove");

    JValue requestPayload = pbnjson::Object();
    requestPayload.put("id", appId);
    requestPayload.put("subscribe", false);

    Call call = ApplicationManager::getInstance().callOneReply(method.c_str(), requestPayload.stringify().c_str(), onRemove, nullptr);
    return call;
}
