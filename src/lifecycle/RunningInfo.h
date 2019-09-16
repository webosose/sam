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

#ifndef APP_INFO_H
#define APP_INFO_H

#include <lifecycle/LifecycleRouter.h>
#include <map>
#include <memory>
#include <string>
#include "launchpoint/launch_point/LaunchPoint.h"
#include "util/Logger.h"

#include <pbnjson.hpp>

class RunningInfo {
public:
    RunningInfo(const std::string& appId)
        : m_appId(appId),
          m_display(""),
          m_isPreloadMode(false),
          m_lastLaunchTime(0),
          m_lifeStatus(LifeStatus::STOP),
          m_runtimeStatus(RuntimeStatus::STOP)
    {
    }
    virtual ~RunningInfo()
    {
    }

    void setAppId(const std::string& appId)
    {
        m_appId = appId;
    }
    const std::string& getAppId() const
    {
        return m_appId;
    }

    void setDisplay(const std::string& display)
    {
        m_display = display;
    }
    const std::string& getDisplay() const
    {
        return m_display;
    }

    void setPid(const std::string& pid)
    {
        m_pid = pid;
    }
    const std::string& getPid() const
    {
        return m_pid;
    }

    void setWebprocid(const std::string& webprocid)
    {
        m_webprocid = webprocid;
    }
    const std::string& getWebprocid() const
    {
        return m_webprocid;
    }

    void setPreloadMode(bool mode)
    {
        m_isPreloadMode = mode;
    }
    bool getPreloadMode() const
    {
        return m_isPreloadMode;
    }

    void setLastLaunchTime(double launch_time)
    {
        m_lastLaunchTime = launch_time;
    }
    double getLastLaunchTime() const
    {
        return m_lastLaunchTime;
    }

    void setLifeStatus(const LifeStatus& status)
    {
        Logger::debug("RunningInfo", __FUNCTION__, Logger::format("(%s) appId: %s, status: %d", __FUNCTION__, m_appId.c_str(), (int) status));
        m_lifeStatus = status;
    }
    LifeStatus getLifeStatus() const
    {
        return m_lifeStatus;
    }

    void setRuntimeStatus(RuntimeStatus status)
    {
        m_runtimeStatus = status;
    }
    RuntimeStatus getRuntimeStatus() const
    {
        return m_runtimeStatus;
    }

private:
    LaunchPointPtr m_launchPointPtr;

    std::string m_appId;
    std::string m_display;
    std::string m_pid;
    std::string m_webprocid;

    bool m_isPreloadMode;
    double m_lastLaunchTime;

    LifeStatus m_lifeStatus;
    RuntimeStatus m_runtimeStatus;
};

typedef std::shared_ptr<RunningInfo> RunningInfoPtr;
typedef std::map<std::string, RunningInfoPtr> RunningInfoList;

#endif
