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

#include <lifecycle/app_info_manager.h>
#include <util/logging.h>
#include <algorithm>


const std::string& NULL_STR = "";
const std::string& DEFAULT_NULL_APP = "@APP_INFO_DEFAULT_APP@";

AppInfoManager::AppInfoManager() :
        m_jsonForegroundInfo(pbnjson::Array())
{
    m_defaultAppInfo = std::make_shared<AppInfo>(DEFAULT_NULL_APP);
}

AppInfoManager::~AppInfoManager()
{
}

void AppInfoManager::init()
{
    PackageManager::instance().signalAllAppRosterChanged.connect(boost::bind(&AppInfoManager::onAllAppRosterChanged, this, _1));
}

AppInfoPtr AppInfoManager::getAppInfoForSetter(const std::string& appId)
{
    if (appId.empty()) {
        LOG_ERROR(MSGID_APPINFO_ERR, 1, PMLOGKS("reason", "empty_app_id"), "");
        return NULL;
    }

    AppInfoPtr appInfo = getAppInfo(appId);
    if (appInfo == NULL) {
        appInfo = std::make_shared<AppInfo>(appId);
        if (appInfo == NULL) {
            LOG_ERROR(MSGID_APPINFO_ERR, 1, PMLOGKS("reason", "failed_create_new_app_info"), "");
            return NULL;
        }
        m_appinfoList[appId] = appInfo;
        LOG_INFO(MSGID_APPINFO, 2,
                 PMLOGKS("app_id", appId.c_str()),
                 PMLOGKS("status", "item_added"), "");
    }
    return appInfo;
}

AppInfoPtr AppInfoManager::getAppInfoForGetter(const std::string& app_id)
{
    AppInfoPtr appInfo = getAppInfo(app_id);
    if (appInfo == NULL)
        return m_defaultAppInfo;
    return appInfo;
}

AppInfoPtr AppInfoManager::getAppInfo(const std::string& app_id)
{
    auto it = m_appinfoList.find(app_id);
    if (it == m_appinfoList.end())
        return NULL;
    return m_appinfoList[app_id];
}

void AppInfoManager::removeAppInfo(const std::string& app_id)
{
    LifeStatus current_status = lifeStatus(app_id);
    if (LifeStatus::INVALID == current_status || LifeStatus::STOP == current_status) {
        LOG_INFO(MSGID_APPINFO, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("status", "item_removed"), "");
        m_appinfoList.erase(app_id);
    } else {
        LOG_INFO(MSGID_APPINFO, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("status", "set_removed_flag_for_lazy_release"), "");
        setRemovalFlag(app_id, true);
    }
}

void AppInfoManager::setExecutionLock(const std::string& app_id, bool v)
{
    AppInfoPtr app_info = NULL;
    if (v == false) {
        app_info = getAppInfo(app_id);
        if (app_info == NULL)
            return;
    } else {
        app_info = getAppInfoForSetter(app_id);
        if (app_info == NULL)
            return;
    }
    app_info->set_execution_lock(v);
}

void AppInfoManager::setRemovalFlag(const std::string& app_id, bool v)
{
    AppInfoPtr app_info = getAppInfoForSetter(app_id);
    if (app_info == NULL)
        return;
    app_info->set_removal_flag(v);
}

void AppInfoManager::setPreloadMode(const std::string& app_id, bool mode)
{
    AppInfoPtr app_info = getAppInfoForSetter(app_id);
    if (app_info == NULL)
        return;
    app_info->set_preload_mode(mode);
}

void AppInfoManager::setLastLaunchTime(const std::string& app_id, double launch_time)
{
    AppInfoPtr app_info = getAppInfoForSetter(app_id);
    if (app_info == NULL)
        return;
    app_info->set_last_launch_time(launch_time);
}

void AppInfoManager::setLifeStatus(const std::string& app_id, const LifeStatus& status)
{
    AppInfoPtr app_info = getAppInfoForSetter(app_id);
    if (app_info == NULL)
        return;
    app_info->set_life_status(status);
    if (app_info->isRemoveFlagged())
        removeAppInfo(app_id);
}

void AppInfoManager::setRuntimeStatus(const std::string& app_id, RuntimeStatus status)
{
    AppInfoPtr app_info = getAppInfoForSetter(app_id);
    if (app_info == NULL)
        return;
    app_info->set_runtime_status(status);
}

void AppInfoManager::setVirtualLaunchParams(const std::string& app_id, const pbnjson::JValue& params)
{
    AppInfoPtr app_info = getAppInfoForSetter(app_id);
    if (app_info == NULL)
        return;
    app_info->set_virtual_launch_params(params);
}

bool AppInfoManager::canExecute(const std::string& app_id)
{
    return getAppInfoForGetter(app_id)->executionLock();
}

bool AppInfoManager::isRemoveFlagged(const std::string& app_id)
{
    return getAppInfoForGetter(app_id)->isRemoveFlagged();
}

bool AppInfoManager::preloadModeOn(const std::string& app_id)
{
    return getAppInfoForGetter(app_id)->preload_mode_on();
}

bool AppInfoManager::isOutOfService(const std::string& app_id)
{
    for (auto& it : m_outOfServiceInfoList) {
        if (it == app_id)
            return true;
    }
    return false;
}

bool AppInfoManager::hasUpdate(const std::string& app_id)
{
    for (auto& it : m_updateInfoList) {
        if (it.first == app_id)
            return true;
    }
    return false;
}

const std::string AppInfoManager::updateCategory(const std::string& app_id)
{
    for (auto& it : m_updateInfoList) {
        if (it.first == app_id) {
            pbnjson::JValue json = it.second;
            std::string category = json["category"].asString();
            return category;
        }
    }
    return NULL_STR;
}

const std::string AppInfoManager::updateType(const std::string& app_id)
{
    for (auto& it : m_updateInfoList) {
        if (it.first == app_id) {
            pbnjson::JValue json = it.second;
            std::string type = json["type"].asString();
            return type;
        }
    }
    return NULL_STR;
}

const std::string AppInfoManager::updateVersion(const std::string& app_id)
{
    for (auto& it : m_updateInfoList) {
        if (it.first == app_id) {
            pbnjson::JValue json = it.second;
            std::string version = json["version"].asString();
            return version;
        }
    }
    return NULL_STR;
}

double AppInfoManager::lastLaunchTime(const std::string& app_id)
{
    return getAppInfoForGetter(app_id)->last_launch_time();
}

LifeStatus AppInfoManager::lifeStatus(const std::string& app_id)
{
    return getAppInfoForGetter(app_id)->life_status();
}

RuntimeStatus AppInfoManager::runtimeStatus(const std::string& appId)
{
    return getAppInfoForGetter(appId)->runtime_status();
}

const pbnjson::JValue& AppInfoManager::virtualLaunchParams(const std::string& app_id)
{
    return getAppInfoForGetter(app_id)->virtual_launch_params();
}

bool AppInfoManager::isAppOnFullscreen(const std::string& app_id)
{
    if (app_id.empty())
        return false;
    return (m_currentForegroundAppId == app_id);
}

bool AppInfoManager::isAppOnForeground(const std::string& app_id)
{
    for (auto& foreground_app_id : m_foregroundApps) {
        if (foreground_app_id == app_id)
            return true;
    }
    return false;
}

void AppInfoManager::getJsonForegroundInfoById(const std::string& app_id, pbnjson::JValue& info)
{
    if (!m_jsonForegroundInfo.isArray() || m_jsonForegroundInfo.arraySize() < 1)
        return;

    for (auto item : m_jsonForegroundInfo.items()) {
        if (!item.hasKey("appId") || !item["appId"].isString())
            continue;

        if (item["appId"].asString() == app_id) {
            info = item.duplicate();
            return;
        }
    }
}

void AppInfoManager::onAllAppRosterChanged(const AppDescMaps& all_apps)
{
    std::vector<std::string> removed_apps;
    for (const auto& app_info : m_appinfoList) {
        if (all_apps.count(app_info.first) == 0)
            removed_apps.push_back(app_info.first);
    }

    for (const auto& app_id : removed_apps) {
        removeAppInfo(app_id);
    }
}

void AppInfoManager::resetAllOutOfServiceInfo()
{
    m_outOfServiceInfoList.clear();
}

void AppInfoManager::addUpdateInfo(const std::string& app_id, const std::string& type, const std::string& category, const std::string& version)
{
    pbnjson::JValue info = pbnjson::Object();
    info.put("type", type);
    info.put("category", category);
    info.put("version", version);

    m_updateInfoList.insert(std::make_pair(app_id, info));
}

void AppInfoManager::resetAllUpdateInfo()
{
    m_updateInfoList.clear();
}

void AppInfoManager::addRunningInfo(const std::string& app_id, const std::string& pid, const std::string& webprocid)
{
    for (auto& running_data : m_runningList) {
        if (running_data->m_appId == app_id) {
            running_data->m_pid = pid;
            running_data->m_webprocid = webprocid;
            return;
        }
    }

    RunningInfoPtr new_running_item = std::make_shared<RunningInfo>(app_id, pid, webprocid);
    if (new_running_item == NULL) {
        LOG_ERROR(MSGID_RUNNING_LIST_ERR, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "make_shared_fail"), "");
        return;
    }

    LOG_INFO(MSGID_RUNNING_LIST, 4, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("pid", pid.c_str()), PMLOGKS("webprocid", webprocid.c_str()), PMLOGKS("status", "added"), "");

    m_runningList.push_back(new_running_item);
}

void AppInfoManager::removeRunningInfo(const std::string& app_id)
{
    auto it = std::find_if(m_runningList.begin(), m_runningList.end(), [&app_id](RunningInfoPtr running_data) {return (running_data->m_appId == app_id);});
    if (it == m_runningList.end()) {
        LOG_ERROR(MSGID_RUNNING_LIST_ERR, 2, PMLOGKS("status", "failed_to_remove"), PMLOGKS("app_id", app_id.c_str()), "not found app_id in running_list");
        return;
    }

    m_runningList.erase(it);

    LOG_INFO(MSGID_RUNNING_LIST, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("action", "removed"), "");
}

void AppInfoManager::getRunningAppIds(std::vector<std::string>& running_app_ids)
{
    for (auto& running_data : m_runningList)
        running_app_ids.push_back(running_data->m_appId);
}

void AppInfoManager::getRunningList(pbnjson::JValue& running_list, bool devmode_only)
{
    if (!running_list.isArray())
        return;

    for (auto& running_data : m_runningList) {
        pbnjson::JValue running_info = pbnjson::Object();
        AppDescPtr app_desc = PackageManager::instance().getAppById(running_data->m_appId);
        if (app_desc == NULL)
            continue;

        if (devmode_only && AppTypeByDir::AppTypeByDir_Dev != app_desc->getTypeByDir())
            continue;

        std::string app_type = AppDescription::toString(app_desc->type());
        running_info.put("id", running_data->m_appId);
        running_info.put("processid", running_data->m_pid);
        running_info.put("webprocessid", running_data->m_webprocid);
        running_info.put("defaultWindowType", app_desc->defaultWindowType());
        running_info.put("appType", app_type);

        running_list.append(running_info);
    }
}

RunningInfoPtr AppInfoManager::getRunningData(const std::string& app_id)
{
    auto it = std::find_if(m_runningList.begin(), m_runningList.end(), [&app_id](const RunningInfoPtr running_data) {return (running_data->m_appId == app_id);});
    if (it != m_runningList.end())
        return (*it);
    return NULL;
}

bool AppInfoManager::isRunning(const std::string& app_id)
{
    auto it = std::find_if(m_runningList.begin(), m_runningList.end(), [&app_id](const RunningInfoPtr running_data) {return (running_data->m_appId == app_id);});
    if (it == m_runningList.end())
        return false;
    return true;
}

const std::string& AppInfoManager::getAppIdByPid(const std::string& pid)
{
    auto it = std::find_if(m_runningList.begin(), m_runningList.end(), [&pid](const RunningInfoPtr running_data) {return (running_data->m_pid == pid);});
    if (it == m_runningList.end())
        return NULL_STR;
    return (*it)->m_appId;
}

const std::string& AppInfoManager::pid(const std::string& app_id)
{
    auto it = std::find_if(m_runningList.begin(), m_runningList.end(), [&app_id](const RunningInfoPtr running_data) {return (running_data->m_appId == app_id);});
    if (it == m_runningList.end())
        return NULL_STR;
    return (*it)->m_pid;
}

const std::string& AppInfoManager::webprocid(const std::string& app_id)
{
    auto it = std::find_if(m_runningList.begin(), m_runningList.end(), [&app_id](const RunningInfoPtr running_data) {return (running_data->m_appId == app_id);});
    if (it == m_runningList.end())
        return NULL_STR;
    return (*it)->m_webprocid;
}

std::string AppInfoManager::toString(const LifeStatus& status)
{
    std::string str_status;

    switch (status) {
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

void AppInfoManager::getAppIdsByLifeStatus(const LifeStatus& status, std::vector<std::string>& app_ids)
{
    if (LifeStatus::LAUNCHING != status &&
        LifeStatus::RELAUNCHING != status &&
        LifeStatus::CLOSING != status &&
        LifeStatus::FOREGROUND != status &&
        LifeStatus::BACKGROUND != status) {
        return;
    }

    for (auto& it : m_appinfoList) {
        if (it.second->life_status() == status)
            app_ids.push_back(it.first);
    }
}
