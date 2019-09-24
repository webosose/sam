// Copyright (c) 2019 LG Electronics, Inc.
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

#ifndef BASE_RUNNINGAPPLIST_H_
#define BASE_RUNNINGAPPLIST_H_

#include <iostream>
#include <memory>
#include <list>

#include "interface/ISingleton.h"
#include "interface/IClassName.h"
#include "RunningApp.h"

using namespace std;

class RunningAppList : public ISingleton<RunningAppList>,
                       public IClassName {
friend class ISingleton<RunningAppList>;
public:
    virtual ~RunningAppList();

    RunningAppPtr create(LaunchPointPtr launchPoint);
    RunningAppPtr add(RunningAppPtr runningApp);

    RunningAppPtr getByAppId(const string& appId, bool createIfNotExist = false);
    RunningAppPtr getByLaunchPointId(const string& launchPointId);
    RunningAppPtr getByPid(const string& pid);
    RunningAppPtr getByInstanceId(const string& instanceId);
    RunningAppPtr getForeground();

    void remove(RunningAppPtr runningApp);
    void removeByAppId(const string& appId);

    bool isRunning(const string& appId);
    bool isForeground(const string& appId);

    void toJson(JValue& array, bool devmodeOnly = false);


    // TODO following methods should be deleted


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
    void getForegroundInfoById(const std::string& appId, pbnjson::JValue& info)
    {
        if (!m_foregroundInfo.isArray() || m_foregroundInfo.arraySize() < 1)
            return;

        for (auto item : m_foregroundInfo.items()) {
            if (!item.hasKey("appId") || !item["appId"].isString())
                continue;

            if (item["appId"].asString() == appId) {
                info = item.duplicate();
                return;
            }
        }
    }

private:
    RunningAppList();

    list<RunningAppPtr> m_list;


    // TODO: following should be deleted
    std::string m_foregroundAppId;
    std::vector<std::string> m_foregroundAppIds;
    pbnjson::JValue m_foregroundInfo;
};

#endif /* BASE_RUNNINGAPPLIST_H_ */
