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

#include "extensions/webos_base/base_extension.h"

#include "core/base/jutil.h"
#include "core/base/lsutils.h"
#include "core/base/prerequisite_monitor.h"
#include "core/bus/appmgr_service.h"
#include "core/launch_point/launch_point_manager.h"
#include "core/lifecycle/app_info.h"
#include "core/lifecycle/app_life_manager.h"
#include "core/module/service_observer.h"
#include "core/module/subscriber_of_appinstalld.h"
#include "core/module/subscriber_of_bootd.h"
#include "core/module/subscriber_of_configd.h"
#include "core/module/subscriber_of_lsm.h"
#include "core/package/application_manager.h"
#include "extensions/webos_base/base_logs.h"
#include "extensions/webos_base/base_settings.h"

BaseExtension::BaseExtension()
{

}

BaseExtension::~BaseExtension()
{
}

void BaseExtension::init(PrerequisiteMonitor& prerequisite_monitor)
{
    BaseSettingsImpl::instance().Load(NULL);

    // to check first app launch finished
    AppLifeManager::instance().signal_launching_finished.connect(boost::bind(&BaseExtension::OnLaunchingFinished, this, _1));

    // set extension handlers for lifecycle interface
    AppLifeManager::instance().setApplifeitemFactory(app_launching_item_factory_);
    AppLifeManager::instance().setPrelauncherHandler(prelauncher_);
    AppLifeManager::instance().setMemoryCheckerHandler(memory_checker_);
    AppLifeManager::instance().setLastappHandler(lastapp_handler_);

    // set extension handlers for package interface
    ApplicationManager::instance().SetAppScanFilter(app_scan_filter_);
    ApplicationManager::instance().SetAppDescriptionFactory(app_description_factory_);

    // set extension handlers for launch point interface
    LaunchPointManager::instance().SetDbHandler(db_handler_);
    LaunchPointManager::instance().SetOrderingHandler(ordering_handler_);
    LaunchPointManager::instance().SetLaunchPointFactory(launch_point_factory_);
}

void BaseExtension::OnReady()
{
}

AppDesc4BasicPtr BaseExtension::GetAppDesc(const std::string& app_id)
{
    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if (!app_desc)
        return nullptr;
    return std::static_pointer_cast<ApplicationDescription4Base>(app_desc);
}

void BaseExtension::OnLaunchingFinished(AppLaunchingItemPtr item)
{
    AppLaunchingItem4BasePtr basic_item = std::static_pointer_cast<AppLaunchingItem4Base>(item);

    if (basic_item->errText().empty())
        AppLifeManager::instance().setLastLoadingApp(basic_item->appId());
}

