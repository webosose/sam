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

#ifndef APP_INFO_MANAGER_H_
#define APP_INFO_MANAGER_H_

#include <pbnjson.hpp>

#include "core/base/singleton.h"
#include "core/lifecycle/app_info.h"
#include "core/package/application_manager.h"

struct RunningInfo
{
    std::string app_id;
    std::string pid;
    std::string webprocid;
    RunningInfo(const std::string& _app_id, const std::string& _pid, const std::string& _webprocid)
        : app_id(_app_id), pid(_pid), webprocid(_webprocid)
    {}
};

typedef std::shared_ptr<RunningInfo> RunningInfoPtr;
typedef std::list<RunningInfoPtr> RunningInfoList;
typedef std::map<std::string, pbnjson::JValue> UpdateInfoList;

class AppInfoManager: public Singleton<AppInfoManager>
{
public:
    AppInfoManager();
    ~AppInfoManager();

    void Init();

    ////////////////////////////////////////////////////
    /// information per app
    ////////////////////////////////////////////////////

    // getter list
    bool can_execute(const std::string& app_id);
    bool is_remove_flagged(const std::string& app_id);
    const std::string& pid(const std::string& app_id);
    const std::string& webprocid(const std::string& app_id);
    bool preload_mode_on(const std::string& app_id);
    bool is_out_of_service(const std::string& app_id);
    bool has_update(const std::string& app_id);
    const std::string update_category(const std::string& app_id);
    const std::string update_type(const std::string& app_id);
    const std::string update_version(const std::string& app_id);
    double last_launch_time(const std::string& app_id);
    LifeStatus life_status(const std::string& app_id);
    RuntimeStatus runtime_status(const std::string& app_id);
    const pbnjson::JValue& virtual_launch_params(const std::string& app_id);

    // setter list
    void set_execution_lock(const std::string& app_id, bool v=true);
    void set_removal_flag(const std::string& app_id, bool v=true);
    void set_preload_mode(const std::string& app_id, bool mode);
    void set_last_launch_time(const std::string& app_id, double launch_time);
    void set_life_status(const std::string& app_id, const LifeStatus& status);
    void set_runtime_status(const std::string& app_id, RuntimeStatus status);
    void set_virtual_launch_params(const std::string& app_id, const pbnjson::JValue& params);

    ////////////////////////////////////////////////////
    /// common functions
    ////////////////////////////////////////////////////
    void remove_app_info(const std::string& app_id);

    // life status
    std::string life_status_to_string(const LifeStatus& status);
    void get_app_ids_by_life_status(const LifeStatus& status, std::vector<std::string>& app_list);

    ////////////////////////////////////////////////////
    /// information list
    ////////////////////////////////////////////////////

    // out of service info
    void add_out_of_service_info(const std::string& app_id) { m_out_of_service_info_list.push_back(app_id); }
    void reset_all_out_of_service_info();

    // update info
    void add_update_info(const std::string& app_id, const std::string& type, const std::string& category, const std::string& version);
    void reset_all_update_info();

    // running info
    const std::string& get_app_id_by_pid(const std::string& pid);
    void get_running_list(pbnjson::JValue& running_list, bool devmode_only = false);
    void get_running_app_ids(std::vector<std::string>& running_app_ids);
    RunningInfoPtr get_running_data(const std::string& app_id);
    void add_running_info(const std::string& app_id, const std::string& pid, const std::string& webprocid);
    void remove_running_info(const std::string& app_id);
    bool is_running(const std::string& app_id);

    // foreground info
    const std::string& get_last_foreground_app_id() const { return m_last_foreground_app_id; }
    const std::string& get_current_foreground_app_id() const { return m_current_foreground_app_id; }
    const pbnjson::JValue& get_json_foreground_info() const { return m_json_foreground_info; }
    void get_json_foreground_info_by_id(const std::string& app_id, pbnjson::JValue& info);
    const std::vector<std::string>& get_foreground_apps() const { return m_foreground_apps; }
    bool is_app_on_fullscreen(const std::string& app_id);
    bool is_app_on_foreground(const std::string& app_id);

    void set_last_foreground_app_id(const std::string& app_id) { m_last_foreground_app_id = app_id; }
    void set_current_foreground_app_id(const std::string& app_id) { m_current_foreground_app_id = app_id; }
    void set_foreground_info(const pbnjson::JValue& new_info) { m_json_foreground_info = new_info.duplicate(); }
    void set_foreground_apps(const std::vector<std::string>& new_apps) { m_foreground_apps = new_apps; }

private:
    friend class Singleton<AppInfoManager>;

    void OnAllAppRosterChanged(const AppDescMaps& all_apps);
    AppInfoPtr get_app_info(const std::string& app_id);
    AppInfoPtr get_app_info_for_setter(const std::string& app_id);
    AppInfoPtr get_app_info_for_getter(const std::string& app_id);

    AppInfoPtr      m_default_app_info;
    AppInfoList     m_appinfo_list;
    RunningInfoList m_running_list;
    UpdateInfoList  m_update_info_list;

    std::string     m_last_foreground_app_id;
    std::string     m_current_foreground_app_id;
    pbnjson::JValue m_json_foreground_info;

    std::vector<std::string> m_foreground_apps;
    std::vector<std::string> m_out_of_service_info_list;
};

#endif
