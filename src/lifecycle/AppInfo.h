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

#ifndef APP_INFO_H
#define APP_INFO_H

#include <lifecycle/AppLifeStatus.h>
#include <map>
#include <memory>
#include <string>

#include <pbnjson.hpp>


const std::string CATEGORY_ON_STORE_APPS = "GAME_APPS";
const std::string UPDATE_TYPE_OPTIONAL = "Y";
const std::string UPDATE_TYPE_REQUIRED = "F";
const std::string UPDATE_TYPE_AUTO_REQUIRED = "K";
const std::string OUT_OF_SERVICE = "D";

class AppInfo {
public:
    AppInfo(const std::string& app_id);
    virtual ~AppInfo();

    // getter list
    bool executionLock() const
    {
        return !m_executionLock;
    }
    const std::string& appId() const
    {
        return m_appId;
    }
    bool isRemoveFlagged() const
    {
        return m_removalFlag;
    }
    bool preloadModeOn() const
    {
        return m_preloadModeOn;
    }
    double lastLaunchTime() const
    {
        return m_lastLaunchTime;
    }
    LifeStatus lifeStatus() const
    {
        return m_lifeStatus;
    }
    RuntimeStatus runtimeStatus() const
    {
        return m_runtimeStatus;
    }

    // setter list
    void setExecutionLock(bool v = true)
    {
        m_executionLock = v;
    }
    void setRemovalFlag(bool v = true)
    {
        m_removalFlag = v;
    }
    void setPreloadMode(bool mode)
    {
        m_preloadModeOn = mode;
    }
    void setLastLaunchTime(double launch_time)
    {
        m_lastLaunchTime = launch_time;
    }
    void setLifeStatus(const LifeStatus& status);
    void setRuntimeStatus(RuntimeStatus status)
    {
        m_runtimeStatus = status;
    }

private:
    std::string m_appId;
    bool m_executionLock;
    bool m_removalFlag;
    bool m_preloadModeOn;
    double m_lastLaunchTime;
    LifeStatus m_lifeStatus;
    RuntimeStatus m_runtimeStatus;
};

typedef std::shared_ptr<const AppInfo> AppInfoConstPtr;
typedef std::shared_ptr<AppInfo> AppInfoPtr;
typedef std::map<std::string, AppInfoPtr> AppInfoList;

#endif
