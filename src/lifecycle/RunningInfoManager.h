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

#include <iostream>
#include <lifecycle/RunningInfo.h>
#include <package/AppPackageManager.h>
#include <pbnjson.hpp>
#include <util/Singleton.h>

using namespace std;

class RunningInfoManager: public Singleton<RunningInfoManager> {
friend class Singleton<RunningInfoManager> ;
public:
    static std::string toString(const LifeStatus& status);

    RunningInfoManager();
    virtual ~RunningInfoManager();

    void initialize();

    // setter list
    RunningInfoPtr getRunningInfo(const std::string& appId, const std::string& display = "default");
    RunningInfoPtr getRunningInfoByPid(const std::string& pid);
    RunningInfoPtr addRunningInfo(const std::string& appId, const std::string& display = "default");
    bool removeRunningInfo(const std::string& appId, const std::string& display = "default");

    bool isRunning(const std::string& appId, const std::string& display = "default");
    void getRunningAppIds(std::vector<std::string>& appIds);
    void getRunningList(pbnjson::JValue& runningList, bool devmodeOnly = false);

    bool isAppOnFullscreen(const std::string& appId);

    void setForegroundApp(const std::string& appId)
    {
        m_foregroundAppId = appId;
    }
    const std::string& getForegroundAppId() const
    {
        return m_foregroundAppId;
    }

    void setForegroundAppIds(const std::vector<std::string>& apps)
    {
        m_foregroundAppIds = apps;
    }
    const std::vector<std::string>& getForegroundAppIds() const
    {
        return m_foregroundAppIds;
    }

    void setForegroundInfo(const pbnjson::JValue& info)
    {
        m_foregroundInfo = info.duplicate();
    }
    const pbnjson::JValue& getForegroundInfo() const
    {
        return m_foregroundInfo;
    }
    void getForegroundInfoById(const std::string& appId, pbnjson::JValue& info);

private:
    static string getKey(const std::string& appId, const std::string& display = "default")
    {
        string key = appId + "_" + display;
        return key;
    }

    RunningInfoList m_runningList;

    std::string m_foregroundAppId;
    std::vector<std::string> m_foregroundAppIds;
    pbnjson::JValue m_foregroundInfo;
};

#endif
