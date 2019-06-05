// Copyright (c) 2017-2018 LG Electronics, Inc.
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

#ifndef LIFECYCLE_LIFECYCLE_TASK_H_
#define LIFECYCLE_LIFECYCLE_TASK_H_

#include <bus/appmgr_service.h>
#include <package/AppDescription.h>

enum class LifeCycleTaskType : int8_t {
    Launch = 1,
    Pause,
    Close,
    CloseAll,
};

enum class LifeCycleTaskStatus : int8_t {
    Ready = 1,
    Pending,
    Waiting,
    Done,
};

class LifeCycleTask {
public:
    LifeCycleTask(LifeCycleTaskType type, LunaTaskPtr luna_task, AppDescPtr app_desc = nullptr)
        : m_lifecycleTaskType(type),
          m_status(LifeCycleTaskStatus::Ready),
          m_appDesc(app_desc),
          m_lunaTask(luna_task)
    {
    }
    virtual ~LifeCycleTask()
    {
    }

    LifeCycleTaskType type() const
    {
        return m_lifecycleTaskType;
    }
    LifeCycleTaskStatus status() const
    {
        return m_status;
    }
    void set_status(LifeCycleTaskStatus status)
    {
        m_status = status;
    }
    void SetError(int32_t code, const std::string& text)
    {
        m_lunaTask->setError(code, text);
    }
    const std::string& app_id() const
    {
        return m_appId;
    }
    void SetAppId(const std::string& app_id)
    {
        m_appId = app_id;
    }
    LunaTaskPtr LunaTask() const
    {
        return m_lunaTask;
    }
    void Finalize()
    {
        m_lunaTask->replyResult();
    }
    void Finalize(const pbnjson::JValue& payload)
    {
        m_lunaTask->ReplyResult(payload);
    }
    void FinalizeWithError(int32_t code, const std::string& text)
    {
        SetError(code, text);
        Finalize();
    }

private:
    LifeCycleTaskType m_lifecycleTaskType;
    LifeCycleTaskStatus m_status;
    AppDescPtr m_appDesc;
    std::string m_appId;
    LunaTaskPtr m_lunaTask;
};

typedef std::shared_ptr<LifeCycleTask> LifeCycleTaskPtr;

#endif  // CORE_LIFECYCLE_LIFECYCLE_TASK_CONTROLLER_H_

