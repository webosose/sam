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

#include <bus/client/AppInstallService.h>
#include <bus/service/ApplicationManager.h>
#include <pbnjson.hpp>
#include <util/JValueUtil.h>
#include <util/LSUtils.h>

bool AppInstallService::onStatus(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = pbnjson::JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull())
        return false;

    if (!responsePayload["details"].isObject())
        return false;

    std::string packageId;
    std::string id;

    JValueUtil::getValue(responsePayload, "details", "packageId", packageId);
    JValueUtil::getValue(responsePayload, "id", id);
    if ((packageId.empty() && id.empty())) {
        return true;
    }

    int statusValue;
    PackageStatus packageStatus = PackageStatus::Unknown;
    if (JValueUtil::getValue(responsePayload, "statusValue", statusValue)) {
        switch (statusValue) {
        case 30: // installed
            packageStatus = PackageStatus::Installed;
            break;

        case 22: // install cancelled
        case 23: // download err
        case 24: // install err
            packageStatus = PackageStatus::InstallFailed;
            break;

        case 31: // uninstalled
            packageStatus = PackageStatus::Uninstalled;
            break;

        case 25: // uninstall fail
            packageStatus = PackageStatus::UninstallFailed;
            break;

        case 41: // about to uninstall
            packageStatus = PackageStatus::AboutToUninstall;
            break;

        default:
            packageStatus = PackageStatus::Unknown;
            return true;
        }
    }

    // Handle variation of uninstall
    string type;
    if (PackageStatus::Uninstalled == packageStatus &&
        JValueUtil::getValue(responsePayload, "details", "type", type) &&
        type == "initialize") {
        packageStatus = PackageStatus::ResetToDefault;
    }

    std::string appId = packageId.empty() ? id : packageId;
    Logger::info(getInstance().getClassName(), __FUNCTION__, appId, std::to_string((int)packageStatus));
    AppInstallService::getInstance().EventStatusChanged(appId, packageStatus);
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

