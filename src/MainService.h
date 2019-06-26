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

#ifndef MAIN_SERVICE_H
#define MAIN_SERVICE_H

#include <base/WebOSService.h>
#include <base/PrerequisiteMonitor.h>
#include <launch_point/handler/DBHandler.h>
#include <launch_point/handler/OrderingHandler.h>
#include <launch_point/launch_point/LaunchPoint.h>
#include <launch_point/launch_point/LaunchPointFactory.h>
#include <lifecycle/stage/AppLaunchingItemFactory.h>
#include <lifecycle/stage/MemoryChecker.h>
#include <set>
#include <luna-service2/lunaservice.h>
#include <package/AppDescription.h>
#include <pbnjson.hpp>
#include <policy/LastAppHandler.h>

#include <util/Singleton.h>

class MainService: public WebOSService {
public:
    virtual ~MainService();

protected:
    virtual bool initialize();
    virtual bool terminate();

private:
    void onLaunchingFinished(AppLaunchingItemPtr item);

    // lifecycle adapter
    AppLaunchingItemFactory m_appLaunchingItemFactory;
    MemoryChecker m_memoryChecker;
    LastAppHandler m_lastappHandler;

    // launchpoint adapter
    DBHandler m_dbHandler;
    OrderingHandler m_orderingHandler;
    LaunchPointFactory m_launchPointFactory;
};

#endif
