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

#include <base/prerequisite_monitor.h>
#include <base_extension.h>
#include <bus/appmgr_service.h>
#include <launch_point/launch_point_manager.h>
#include <lifecycle/app_info.h>
#include <lifecycle/lifecycle_manager.h>
#include <module/service_observer.h>
#include <module/subscriber_of_appinstalld.h>
#include <module/subscriber_of_bootd.h>
#include <module/subscriber_of_configd.h>
#include <module/subscriber_of_lsm.h>
#include <package/PackageManager.h>
#include <setting/base_settings.h>
#include <util/base_logs.h>
#include <util/jutil.h>
#include <util/lsutils.h>

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
    LifecycleManager::instance().signal_launching_finished.connect(boost::bind(&BaseExtension::OnLaunchingFinished, this, _1));

    // set extension handlers for lifecycle interface
    LifecycleManager::instance().setApplifeitemFactory(m_appLaunchingItemFactory);
    LifecycleManager::instance().setPrelauncherHandler(m_prelauncher);
    LifecycleManager::instance().setMemoryCheckerHandler(m_memoryChecker);
    LifecycleManager::instance().setLastappHandler(m_lastappHandler);

    // set extension handlers for launch point interface
    LaunchPointManager::instance().setDbHandler(m_dbHandler);
    LaunchPointManager::instance().setOrderingHandler(m_orderingHandler);
    LaunchPointManager::instance().setLaunchPointFactory(m_launchPointFactory);
}

void BaseExtension::OnReady()
{
}

AppDescPtr BaseExtension::GetAppDesc(const std::string& app_id)
{
    AppDescPtr app_desc = PackageManager::instance().getAppById(app_id);
    if (!app_desc)
        return nullptr;
    return std::static_pointer_cast<AppDescription>(app_desc);
}

void BaseExtension::OnLaunchingFinished(AppLaunchingItemPtr item)
{
    AppLaunchingItem4BasePtr basic_item = std::static_pointer_cast<AppLaunchingItem4Base>(item);

    if (basic_item->errText().empty())
        LifecycleManager::instance().setLastLoadingApp(basic_item->appId());
}

