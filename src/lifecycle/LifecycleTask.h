// Copyright (c) 2017-2019 LG Electronics, Inc.
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

#ifndef LIFECYCLE_LIFECYCLETASK_H_
#define LIFECYCLE_LIFECYCLETASK_H_

#include <bus/service/ApplicationManager.h>
#include <package/AppPackage.h>

class LifecycleTask {
public:
    LifecycleTask(LunaTaskPtr lunaTaskPtr)
        : m_lunaTaskPtr(lunaTaskPtr)
    {
    }

    virtual ~LifecycleTask()
    {
    }

    const std::string& getAppId() const
    {
        return m_appId;
    }
    void setAppId(const std::string& appId)
    {
        m_appId = appId;
    }

    const std::string& getDisplay() const
    {
        return m_display;
    }
    void setDisplay(const std::string& display)
    {
        m_display = display;
    }

    LunaTaskPtr getLunaTask() const
    {
        return m_lunaTaskPtr;
    }
    void finalize()
    {
        m_lunaTaskPtr->reply();
    }
    void finalize(const pbnjson::JValue& payload)
    {
        m_lunaTaskPtr->replyResult(payload);
    }
    void finalizeWithError(int32_t code, const std::string& text)
    {
        m_lunaTaskPtr->setError(code, text);
        finalize();
    }

private:
    LunaTaskPtr m_lunaTaskPtr;
    std::string m_appId;
    std::string m_display;

};

typedef std::shared_ptr<LifecycleTask> LifecycleTaskPtr;

#endif  // CORE_LIFECYCLE_LIFECYCLE_TASK_CONTROLLER_H_

