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

#ifndef MAIN_DAEMON_H
#define MAIN_DAEMON_H

#include <launchpoint/handler/DBHandler.h>
#include <launchpoint/handler/OrderingHandler.h>
#include <launchpoint/launch_point/LaunchPointFactory.h>
#include "package/AppPackageManager.h"
#include "lifecycle/stage/appitem/LaunchAppItem.h"
#include <luna-service2/lunaservice.h>
#include <prerequisite/BootdPrerequisiteItem.h>
#include <prerequisite/ConfigdPrerequisiteItem.h>
#include <prerequisite/PrerequisiteItem.h>

class MainDaemon : public PrerequisiteItemListener {
public:
    MainDaemon();
    virtual ~MainDaemon();

    void start();
    void stop();

    // PrerequisiteItemListener
    virtual void onPrerequisiteItemStatusChanged(PrerequisiteItem* item) override;


    // APIs
    void onLaunchPointsListChanged(const pbnjson::JValue& launchPoints);
    void onLaunchPointChanged(const std::string& change, const pbnjson::JValue& launchPoint);

    void onForegroundAppChanged(const std::string& app_id);
    void onExtraForegroundInfoChanged(const pbnjson::JValue& foreground_info);
    void onLifeCycleEventGenarated(const pbnjson::JValue& event);

    void onListAppsChanged(const pbnjson::JValue& apps, const std::vector<std::string>& changes, bool dev);
    void onOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev);
    void onAppStatusChanged(AppStatusChangeEvent event, AppPackagePtr app_desc);

private:
    void onLaunchingFinished(LaunchAppItemPtr item);

    GMainLoop *m_mainLoop;

    ConfigdPrerequisiteItem m_configdPrerequisiteItem;
    BootdPrerequisiteItem m_bootdPrerequisiteItem;
};

#endif
