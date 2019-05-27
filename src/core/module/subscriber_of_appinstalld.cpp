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

#include "core/module/subscriber_of_appinstalld.h"

#include <pbnjson.hpp>

#include "core/base/jutil.h"
#include "core/base/logging.h"
#include "core/base/lsutils.h"
#include "core/base/related_luna_service_roster.h"
#include "core/bus/appmgr_service.h"
#include "core/module/service_observer.h"

AppinstalldSubscriber::AppinstalldSubscriber() :
        token_install_status_(0), token_update_info_(0)
{
}

AppinstalldSubscriber::~AppinstalldSubscriber()
{
}

void AppinstalldSubscriber::Init()
{
    ServiceObserver::instance().Add(WEBOS_SERVICE_APPINSTALL, std::bind(&AppinstalldSubscriber::OnInstallServiceStatusChanged, this, std::placeholders::_1));
    ServiceObserver::instance().Add(WEBOS_SERVICE_APPUPDATE, std::bind(&AppinstalldSubscriber::OnUpdateServiceStatusChanged, this, std::placeholders::_1));
}

boost::signals2::connection AppinstalldSubscriber::SubscribeInstallStatus(boost::function<void(const std::string&, const PackageStatus&)> func)
{
    return notify_install_status_.connect(func);
}

boost::signals2::connection AppinstalldSubscriber::SubscribeUpdateInfo(boost::function<void(const pbnjson::JValue&)> func)
{
    return notify_update_info_.connect(func);
}

//////////////////////////////////////////
// app install service
void AppinstalldSubscriber::OnInstallServiceStatusChanged(bool connection)
{
    if (connection) {
        RequestInstallStatus();
    } else {
        if (0 != token_install_status_) {
            (void) LSCallCancel(AppMgrService::instance().serviceHandle(), token_install_status_, NULL);
            token_install_status_ = 0;
        }
    }
}

void AppinstalldSubscriber::RequestInstallStatus()
{
    if (token_install_status_ != 0)
        return;

    std::string method = std::string("luna://") + WEBOS_SERVICE_APPINSTALL + std::string("/status");

    LSErrorSafe lserror;
    if (!LSCall(AppMgrService::instance().serviceHandle(), method.c_str(), "{\"subscribe\":true}", OnInstallStatusCallback, this, &token_install_status_, &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "lscall"), PMLOGJSON("payload", "{\"subscribe\":true}"), PMLOGKS("where", __FUNCTION__), "err: %s", lserror.message);
    }
}

// appinstalld's return example
//
//  {
//    "id": "com.webos.fd.systemapp",
//    "statusValue": 31,
//    "details": {
//        "state": "removed",
//        "client": "com.webos.lunasend-5582",
//        "packageId": "com.webos.fd.systemapp",
//        "simpleStatus": "remove",
//        "verified": true,
//        "type" : "initialize"  <= when system app was rollback to default
//        "services": [
//            "com.webos.fd.systemapp.service"
//        ],
//        "progress": 100
//    }
//  }

bool AppinstalldSubscriber::OnInstallStatusCallback(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    AppinstalldSubscriber* subscriber = static_cast<AppinstalldSubscriber*>(user_data);
    if (!subscriber)
        return false;

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull())
        return false;

    if (!jmsg["details"].isObject())
        return false;

    std::string package_id;
    std::string id;
    std::string app_id;
    PackageStatus package_status = PackageStatus::Unknown;

    if (jmsg["details"].hasKey("packageId") && jmsg["details"]["packageId"].isString())
        package_id = jmsg["details"]["packageId"].asString();

    if (jmsg.hasKey("id") && jmsg["id"].isString())
        id = jmsg["id"].asString();

    if (jmsg.hasKey("statusValue") && jmsg["statusValue"].isNumber()) {
        int status = jmsg["statusValue"].asNumber<int>();
        switch (status) {
        case 30: // installed
            package_status = PackageStatus::Installed;
            break;
        case 22: // install cancelled
        case 23: // download err
        case 24: // install err
            package_status = PackageStatus::InstallFailed;
            break;
        case 31: // uninstalled
            package_status = PackageStatus::Uninstalled;
            break;
        case 25: // uninstall fail
            package_status = PackageStatus::UninstallFailed;
            break;
        case 41: // about to uninstall
            package_status = PackageStatus::AboutToUninstall;
            break;
        default:
            package_status = PackageStatus::Unknown;
            break;
        }
    }

    if ((package_id.empty() && id.empty()) || PackageStatus::Unknown == package_status) {
        return true;
    }

    // Handle variation of uninstall
    if (PackageStatus::Uninstalled == package_status && jmsg["details"].hasKey("type") && jmsg["details"]["type"].isString() && jmsg["details"]["type"].asString() == "initialize") {
        package_status = PackageStatus::ResetToDefault;
    }

    app_id = package_id.empty() ? id : package_id;

    LOG_INFO(MSGID_PACKAGE_STATUS, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKFV("status", "%d", (int) package_status), "");

    subscriber->notify_install_status_(app_id, package_status);
    return true;
}

//////////////////////////////////////////
// app update info
void AppinstalldSubscriber::OnUpdateServiceStatusChanged(bool connection)
{
    if (connection) {
        RequestUpdateInfo();
    } else {
        if (0 != token_update_info_) {
            (void) LSCallCancel(AppMgrService::instance().serviceHandle(), token_update_info_, NULL);
            token_update_info_ = 0;
        }
    }
}

void AppinstalldSubscriber::RequestUpdateInfo()
{
    if (token_update_info_ != 0)
        return;

    std::string method = std::string("luna://") + WEBOS_SERVICE_APPUPDATE + std::string("/statusAll");

    LSErrorSafe lserror;
    if (!LSCall(AppMgrService::instance().serviceHandle(), method.c_str(), "{\"subscribe\":true}", OnUpdateInfoCallback, this, &token_update_info_, &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "lscall"), PMLOGJSON("payload", "{\"subscribe\":true}"), PMLOGKS("where", __FUNCTION__), "err: %s", lserror.message);
    }
}

bool AppinstalldSubscriber::OnUpdateInfoCallback(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    AppinstalldSubscriber* subscriber = static_cast<AppinstalldSubscriber*>(user_data);
    if (!subscriber)
        return false;

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull())
        return false;

    subscriber->notify_update_info_(jmsg);
    return true;
}

