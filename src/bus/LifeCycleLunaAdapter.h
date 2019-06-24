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

#ifndef CORE_LIFECYCLE_LIFECYCLE_LUNA_ADATER_H_
#define CORE_LIFECYCLE_LIFECYCLE_LUNA_ADATER_H_

#include <bus/LunaTask.h>
#include <package/AppDescriptionScanner.h>

class LifeCycleLunaAdapter {
public:
    LifeCycleLunaAdapter();
    virtual ~LifeCycleLunaAdapter();

    void init();

private:
    void initLunaApiHandler();
    void noHandler(LunaTaskPtr task);
    void requestController(LunaTaskPtr task);
    void handleRequest(LunaTaskPtr task);
    void onReady();
    void onScanFinished(ScanMode mode, const AppDescMaps& scannced_apps);

    // api handler
    void launch(LunaTaskPtr task);
    void pause(LunaTaskPtr task);
    void closeByAppId(LunaTaskPtr task);
    void closeByAppIdForDev(LunaTaskPtr task);
    void closeAllApps(LunaTaskPtr task);
    void running(LunaTaskPtr task);
    void runningForDev(LunaTaskPtr task);
    void changeRunningAppId(LunaTaskPtr task);
    void getAppLifeEvents(LunaTaskPtr task);
    void getAppLifeStatus(LunaTaskPtr task);
    void getForegroundAppInfo(LunaTaskPtr task);
    void lockApp(LunaTaskPtr task);
    void registerApp(LunaTaskPtr task);
    void registerNativeApp(LunaTaskPtr task);
    void notifyAlertClosed(LunaTaskPtr task);

    void onForegroundAppChanged(const std::string& app_id);
    void onExtraForegroundInfoChanged(const pbnjson::JValue& foreground_info);
    void onLifeCycleEventGenarated(const pbnjson::JValue& event);

    // TODO : make them retired via sangria
    // api handler which should be deprecated
    void close(LunaTaskPtr task);
    void notifySplashTimeout(LunaTaskPtr task);
    void onLaunch(LunaTaskPtr task);

    std::vector<LunaTaskPtr> m_pendingTasksOnReady;
    std::vector<LunaTaskPtr> m_pendingTasksOnScanner;
};

#endif  // CORE_LIFECYCLE_LIFECYCLE_LUNA_ADAPTER_H_
