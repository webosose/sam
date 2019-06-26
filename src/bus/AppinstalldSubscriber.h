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

#ifndef BUS_APPINSTALLDSUBSCRIBER_H_
#define BUS_APPINSTALLDSUBSCRIBER_H_

#include <luna-service2/lunaservice.h>
#include <boost/signals2.hpp>
#include <pbnjson.hpp>
#include <util/Singleton.h>


enum class PackageStatus : int8_t {
    Unknown = 0,
    Installed,
    InstallFailed,
    Uninstalled,
    UninstallFailed,
    AboutToUninstall,
    ResetToDefault,
};

class AppinstalldSubscriber: public Singleton<AppinstalldSubscriber> {
public:
    AppinstalldSubscriber();
    ~AppinstalldSubscriber();

    void initialize();
    boost::signals2::connection subscribeInstallStatus(boost::function<void(const std::string&, const PackageStatus&)> func);
    boost::signals2::connection subscribeUpdateInfo(boost::function<void(const pbnjson::JValue&)> func);

private:
    friend class Singleton<AppinstalldSubscriber> ;

    void onInstallServiceStatusChanged(bool connection);
    void requestInstallStatus();
    static bool onInstallStatusCallback(LSHandle* handle, LSMessage* lsmsg, void* user_data);

    void onUpdateServiceStatusChanged(bool connection);
    void requestUpdateInfo();
    static bool onUpdateInfoCallback(LSHandle* handle, LSMessage* lsmsg, void* user_data);

    LSMessageToken m_tokenInstallStatus;
    LSMessageToken m_tokenUpdateInfo;
    boost::signals2::signal<void(const std::string&, const PackageStatus&)> notify_install_status_;
    boost::signals2::signal<void(const pbnjson::JValue&)> notify_update_info_;
};

#endif  // BUS_APPINSTALLDSUBSCRIBER_H_

