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

#ifndef APP_LIFE_STATUS_H_
#define APP_LIFE_STATUS_H_

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "base/RunningApp.h"

enum class RouteAction : int8_t {
    SET = 0,
    IGNORE,
    CONVERT,
};

enum class RouteLog : int8_t {
    NONE = 0,   // normal flow as expected
    CHECK,      // for checking improvement point later
    WARN,       // can happen
    ERROR,      // should not happen
};

typedef std::tuple<LifeStatus, RouteAction, RouteLog> LifecycleRoutePolicy;
typedef std::map<LifeStatus, LifecycleRoutePolicy> LifeStatusConvertPolicy;
typedef std::pair<RuntimeStatus, RouteAction> RuntimeRoutePolicy;

class LifecycleRouter {
public:
    LifecycleRouter();
    ~LifecycleRouter();

    void initialize();
    const LifecycleRoutePolicy& getLifeCycleRoutePolicy(LifeStatus current, LifeStatus next);
    void setRuntimeStatus(const std::string& appId, RuntimeStatus next);
    LifeStatus getLifeStatusFromRuntimeStatus(RuntimeStatus runtime_status);
    LifeEvent getLifeEventFromLifeStatus(LifeStatus status);

private:
    static const LifecycleRoutePolicy INVALID_ROUTE_POLICY;

    const LifecycleRoutePolicy& getConvRoutePolilcy(LifeStatus current, LifeStatus next);

    std::map<LifeStatus, std::vector<LifecycleRoutePolicy>> m_lifecycleRouteMap;
    std::map<LifeStatus, LifeStatusConvertPolicy> m_lifestatusConvMap;
    std::map<RuntimeStatus, std::vector<RuntimeRoutePolicy>> m_runtimeRouteMap;
    std::map<RuntimeStatus, LifeStatus> m_runtimeStatusConvMap;
    std::map<LifeStatus, LifeEvent> m_lifeEventConvMap;
};

#endif // APP_LIFE_STATUS_H_

