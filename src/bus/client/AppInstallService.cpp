// Copyright (c) 2017-2020 LG Electronics, Inc.
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
    JValue subscriptionPayload = JDomParser::fromString(response.getPayload());
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
    case 30: // All install operations are complete successfully
    case 31: // uninstalled
        AppDescriptionList::getInstance().scanApp(appId);
        appDesc = AppDescriptionList::getInstance().getByAppId(appId);
        if (appDesc) {
            appDesc->unlock();
        }
        break;

    case 32: // Need to close app
        // TODO should we need to close runningApps?
        break;

    case 41: // Prepare to remove app
        appDesc = AppDescriptionList::getInstance().getByAppId(appId);
        if (appDesc) {
            appDesc->lock();
        }
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

void AppInstallService::onInitialzed()
{

}

void AppInstallService::onFinalized()
{
    m_statusCall.cancel();
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
        m_statusCall.cancel();
    }
}

bool AppInstallService::onRemove(LSHandle* sh, LSMessage *message, void* context)
{
    Message response(message);
    JValue responsePayload = JDomParser::fromString(response.getPayload());
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
