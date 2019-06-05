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

#ifndef BASE_EXTENSION_H
#define BASE_EXTENSION_H

#include <base/prerequisite_monitor.h>
#include <launch_point/handler/db_handler_4_base.h>
#include <launch_point/handler/ordering_handler_4_base.h>
#include <launch_point/launch_point/launch_point_4_base.h>
#include <launch_point/launch_point/launch_point_factory_4_base.h>
#include <lifecycle/app_launching_item_factory_4_base.h>
#include <lifecycle/last_app_handler_4_base.h>
#include <lifecycle/MemoryChecker.h>
#include <lifecycle/prelauncher_4_base.h>
#include <set>
#include <luna-service2/lunaservice.h>
#include <package/AppDescription.h>
#include <pbnjson.hpp>

#include <util/singleton.h>

class BaseExtension: public Singleton<BaseExtension> {
friend class Singleton<BaseExtension> ;
public:
    void init(PrerequisiteMonitor& prerequisite_monitor);
    void OnReady();

    AppDescPtr GetAppDesc(const std::string& app_id);

private:
    BaseExtension();
    virtual ~BaseExtension();
    BaseExtension(BaseExtension const&);
    BaseExtension& operator=(const BaseExtension&);

    void OnLaunchingFinished(AppLaunchingItemPtr item);

    // lifecycle adapter
    AppLaunchingItemFactory4Base m_appLaunchingItemFactory;
    Prelauncher4Base m_prelauncher;
    MemoryChecker m_memoryChecker;
    LastAppHandler4Base m_lastappHandler;

    // launchpoint adapter
    DbHandler4Base m_dbHandler;
    OrderingHandler4Base m_orderingHandler;
    LaunchPointFactory4Basic m_launchPointFactory;

};

#endif
