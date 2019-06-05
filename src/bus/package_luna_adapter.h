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

#ifndef CORE_BUS_PACKAGE_LUNA_ADATER_H_
#define CORE_BUS_PACKAGE_LUNA_ADATER_H_

#include <bus/luna_task.h>
#include <package/AppDescription.h>
#include <package/AppDescriptionScanner.h>
#include <package/PackageManager.h>

class PackageLunaAdapter {
public:
    PackageLunaAdapter();
    virtual ~PackageLunaAdapter();

    void init();

private:
    void initLunaApiHandler();
    void requestController(LunaTaskPtr task);
    void handleRequest(LunaTaskPtr task);
    void onReady();
    void onScanFinished(ScanMode mode, const AppDescMaps& scannced_apps);
    void onListAppsChanged(const pbnjson::JValue& apps, const std::vector<std::string>& changes, bool dev);
    void onOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev);
    void onAppStatusChanged(AppStatusChangeEvent event, AppDescPtr app_desc);

    void listApps(LunaTaskPtr task);
    void listAppsForDev(LunaTaskPtr task);
    void getAppStatus(LunaTaskPtr task);
    void getAppInfo(LunaTaskPtr task);
    void getAppBasePath(LunaTaskPtr task);

    std::vector<LunaTaskPtr> m_pendingTasksOnReady;
    std::vector<LunaTaskPtr> m_pendingTasksOnScanner;
};

#endif  // CORE_BUS_PACKGE_LUNA_ADAPTER_H_

