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

#include <set>
#include <luna-service2/lunaservice.h>
#include <pbnjson.hpp>

#include "core/base/prerequisite_monitor.h"
#include "core/base/singleton.h"
#include "extensions/webos_base/launch_point/handler/db_handler_4_base.h"
#include "extensions/webos_base/launch_point/handler/ordering_handler_4_base.h"
#include "extensions/webos_base/launch_point/launch_point/launch_point_4_base.h"
#include "extensions/webos_base/launch_point/launch_point/launch_point_factory_4_base.h"
#include "extensions/webos_base/lifecycle/app_launching_item_factory_4_base.h"
#include "extensions/webos_base/lifecycle/last_app_handler_4_base.h"
#include "extensions/webos_base/lifecycle/memory_checker_4_base.h"
#include "extensions/webos_base/lifecycle/prelauncher_4_base.h"
#include "extensions/webos_base/package/app_description_factory_4_base.h"
#include "extensions/webos_base/package/application_description_4_base.h"
#include "extensions/webos_base/package/appscan_filter_4_base.h"

class BaseExtension: public Singleton<BaseExtension> {
public:
    void init(PrerequisiteMonitor& prerequisite_monitor);
    void OnReady();

    AppDesc4BasicPtr GetAppDesc(const std::string& app_id);

private:
    friend class Singleton<BaseExtension> ;
    BaseExtension();
    virtual ~BaseExtension();
    BaseExtension(BaseExtension const&);
    BaseExtension& operator=(const BaseExtension&);

    void OnLaunchingFinished(AppLaunchingItemPtr item);

    // lifecycle adapter
    AppLaunchingItemFactory4Base app_launching_item_factory_;
    Prelauncher4Base prelauncher_;
    MemoryChecker4Base memory_checker_;
    LastAppHandler4Base lastapp_handler_;

    // package adapter
    AppScanFilter4Base app_scan_filter_;
    AppDescriptionFactory4Base app_description_factory_;

    // launchpoint adapter
    DbHandler4Base db_handler_;
    OrderingHandler4Base ordering_handler_;
    LaunchPointFactory4Basic launch_point_factory_;

};

#endif
