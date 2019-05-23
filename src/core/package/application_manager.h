// Copyright (c) 2012-2018 LG Electronics, Inc.
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

#ifndef CORE_PACKAGE_APPLICATION_MANAGER_H_
#define CORE_PACKAGE_APPLICATION_MANAGER_H_

#include <boost/signals2.hpp>
#include <luna-service2/lunaservice.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/base/singleton.h"
#include "core/lifecycle/application_errors.h"
#include "core/module/subscriber_of_appinstalld.h"
#include "core/package/app_scanner.h"
#include "core/package/application_description.h"
#include "interface/package/appscan_filter_interface.h"

enum class AppStatusChangeEvent
    : int8_t {
        NOTHING = 0, APP_INSTALLED, APP_UNINSTALLED, STORAGE_ATTACHED, STORAGE_DETACHED, UPDATE_COMPLETED,
};

class ApplicationManager: public Singleton<ApplicationManager> {
public:
    ApplicationManager();
    ~ApplicationManager();

    void SetAppScanFilter(AppScanFilterInterface& scan_filter);
    void SetAppDescriptionFactory(AppDescriptionFactoryInterface& factory);

    void Init();
    void ScanInitialApps();
    void StartPostInit();
    void Rescan(const std::vector<std::string>& reason = std::vector<std::string>());

    AppDescPtr getAppById(const std::string& app_id);
    const AppDescMaps& allApps();
    AppScanner& appScanner()
    {
        return app_scanner_;
    }
    bool LockAppForUpdate(const std::string& app_id, bool lock, std::string& err_text);
    void UninstallApp(const std::string& id, std::string& errorReason);

    std::string AppStatusChangeEventToString(const AppStatusChangeEvent& event);

    // Event Signals
    boost::signals2::signal<void(const pbnjson::JValue&, const std::vector<std::string>&, bool dev)> signalListAppsChanged;
    boost::signals2::signal<void(const pbnjson::JValue&, const std::string&, const std::string&, bool dev)> signalOneAppChanged;
    boost::signals2::signal<void(const AppDescMaps& all_apps)> signalAllAppRosterChanged;
    boost::signals2::signal<void(AppStatusChangeEvent, AppDescPtr app_desc)> signal_app_status_changed;

private:
    friend class Singleton<ApplicationManager> ;
    friend class VirtualAppManager;
    friend class ASM;

    static bool cbAppUninstalled(LSHandle* lshandle, LSMessage* message, void* data);

    void Scan();
    void Clear();
    void AddApp(const std::string& app_id, AppDescPtr new_desc, AppStatusChangeEvent event = AppStatusChangeEvent::APP_INSTALLED);
    void RemoveApp(const std::string& app_id, bool rescan = true, AppStatusChangeEvent event = AppStatusChangeEvent::APP_UNINSTALLED);
    void ReloadApp(const std::string& app_id);
    void ReplaceAppDesc(const std::string& app_id, AppDescPtr new_desc);
    void RemoveAppsOnUSB(const std::string& app_dir, const AppTypeByDir& app_type);

    void OnLocaleChanged(const std::string& lang, const std::string& region, const std::string& script);
    void OnReadyToUninstallSystemApp(pbnjson::JValue result, ErrorInfo err_info, void* user_data);
    void OnReadyToUninstallVirtualApp(pbnjson::JValue result, ErrorInfo err_info, void* user_data);

    void OnAppInstalled(const std::string& app_id);
    void OnAppUninstalled(const std::string& app_id);
    void OnAppScanFinished(ScanMode mode, const AppDescMaps& scanned_apps);
    void OnPackageStatusChanged(const std::string& app_id, const PackageStatus& status);

    void PublishListApps();
    void PublishOneAppChange(AppDescPtr app_desc, const std::string& change, AppStatusChangeEvent event);

    // variables
    AppScanner app_scanner_;
    AppDescMaps app_roster_;
    bool first_full_scan_started_;
    std::vector<std::string> scan_reason_;
};

#endif // CORE_PACKAGE_APPLICATION_MANAGER_H_
