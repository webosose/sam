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

#include "core/lifecycle/app_info_manager.h"

#include <algorithm>

#include "core/base/logging.h"

const std::string& NULL_STR = "";
const std::string& DEFAULT_NULL_APP = "@APP_INFO_DEFAULT_APP@";

AppInfoManager::AppInfoManager()
    : m_json_foreground_info(pbnjson::Array())
{
    m_default_app_info = std::make_shared<AppInfo>(DEFAULT_NULL_APP);
}

AppInfoManager::~AppInfoManager()
{
}

void AppInfoManager::Init() {
  ApplicationManager::instance().signalAllAppRosterChanged.connect(
      boost::bind(&AppInfoManager::OnAllAppRosterChanged, this, _1));
}

AppInfoPtr AppInfoManager::get_app_info_for_setter(const std::string& app_id)
{
    if(app_id.empty())
    {
        LOG_ERROR(MSGID_APPINFO_ERR, 1, PMLOGKS("reason", "empty_app_id"), "");
        return NULL;
    }

    AppInfoPtr app_info = get_app_info(app_id);
    if(app_info == NULL)
    {
        app_info = std::make_shared<AppInfo>(app_id);
        if(app_info == NULL)
        {
            LOG_ERROR(MSGID_APPINFO_ERR, 1, PMLOGKS("reason", "failed_create_new_app_info"), "");
            return NULL;
        }
        m_appinfo_list[app_id] = app_info;
        LOG_INFO(MSGID_APPINFO, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "item_added"), "");
    }
    return app_info;
}

AppInfoPtr AppInfoManager::get_app_info_for_getter(const std::string& app_id)
{
    AppInfoPtr app_info = get_app_info(app_id);
    if(app_info == NULL)
        return m_default_app_info;
    return app_info;
}

AppInfoPtr AppInfoManager::get_app_info(const std::string& app_id)
{
    auto it = m_appinfo_list.find(app_id);
    if(it == m_appinfo_list.end())
        return NULL;
    return m_appinfo_list[app_id];
}

void AppInfoManager::remove_app_info(const std::string& app_id)
{
  LifeStatus current_status = life_status(app_id);
  if (LifeStatus::INVALID == current_status || LifeStatus::STOP == current_status) {
    LOG_INFO(MSGID_APPINFO, 2, PMLOGKS("app_id", app_id.c_str()),
                               PMLOGKS("status", "item_removed"), "");
    m_appinfo_list.erase(app_id);
  } else {
    LOG_INFO(MSGID_APPINFO, 2, PMLOGKS("app_id", app_id.c_str()),
                               PMLOGKS("status", "set_removed_flag_for_lazy_release"), "");
    set_removal_flag(app_id, true);
  }
}


//////////////////////////////////////////////////////////////////
/// setters
//////////////////////////////////////////////////////////////////
void AppInfoManager::set_execution_lock(const std::string& app_id, bool v)
{
    AppInfoPtr app_info = NULL;
    if(v == false) {
        app_info = get_app_info(app_id);
        if(app_info == NULL) return;
    } else {
        app_info = get_app_info_for_setter(app_id);
        if(app_info == NULL) return;
    }
    app_info->set_execution_lock(v);
}

void AppInfoManager::set_removal_flag(const std::string& app_id, bool v)
{
    AppInfoPtr app_info = get_app_info_for_setter(app_id);
    if(app_info == NULL)
        return;
    app_info->set_removal_flag(v);
}

void AppInfoManager::set_preload_mode(const std::string& app_id, bool mode)
{
    AppInfoPtr app_info = get_app_info_for_setter(app_id);
    if(app_info == NULL)
        return;
    app_info->set_preload_mode(mode);
}

void AppInfoManager::set_last_launch_time(const std::string& app_id, double launch_time)
{
    AppInfoPtr app_info = get_app_info_for_setter(app_id);
    if(app_info == NULL)
        return;
    app_info->set_last_launch_time(launch_time);
}

void AppInfoManager::set_life_status(const std::string& app_id, const LifeStatus& status)
{
    AppInfoPtr app_info = get_app_info_for_setter(app_id);
    if(app_info == NULL)
        return;
    app_info->set_life_status(status);
    if (app_info->is_remove_flagged())
      remove_app_info(app_id);
}

void AppInfoManager::set_runtime_status(const std::string& app_id, RuntimeStatus status)
{
    AppInfoPtr app_info = get_app_info_for_setter(app_id);
    if(app_info == NULL)
        return;
    app_info->set_runtime_status(status);
}

void AppInfoManager::set_virtual_launch_params(const std::string& app_id, const pbnjson::JValue& params)
{
    AppInfoPtr app_info = get_app_info_for_setter(app_id);
    if(app_info == NULL)
        return;
    app_info->set_virtual_launch_params(params);
}

//////////////////////////////////////////////////////////////////
/// getters
//////////////////////////////////////////////////////////////////
bool AppInfoManager::can_execute(const std::string& app_id)
{
    return get_app_info_for_getter(app_id)->can_execute();
}

bool AppInfoManager::is_remove_flagged(const std::string& app_id)
{
    return get_app_info_for_getter(app_id)->is_remove_flagged();
}

bool AppInfoManager::preload_mode_on(const std::string& app_id)
{
    return get_app_info_for_getter(app_id)->preload_mode_on();
}

bool AppInfoManager::is_out_of_service(const std::string& app_id)
{
    for(auto& it : m_out_of_service_info_list)
    {
        if(it == app_id) return true;
    }
    return false;
}

bool AppInfoManager::has_update(const std::string& app_id)
{
    for(auto& it: m_update_info_list)
    {
        if(it.first == app_id) return true;
    }
    return false;
}

const std::string AppInfoManager::update_category(const std::string& app_id)
{
    for(auto& it: m_update_info_list)
    {
        if(it.first == app_id)
        {
            pbnjson::JValue json = it.second;
            std::string category = json["category"].asString();
            return category;
        }
    }
    return NULL_STR;
}

const std::string AppInfoManager::update_type(const std::string& app_id)
{
    for(auto& it: m_update_info_list)
    {
        if(it.first == app_id)
        {
            pbnjson::JValue json = it.second;
            std::string type = json["type"].asString();
            return type;
        }
    }
    return NULL_STR;
}

const std::string AppInfoManager::update_version(const std::string& app_id)
{
    for(auto& it: m_update_info_list)
    {
        if(it.first == app_id)
        {
            pbnjson::JValue json = it.second;
            std::string version = json["version"].asString();
            return version;
        }
    }
    return NULL_STR;
}

double AppInfoManager::last_launch_time(const std::string& app_id)
{
    return get_app_info_for_getter(app_id)->last_launch_time();
}

LifeStatus AppInfoManager::life_status(const std::string& app_id)
{
    return get_app_info_for_getter(app_id)->life_status();
}

RuntimeStatus AppInfoManager::runtime_status(const std::string& app_id)
{
    return get_app_info_for_getter(app_id)->runtime_status();
}

const pbnjson::JValue& AppInfoManager::virtual_launch_params(const std::string& app_id)
{
    return get_app_info_for_getter(app_id)->virtual_launch_params();
}


//////////////////////////////////////////////////////////////////
/// get_foreground_apps
//////////////////////////////////////////////////////////////////
bool AppInfoManager::is_app_on_fullscreen(const std::string& app_id)
{
    if(app_id.empty())
        return false;
    return (m_current_foreground_app_id == app_id);
}

bool AppInfoManager::is_app_on_foreground(const std::string& app_id)
{
    for(auto& foreground_app_id: m_foreground_apps) {
        if(foreground_app_id == app_id) return true;
    }
    return false;
}

void AppInfoManager::get_json_foreground_info_by_id(const std::string& app_id, pbnjson::JValue& info)
{
    if (!m_json_foreground_info.isArray() || m_json_foreground_info.arraySize() < 1)
        return;

    for(auto item: m_json_foreground_info.items()) {
        if (!item.hasKey("appId") || !item["appId"].isString()) continue;

        if (item["appId"].asString() == app_id) {
            info = item.duplicate();
            return;
        }
    }
}


////////////////////////////////////////////////////////////////////
/// handling app info list
////////////////////////////////////////////////////////////////////
// remove app_info item if the app is not valid anymore after full scanning done
void AppInfoManager::OnAllAppRosterChanged(const AppDescMaps& all_apps)
{
    std::vector<std::string> removed_apps;
    for (const auto& app_info: m_appinfo_list) {
        if (all_apps.count(app_info.first) == 0) removed_apps.push_back(app_info.first);
    }

    for (const auto& app_id: removed_apps) {
        remove_app_info(app_id);
    }
}

////////////////////////////////////////////////////////////////////
/// handling out of service info list
////////////////////////////////////////////////////////////////////
void AppInfoManager::reset_all_out_of_service_info()
{
    m_out_of_service_info_list.clear();
}

////////////////////////////////////////////////////////////////////
/// handling update info list
////////////////////////////////////////////////////////////////////

void AppInfoManager::add_update_info(const std::string& app_id, const std::string& type, const std::string& category, const std::string& version)
{
    pbnjson::JValue info = pbnjson::Object();
    info.put("type", type);
    info.put("category", category);
    info.put("version", version);

    m_update_info_list.insert(std::make_pair(app_id, info));
}

void AppInfoManager::reset_all_update_info()
{
    m_update_info_list.clear();
}

////////////////////////////////////////////////////////////////////
/// handling running list
////////////////////////////////////////////////////////////////////
void AppInfoManager::add_running_info(const std::string& app_id, const std::string& pid, const std::string& webprocid)
{
    for(auto& running_data: m_running_list)
    {
        if(running_data->app_id == app_id)
        {
            running_data->pid = pid;
            running_data->webprocid = webprocid;
            return;
        }
    }

    RunningInfoPtr new_running_item = std::make_shared<RunningInfo>(app_id, pid, webprocid);
    if(new_running_item == NULL)
    {
        LOG_ERROR(MSGID_RUNNING_LIST_ERR, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "make_shared_fail"), "");
        return;
    }

    LOG_INFO(MSGID_RUNNING_LIST, 4, PMLOGKS("app_id", app_id.c_str()),
                                    PMLOGKS("pid", pid.c_str()),
                                    PMLOGKS("webprocid", webprocid.c_str()),
                                    PMLOGKS("status", "added"), "");

    m_running_list.push_back(new_running_item);
}

void AppInfoManager::remove_running_info(const std::string& app_id)
{
    auto it = std::find_if(m_running_list.begin(), m_running_list.end(),
                          [&app_id](RunningInfoPtr running_data){ return (running_data->app_id == app_id); });
    if(it == m_running_list.end())
    {
        LOG_ERROR(MSGID_RUNNING_LIST_ERR, 2, PMLOGKS("status", "failed_to_remove"),
                                             PMLOGKS("app_id", app_id.c_str()), "not found app_id in running_list");
        return;
    }

    m_running_list.erase(it);

    LOG_INFO(MSGID_RUNNING_LIST, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("action", "removed"), "");
}

void AppInfoManager::get_running_app_ids(std::vector<std::string>& running_app_ids)
{
    for(auto& running_data: m_running_list)
        running_app_ids.push_back(running_data->app_id);
}

void AppInfoManager::get_running_list(pbnjson::JValue& running_list, bool devmode_only)
{
    if(!running_list.isArray())
        return;

    for(auto& running_data: m_running_list)
    {
        pbnjson::JValue running_info = pbnjson::Object();
        AppDescPtr app_desc = ApplicationManager::instance().getAppById(running_data->app_id);
        if(app_desc == NULL)
            continue;

        if(devmode_only && AppTypeByDir::Dev != app_desc->getTypeByDir())
            continue;

        std::string app_type = ApplicationDescription::appTypeToString(app_desc->type());
        running_info.put("id", running_data->app_id);
        running_info.put("processid", running_data->pid);
        running_info.put("webprocessid", running_data->webprocid);
        running_info.put("defaultWindowType", app_desc->defaultWindowType());
        running_info.put("appType", app_type);

        running_list.append(running_info);
    }
}

RunningInfoPtr AppInfoManager::get_running_data(const std::string& app_id)
{
    auto it = std::find_if(m_running_list.begin(), m_running_list.end(),
                          [&app_id](const RunningInfoPtr running_data){ return (running_data->app_id == app_id); });
    if(it != m_running_list.end())
        return (*it);
    return NULL;
}

bool AppInfoManager::is_running(const std::string& app_id)
{
    auto it = std::find_if(m_running_list.begin(), m_running_list.end(),
                          [&app_id](const RunningInfoPtr running_data){ return (running_data->app_id == app_id); });
    if(it == m_running_list.end())
        return false;
    return true;
}

const std::string& AppInfoManager::get_app_id_by_pid(const std::string& pid)
{
    auto it = std::find_if(m_running_list.begin(), m_running_list.end(),
                          [&pid](const RunningInfoPtr running_data){ return (running_data->pid == pid); });
    if(it == m_running_list.end())
        return NULL_STR;
    return (*it)->app_id;
}

const std::string& AppInfoManager::pid(const std::string& app_id)
{
    auto it = std::find_if(m_running_list.begin(), m_running_list.end(),
                          [&app_id](const RunningInfoPtr running_data){ return (running_data->app_id == app_id); });
    if(it == m_running_list.end())
        return NULL_STR;
    return (*it)->pid;
}

const std::string& AppInfoManager::webprocid(const std::string& app_id)
{
    auto it = std::find_if(m_running_list.begin(), m_running_list.end(),
                          [&app_id](const RunningInfoPtr running_data){ return (running_data->app_id == app_id); });
    if(it == m_running_list.end())
        return NULL_STR;
    return (*it)->webprocid;
}

////////////////////////////////////////////////////////////////////
/// handling list status
////////////////////////////////////////////////////////////////////
std::string AppInfoManager::life_status_to_string(const LifeStatus& status)
{
    std::string str_status;

    switch(status)
    {
        case LifeStatus::PRELOADING:
            str_status = "preloading";
            break;
        case LifeStatus::LAUNCHING:
            str_status = "launching";
            break;
        case LifeStatus::RELAUNCHING:
            str_status = "relaunching";
            break;
        case LifeStatus::CLOSING:
            str_status = "closing";
            break;
        case LifeStatus::STOP:
            str_status = "stop";
            break;
        case LifeStatus::FOREGROUND:
            str_status = "foreground";
            break;
        case LifeStatus::BACKGROUND:
            str_status = "background";
            break;
        case LifeStatus::PAUSING:
            str_status = "pausing";
            break;
        default:
            str_status = "unknown";
            break;
    }

    return str_status;
}

////////////////////////////////////////////////////////////////////
/// get life status
////////////////////////////////////////////////////////////////////
void AppInfoManager::get_app_ids_by_life_status(const LifeStatus& status, std::vector<std::string>& app_ids)
{
    if(LifeStatus::LAUNCHING != status &&
       LifeStatus::RELAUNCHING != status &&
       LifeStatus::CLOSING != status &&
       LifeStatus::FOREGROUND != status &&
       LifeStatus::BACKGROUND != status)
    {
        return;
    }

    for(auto& it: m_appinfo_list)
    {
        if(it.second->life_status() == status)
            app_ids.push_back(it.first);
    }
}
