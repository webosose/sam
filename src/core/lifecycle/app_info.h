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

#include <map>
#include <memory>
#include <string>

#include <pbnjson.hpp>

#include "core/lifecycle/app_life_status.h"

const std::string CATEGORY_ON_STORE_APPS = "GAME_APPS";
const std::string UPDATE_TYPE_OPTIONAL = "Y";
const std::string UPDATE_TYPE_REQUIRED = "F";
const std::string UPDATE_TYPE_AUTO_REQUIRED = "K";
const std::string OUT_OF_SERVICE = "D";

class AppInfo;

typedef std::shared_ptr<const AppInfo> AppInfoConstPtr;
typedef std::shared_ptr<AppInfo> AppInfoPtr;
typedef std::map<std::string, AppInfoPtr> AppInfoList;

class AppInfo {
public:
    AppInfo(const std::string& app_id);
    ~AppInfo();

    // getter list
    bool can_execute() const
    {
        return !m_execution_lock;
    }
    const std::string& app_id() const
    {
        return m_app_id;
    }
    bool is_remove_flagged() const
    {
        return m_removal_flag;
    }
    bool preload_mode_on() const
    {
        return m_preload_mode_on;
    }
    double last_launch_time() const
    {
        return m_last_launch_time;
    }
    LifeStatus life_status() const
    {
        return m_life_status;
    }
    RuntimeStatus runtime_status() const
    {
        return m_runtime_status;
    }
    const pbnjson::JValue& virtual_launch_params() const
    {
        return m_virtual_launch_params;
    }

    // setter list
    void set_execution_lock(bool v = true)
    {
        m_execution_lock = v;
    }
    void set_removal_flag(bool v = true)
    {
        m_removal_flag = v;
    }
    void set_preload_mode(bool mode)
    {
        m_preload_mode_on = mode;
    }
    void set_last_launch_time(double launch_time)
    {
        m_last_launch_time = launch_time;
    }
    void set_life_status(const LifeStatus& status);
    void set_runtime_status(RuntimeStatus status)
    {
        m_runtime_status = status;
    }
    void set_virtual_launch_params(const pbnjson::JValue& params)
    {
        m_virtual_launch_params = params.duplicate();
    }

private:
    std::string m_app_id;
    bool m_execution_lock;
    bool m_removal_flag;
    bool m_preload_mode_on;
    double m_last_launch_time;
    LifeStatus m_life_status;
    RuntimeStatus m_runtime_status;
    pbnjson::JValue m_virtual_launch_params;
};

#endif
