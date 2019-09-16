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

#ifndef PACKAGE_APPPACKAGEMANAGER_H_
#define PACKAGE_APPPACKAGEMANAGER_H_

#include <boost/signals2.hpp>
#include <bus/client/AppInstallService.h>
#include <lifecycle/ApplicationErrors.h>
#include <luna-service2/lunaservice.h>
#include <package/AppPackage.h>
#include <package/AppPackageScanner.h>
#include "interface/ISingleton.h"
#include "interface/IClassName.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace std;
using namespace pbnjson;

enum class AppStatusChangeEvent : int8_t {
    AppStatusChangeEvent_Nothing = 0,
    AppStatusChangeEvent_Installed,
    AppStatusChangeEvent_Uninstalled,
    STORAGE_ATTACHED,
    STORAGE_DETACHED,
    UPDATE_COMPLETED,
};

class AppPackageManager: public ISingleton<AppPackageManager>,
                         public IClassName {
friend class ISingleton<AppPackageManager> ;
public:
    AppPackageManager();
    virtual ~AppPackageManager();

    void initialize();
    void scanInitialApps();
    void startPostInit();
    void rescan(const std::vector<std::string>& reason = std::vector<std::string>());

    AppPackagePtr getAppById(const std::string& appId);
    const AppDescMaps& allApps();
    AppPackageScanner& appScanner()
    {
        return m_appScanner;
    }
    bool lockAppForUpdate(const std::string& appId, bool lock, std::string& errorText);
    void uninstallApp(const std::string& id, std::string& errorText);

    std::string toString(const AppStatusChangeEvent& event);

    // Event Signals
    boost::signals2::signal<void(const pbnjson::JValue&, const std::vector<std::string>&, bool dev)> EventListAppsChanged;
    boost::signals2::signal<void(const pbnjson::JValue&, const std::string&, const std::string&, bool dev)> EventOneAppChanged;
    boost::signals2::signal<void(AppStatusChangeEvent, AppPackagePtr app_desc)> EventAppStatusChanged;

private:
    static bool onBatch(LSHandle* sh, LSMessage* message, void* context);
    static bool onAppUninstalled(LSHandle* sh, LSMessage* message, void* context);
    static bool onCreatePincodePrompt(LSHandle* sh, LSMessage* message, void* context);

    void scan();
    void clear();
    void addApp(const std::string& appId, AppPackagePtr new_desc, AppStatusChangeEvent event = AppStatusChangeEvent::AppStatusChangeEvent_Installed);
    void removeApp(const std::string& appId, bool rescan = true, AppStatusChangeEvent event = AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
    void reloadApp(const std::string& appId);
    void replaceAppDesc(const std::string& appId, AppPackagePtr new_desc);

    void onLocaleChanged(const std::string& lang, const std::string& region, const std::string& script);

    void onAppInstalled(const std::string& appId);
    void onAppUninstalled(const std::string& appId);
    void onAppScanFinished(ScanMode mode, const AppDescMaps& scanned_apps);
    void onPackageStatusChanged(const std::string& appId, const PackageStatus& status);

    void publishListApps();
    void publishOneAppChange(AppPackagePtr app_desc, const std::string& change, AppStatusChangeEvent event);

    // variables
    AppPackageScanner m_appScanner;
    AppDescMaps m_appDescMaps;
    bool m_isFirstFullScanStarted;
    std::vector<std::string> m_scanReason;
};

#endif // PACKAGE_APPPACKAGEMANAGER_H_
