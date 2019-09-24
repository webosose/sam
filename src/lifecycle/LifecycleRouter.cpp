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

#include "LifecycleRouter.h"

#include "base/RunningAppList.h"
#include "base/LaunchPointList.h"

const LifecycleRoutePolicy LifecycleRouter::INVALID_ROUTE_POLICY = {
    LifeStatus::INVALID, RouteAction::IGNORE, RouteLog::ERROR
};

LifecycleRouter::LifecycleRouter()
{
}

LifecycleRouter::~LifecycleRouter()
{
}

void LifecycleRouter::initialize()
{
    m_lifecycleRouteMap[LifeStatus::STOP] = {
        { LifeStatus::STOP, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::PRELOADING, RouteAction::SET, RouteLog::NONE },  // fresh launch for preload
        { LifeStatus::LAUNCHING, RouteAction::SET, RouteLog::NONE },  // fresh launch
        { LifeStatus::RELAUNCHING, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::FOREGROUND, RouteAction::IGNORE, RouteLog::WARN },  // foreground on SAM respawned
        { LifeStatus::BACKGROUND, RouteAction::IGNORE, RouteLog::WARN },  // background on SAM respawned
        { LifeStatus::CLOSING, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::PAUSING, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::RUNNING, RouteAction::CONVERT, RouteLog::WARN }
    };
    m_lifestatusConvMap[LifeStatus::STOP] = {
        { LifeStatus::RUNNING, {LifeStatus::BACKGROUND, RouteAction::SET, RouteLog::NONE }}
    };

    m_lifecycleRouteMap[LifeStatus::PRELOADING] = {
        { LifeStatus::STOP, RouteAction::SET, RouteLog::WARN },  // app crash
        { LifeStatus::PRELOADING, RouteAction::IGNORE, RouteLog::ERROR }, // consecutive preload launch
        { LifeStatus::LAUNCHING, RouteAction::SET, RouteLog::CHECK }, // launch while preloading
        { LifeStatus::RELAUNCHING, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::FOREGROUND, RouteAction::SET, RouteLog::WARN },  // foreground on SAM respawned
        { LifeStatus::BACKGROUND, RouteAction::IGNORE, RouteLog::NONE },  // background on SAM respawned
        { LifeStatus::CLOSING, RouteAction::SET, RouteLog::CHECK }, // close while preloading
        { LifeStatus::PAUSING, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::RUNNING, RouteAction::CONVERT, RouteLog::WARN }
    };
    m_lifestatusConvMap[LifeStatus::PRELOADING] = {
        { LifeStatus::RUNNING, {LifeStatus::BACKGROUND, RouteAction::SET, RouteLog::NONE }}
    };

    m_lifecycleRouteMap[LifeStatus::LAUNCHING] = {
        { LifeStatus::STOP, RouteAction::SET, RouteLog::WARN },  // app crash
        { LifeStatus::PRELOADING, RouteAction::IGNORE, RouteLog::ERROR }, // [should return false]
        { LifeStatus::LAUNCHING, RouteAction::IGNORE, RouteLog::CHECK }, // consecutive launch
        { LifeStatus::RELAUNCHING, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::FOREGROUND, RouteAction::SET, RouteLog::NONE },  // normal flow
        { LifeStatus::BACKGROUND, RouteAction::IGNORE, RouteLog::WARN },  // keep current status
        { LifeStatus::CLOSING, RouteAction::SET, RouteLog::CHECK }, // close while preloading
        { LifeStatus::PAUSING, RouteAction::IGNORE, RouteLog::CHECK },
        { LifeStatus::RUNNING, RouteAction::IGNORE, RouteLog::NONE }
    };

    m_lifecycleRouteMap[LifeStatus::RELAUNCHING] = {
        { LifeStatus::STOP, RouteAction::SET, RouteLog::WARN },  // app crash
        { LifeStatus::PRELOADING, RouteAction::IGNORE, RouteLog::ERROR }, // [should return false]
        { LifeStatus::LAUNCHING, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::RELAUNCHING, RouteAction::IGNORE, RouteLog::CHECK }, // consecutive launch
        { LifeStatus::FOREGROUND, RouteAction::SET, RouteLog::NONE },  // normal flow
        { LifeStatus::BACKGROUND, RouteAction::IGNORE, RouteLog::WARN },  // keep current status
        { LifeStatus::CLOSING, RouteAction::SET, RouteLog::CHECK }, // close while preloading
        { LifeStatus::PAUSING, RouteAction::IGNORE, RouteLog::CHECK },
        { LifeStatus::RUNNING, RouteAction::IGNORE, RouteLog::NONE }
    };

    m_lifecycleRouteMap[LifeStatus::FOREGROUND] = {
        { LifeStatus::STOP, RouteAction::SET, RouteLog::WARN },  // app crash
        { LifeStatus::PRELOADING, RouteAction::IGNORE, RouteLog::ERROR }, // [should return false]
        { LifeStatus::LAUNCHING, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::RELAUNCHING, RouteAction::IGNORE, RouteLog::WARN },  // keep current status
        { LifeStatus::FOREGROUND, RouteAction::IGNORE, RouteLog::WARN },  // keep current status
        { LifeStatus::BACKGROUND, RouteAction::SET, RouteLog::NONE },  // app switched or paused
        { LifeStatus::CLOSING, RouteAction::SET, RouteLog::NONE },  // close request
        { LifeStatus::PAUSING, RouteAction::SET, RouteLog::NONE },
        { LifeStatus::RUNNING, RouteAction::IGNORE, RouteLog::NONE }
    };

    m_lifecycleRouteMap[LifeStatus::BACKGROUND] = {
        { LifeStatus::STOP, RouteAction::SET, RouteLog::WARN },  // app crash
        { LifeStatus::PRELOADING, RouteAction::IGNORE, RouteLog::ERROR }, // [should return false]
        { LifeStatus::LAUNCHING, RouteAction::CONVERT, RouteLog::ERROR },
        { LifeStatus::RELAUNCHING, RouteAction::SET, RouteLog::NONE },
        { LifeStatus::FOREGROUND, RouteAction::SET, RouteLog::WARN },  // unexpected
        { LifeStatus::BACKGROUND, RouteAction::IGNORE, RouteLog::WARN },  // unexpected
        { LifeStatus::CLOSING, RouteAction::SET, RouteLog::NONE },  // close request
        { LifeStatus::PAUSING, RouteAction::IGNORE, RouteLog::NONE },  // keep current status
        { LifeStatus::RUNNING, RouteAction::IGNORE, RouteLog::NONE }
    };
    m_lifestatusConvMap[LifeStatus::BACKGROUND] = {
        { LifeStatus::LAUNCHING, {LifeStatus::RELAUNCHING, RouteAction::SET, RouteLog::NONE }}
    };

    m_lifecycleRouteMap[LifeStatus::CLOSING] = {
        { LifeStatus::STOP, RouteAction::SET, RouteLog::NONE },  // normal flow
        { LifeStatus::PRELOADING, RouteAction::IGNORE, RouteLog::ERROR }, // [should return false]
        { LifeStatus::LAUNCHING, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::RELAUNCHING, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::FOREGROUND, RouteAction::IGNORE, RouteLog::CHECK }, // unexpected
        { LifeStatus::BACKGROUND, RouteAction::IGNORE, RouteLog::WARN },  // unexpected
        { LifeStatus::CLOSING, RouteAction::IGNORE, RouteLog::NONE },  // keep current status
        { LifeStatus::PAUSING, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::RUNNING, RouteAction::IGNORE, RouteLog::ERROR }
    };

    m_lifecycleRouteMap[LifeStatus::PAUSING] = {
        { LifeStatus::STOP, RouteAction::SET, RouteLog::WARN },  // normal flow
        { LifeStatus::PRELOADING, RouteAction::IGNORE, RouteLog::ERROR }, // [should return false]
        { LifeStatus::LAUNCHING, RouteAction::IGNORE, RouteLog::ERROR },
        { LifeStatus::RELAUNCHING, RouteAction::SET, RouteLog::CHECK },
        { LifeStatus::FOREGROUND, RouteAction::IGNORE, RouteLog::WARN },  // unexpected
        { LifeStatus::BACKGROUND, RouteAction::SET, RouteLog::NONE },  // unexpected
        { LifeStatus::CLOSING, RouteAction::SET, RouteLog::CHECK }, // keep current status
        { LifeStatus::PAUSING, RouteAction::IGNORE, RouteLog::WARN },
        { LifeStatus::RUNNING, RouteAction::IGNORE, RouteLog::ERROR }
    };

    m_lifeEventConvMap[LifeStatus::INVALID] = LifeEvent::INVALID;
    m_lifeEventConvMap[LifeStatus::STOP] = LifeEvent::STOP;
    m_lifeEventConvMap[LifeStatus::PRELOADING] = LifeEvent::PRELOAD;
    m_lifeEventConvMap[LifeStatus::LAUNCHING] = LifeEvent::LAUNCH;
    m_lifeEventConvMap[LifeStatus::RELAUNCHING] = LifeEvent::LAUNCH;
    m_lifeEventConvMap[LifeStatus::FOREGROUND] = LifeEvent::FOREGROUND;
    m_lifeEventConvMap[LifeStatus::BACKGROUND] = LifeEvent::BACKGROUND;
    m_lifeEventConvMap[LifeStatus::CLOSING] = LifeEvent::CLOSE;
    m_lifeEventConvMap[LifeStatus::PAUSING] = LifeEvent::PAUSE;
    m_lifeEventConvMap[LifeStatus::RUNNING] = LifeEvent::INVALID;

    m_runtimeStatusConvMap[RuntimeStatus::STOP] = LifeStatus::STOP;
    m_runtimeStatusConvMap[RuntimeStatus::LAUNCHING] = LifeStatus::LAUNCHING;
    m_runtimeStatusConvMap[RuntimeStatus::PRELOADING] = LifeStatus::PRELOADING;
    m_runtimeStatusConvMap[RuntimeStatus::RUNNING] = LifeStatus::RUNNING;
    m_runtimeStatusConvMap[RuntimeStatus::REGISTERED] = LifeStatus::RUNNING;
    m_runtimeStatusConvMap[RuntimeStatus::PAUSING] = LifeStatus::PAUSING;
    m_runtimeStatusConvMap[RuntimeStatus::CLOSING] = LifeStatus::CLOSING;

    // NOTE: define seperate route policy based on app type if required
    // currently this route map focuses on native apps
    // it doesn't matter for web app and qml apps
    m_runtimeRouteMap[RuntimeStatus::STOP] = {
        { RuntimeStatus::STOP, RouteAction::IGNORE },
        { RuntimeStatus::LAUNCHING, RouteAction::SET },
        { RuntimeStatus::PRELOADING, RouteAction::SET },
        { RuntimeStatus::RUNNING, RouteAction::SET },
        { RuntimeStatus::REGISTERED, RouteAction::IGNORE },
        { RuntimeStatus::CLOSING, RouteAction::IGNORE },
        { RuntimeStatus::PAUSING, RouteAction::IGNORE }
    };

    m_runtimeRouteMap[RuntimeStatus::LAUNCHING] = {
        { RuntimeStatus::STOP, RouteAction::SET },
        { RuntimeStatus::LAUNCHING, RouteAction::IGNORE },
        { RuntimeStatus::PRELOADING, RouteAction::IGNORE },
        { RuntimeStatus::RUNNING, RouteAction::SET },
        { RuntimeStatus::REGISTERED, RouteAction::IGNORE },
        { RuntimeStatus::CLOSING, RouteAction::IGNORE },
        { RuntimeStatus::PAUSING, RouteAction::IGNORE }
    };

    m_runtimeRouteMap[RuntimeStatus::PRELOADING] = {
        { RuntimeStatus::STOP, RouteAction::SET },
        { RuntimeStatus::LAUNCHING, RouteAction::IGNORE },
        { RuntimeStatus::PRELOADING, RouteAction::IGNORE },
        { RuntimeStatus::RUNNING, RouteAction::SET },
        { RuntimeStatus::REGISTERED, RouteAction::IGNORE },
        { RuntimeStatus::CLOSING, RouteAction::IGNORE },
        { RuntimeStatus::PAUSING, RouteAction::IGNORE }
    };

    m_runtimeRouteMap[RuntimeStatus::RUNNING] = {
        { RuntimeStatus::STOP, RouteAction::SET },
        { RuntimeStatus::LAUNCHING, RouteAction::IGNORE },
        { RuntimeStatus::PRELOADING, RouteAction::IGNORE },
        { RuntimeStatus::RUNNING, RouteAction::IGNORE },
        { RuntimeStatus::REGISTERED, RouteAction::SET },
        { RuntimeStatus::CLOSING, RouteAction::SET },
        { RuntimeStatus::PAUSING, RouteAction::IGNORE }
    };

    m_runtimeRouteMap[RuntimeStatus::REGISTERED] = {
        { RuntimeStatus::STOP, RouteAction::SET },
        { RuntimeStatus::LAUNCHING, RouteAction::IGNORE },
        { RuntimeStatus::PRELOADING, RouteAction::IGNORE },
        { RuntimeStatus::RUNNING, RouteAction::IGNORE },
        { RuntimeStatus::REGISTERED, RouteAction::IGNORE },
        { RuntimeStatus::CLOSING, RouteAction::SET },
        { RuntimeStatus::PAUSING, RouteAction::IGNORE }
    };

    m_runtimeRouteMap[RuntimeStatus::CLOSING] = {
        { RuntimeStatus::STOP, RouteAction::SET },
        { RuntimeStatus::LAUNCHING, RouteAction::IGNORE },
        { RuntimeStatus::PRELOADING, RouteAction::IGNORE },
        { RuntimeStatus::RUNNING, RouteAction::IGNORE },
        { RuntimeStatus::REGISTERED, RouteAction::IGNORE },
        { RuntimeStatus::CLOSING, RouteAction::IGNORE },
        { RuntimeStatus::PAUSING, RouteAction::IGNORE }
    };
}

const LifecycleRoutePolicy& LifecycleRouter::getConvRoutePolilcy(LifeStatus current, LifeStatus next)
{
    if (m_lifestatusConvMap.count(current) == 0) {
        return INVALID_ROUTE_POLICY;
    }

    if (m_lifestatusConvMap[current].count(next) == 0) {
        return INVALID_ROUTE_POLICY;
    }

    return m_lifestatusConvMap[current][next];
}

const LifecycleRoutePolicy& LifecycleRouter::getLifeCycleRoutePolicy(LifeStatus current, LifeStatus next)
{
    if (m_lifecycleRouteMap.count(current) == 0) {
        return INVALID_ROUTE_POLICY;
    }
    auto it = std::find_if(m_lifecycleRouteMap[current].begin(), m_lifecycleRouteMap[current].end(),
                           [&next](const LifecycleRoutePolicy policy) {
        return next == std::get<0>(policy);
    });
    if (it == m_lifecycleRouteMap[current].end())
        return INVALID_ROUTE_POLICY;

    if (RouteAction::CONVERT == std::get<1>(*it)) {
        return getConvRoutePolilcy(current, next);
    }

    return (*it);
}

void LifecycleRouter::setRuntimeStatus(const std::string& appId, RuntimeStatus next)
{
    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId, true);

    auto it = std::find_if(m_runtimeRouteMap[runningApp->getRuntimeStatus()].begin(), m_runtimeRouteMap[runningApp->getRuntimeStatus()].end(), [&next](const RuntimeRoutePolicy policy) {
        return next == policy.first;
    });
    if (it == m_runtimeRouteMap[runningApp->getRuntimeStatus()].end() || (*it).second != RouteAction::SET) {
        Logger::info("LifecycleRouter", __FUNCTION__, appId, Logger::format("skip set runtime status: current(%d) next(%d)", runningApp->getRuntimeStatus(), next));
        return;
    }

    Logger::info("LifecycleRouter", __FUNCTION__, appId,
                 Logger::format("current(%s) next(%s)", RunningApp::toString(runningApp->getRuntimeStatus()), RunningApp::toString(next)));
    runningApp->setRuntimeStatus(next);
}

LifeStatus LifecycleRouter::getLifeStatusFromRuntimeStatus(RuntimeStatus runtime_status)
{
    LifeStatus next_status = LifeStatus::INVALID;
    if (m_runtimeStatusConvMap.count(runtime_status) != 0) {
        next_status = m_runtimeStatusConvMap[runtime_status];
    }
    return next_status;
}

LifeEvent LifecycleRouter::getLifeEventFromLifeStatus(LifeStatus status)
{
    if (m_lifeEventConvMap.count(status) == 0)
        return LifeEvent::INVALID;
    return m_lifeEventConvMap[status];
}
