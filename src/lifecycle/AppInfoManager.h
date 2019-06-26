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

#ifndef APP_INFO_MANAGER_H_
#define APP_INFO_MANAGER_H_

#include <lifecycle/AppInfo.h>
#include <package/PackageManager.h>
#include <pbnjson.hpp>

#include <util/Singleton.h>

struct RunningInfo {
    std::string m_appId;
    std::string m_pid;
    std::string m_webprocid;

    RunningInfo(const std::string& appId, const std::string& pid, const std::string& webprocid)
        : m_appId(appId),
          m_pid(pid),
          m_webprocid(webprocid)
    {
    }
};

typedef std::shared_ptr<RunningInfo> RunningInfoPtr;
typedef std::list<RunningInfoPtr> RunningInfoList;
typedef std::map<std::string, pbnjson::JValue> UpdateInfoList;

class AppInfoManager: public Singleton<AppInfoManager> {
friend class Singleton<AppInfoManager> ;
public:
    AppInfoManager();
    virtual ~AppInfoManager();

    void initialize();

    // getter list
    bool canExecute(const std::string& app_id);
    bool isRemoveFlagged(const std::string& app_id);
    const std::string& getPid(const std::string& app_id);
    const std::string& webprocid(const std::string& app_id);
    bool preloadModeOn(const std::string& app_id);
    bool hasUpdate(const std::string& app_id);
    const std::string updateCategory(const std::string& app_id);
    const std::string updateType(const std::string& app_id);
    const std::string updateVersion(const std::string& app_id);
    double lastLaunchTime(const std::string& app_id);
    LifeStatus lifeStatus(const std::string& app_id);
    RuntimeStatus runtimeStatus(const std::string& app_id);

    // setter list
    void setExecutionLock(const std::string& app_id, bool v = true);
    void setRemovalFlag(const std::string& app_id, bool v = true);
    void setPreloadMode(const std::string& app_id, bool mode);
    void setLastLaunchTime(const std::string& app_id, double launch_time);
    void setLifeStatus(const std::string& app_id, const LifeStatus& status);
    void setRuntimeStatus(const std::string& app_id, RuntimeStatus status);

    void removeAppInfo(const std::string& app_id);

    // life status
    std::string toString(const LifeStatus& status);
    void getAppIdsByLifeStatus(const LifeStatus& status, std::vector<std::string>& app_list);

    // running info
    const std::string& getAppIdByPid(const std::string& pid);
    void getRunningList(pbnjson::JValue& running_list, bool devmode_only = false);
    void getRunningAppIds(std::vector<std::string>& running_app_ids);
    void addRunningInfo(const std::string& app_id, const std::string& pid, const std::string& webprocid);
    void removeRunningInfo(const std::string& app_id);
    bool isRunning(const std::string& app_id);

    const std::string& getCurrentForegroundAppId() const
    {
        return m_currentForegroundAppId;
    }
    const pbnjson::JValue& getJsonForegroundInfo() const
    {
        return m_jsonForegroundInfo;
    }
    void getJsonForegroundInfoById(const std::string& app_id, pbnjson::JValue& info);
    const std::vector<std::string>& getForegroundApps() const
    {
        return m_foregroundApps;
    }
    bool isAppOnFullscreen(const std::string& app_id);
    bool isAppOnForeground(const std::string& app_id);

    void setCurrentForegroundAppId(const std::string& app_id)
    {
        m_currentForegroundAppId = app_id;
    }
    void setForegroundInfo(const pbnjson::JValue& new_info)
    {
        m_jsonForegroundInfo = new_info.duplicate();
    }
    void setForegroundApps(const std::vector<std::string>& new_apps)
    {
        m_foregroundApps = new_apps;
    }

private:
    void onAllAppRosterChanged(const AppDescMaps& allApps);
    AppInfoPtr getAppInfo(const std::string& appId);
    AppInfoPtr getAppInfoForSetter(const std::string& appId);
    AppInfoPtr getAppInfoForGetter(const std::string& appId);

    AppInfoPtr m_defaultAppInfo;
    AppInfoList m_appinfoList;
    RunningInfoList m_runningList;
    UpdateInfoList m_updateInfoList;

    std::string m_currentForegroundAppId;
    pbnjson::JValue m_jsonForegroundInfo;

    std::vector<std::string> m_foregroundApps;
};

#endif
