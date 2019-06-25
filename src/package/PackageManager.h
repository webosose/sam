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

#ifndef PACKAGE_PACKAGEMANAGER_H_
#define PACKAGE_PACKAGEMANAGER_H_

#include <boost/signals2.hpp>
#include <lifecycle/ApplicationErrors.h>
#include <luna-service2/lunaservice.h>
#include <module/AppinstalldSubscriber.h>
#include <package/AppDescription.h>
#include <package/AppDescriptionScanner.h>
#include <util/Singleton.h>
#include <map>
#include <memory>
#include <string>
#include <vector>


enum class AppStatusChangeEvent : int8_t {
    AppStatusChangeEvent_Nothing = 0,
    AppStatusChangeEvent_Installed,
    AppStatusChangeEvent_Uninstalled,
    STORAGE_ATTACHED,
    STORAGE_DETACHED,
    UPDATE_COMPLETED,
};

class PackageManager: public Singleton<PackageManager> {
friend class Singleton<PackageManager> ;
public:
    PackageManager();
    virtual ~PackageManager();

    void initialize();
    void scanInitialApps();
    void startPostInit();
    void rescan(const std::vector<std::string>& reason = std::vector<std::string>());

    AppDescPtr getAppById(const std::string& app_id);
    const AppDescMaps& allApps();
    AppDescriptionScanner& appScanner()
    {
        return m_appScanner;
    }
    bool lockAppForUpdate(const std::string& app_id, bool lock, std::string& err_text);
    void uninstallApp(const std::string& id, std::string& errorReason);

    std::string toString(const AppStatusChangeEvent& event);

    // Event Signals
    boost::signals2::signal<void(const pbnjson::JValue&, const std::vector<std::string>&, bool dev)> signalListAppsChanged;
    boost::signals2::signal<void(const pbnjson::JValue&, const std::string&, const std::string&, bool dev)> signalOneAppChanged;
    boost::signals2::signal<void(const AppDescMaps& all_apps)> signalAllAppRosterChanged;
    boost::signals2::signal<void(AppStatusChangeEvent, AppDescPtr app_desc)> signalAppStatusChanged;

private:
    static bool cbAppUninstalled(LSHandle* lshandle, LSMessage* message, void* data);

    void scan();
    void clear();
    void addApp(const std::string& app_id, AppDescPtr new_desc, AppStatusChangeEvent event = AppStatusChangeEvent::AppStatusChangeEvent_Installed);
    void removeApp(const std::string& app_id, bool rescan = true, AppStatusChangeEvent event = AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
    void reloadApp(const std::string& app_id);
    void replaceAppDesc(const std::string& app_id, AppDescPtr new_desc);

    void onLocaleChanged(const std::string& lang, const std::string& region, const std::string& script);
    void onReadyToUninstallSystemApp(pbnjson::JValue result, ErrorInfo err_info, void* user_data);

    void onAppInstalled(const std::string& app_id);
    void onAppUninstalled(const std::string& app_id);
    void onAppScanFinished(ScanMode mode, const AppDescMaps& scanned_apps);
    void onPackageStatusChanged(const std::string& app_id, const PackageStatus& status);

    void publishListApps();
    void publishOneAppChange(AppDescPtr app_desc, const std::string& change, AppStatusChangeEvent event);

    // variables
    AppDescriptionScanner m_appScanner;
    AppDescMaps m_appDescMaps;
    bool m_isFirstFullScanStarted;
    std::vector<std::string> m_scanReason;
};

#endif // PACKAGE_PACKAGEMANAGER_H_
