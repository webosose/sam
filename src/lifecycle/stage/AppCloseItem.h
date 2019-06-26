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

#ifndef APP_CLOSE_ITEM_H_
#define APP_CLOSE_ITEM_H_

#include <lifecycle/ApplicationErrors.h>
#include <package/AppDescription.h>
#include <list>
#include <pbnjson.hpp>

class AppCloseItem {
public:
    AppCloseItem(const std::string& app_id, const std::string& pid, const std::string& caller, const std::string& reason);
    virtual ~AppCloseItem();

    const std::string& getUid() const
    {
        return m_uid;
    }
    const std::string& getAppId() const
    {
        return m_app_id;
    }
    const std::string& getPid() const
    {
        return m_pid;
    }
    const std::string& getCaller() const
    {
        return m_caller;
    }
    bool IsMemoryReclaim() const
    {
        return m_is_memory_reclaim;
    }
    const std::string& getReason() const
    {
        return m_reason;
    }
    const double& getCloseStartTime() const
    {
        return m_close_start_time;
    }

private:
    std::string m_uid;
    std::string m_app_id;
    std::string m_pid;
    std::string m_caller;
    bool m_is_memory_reclaim;
    std::string m_reason;
    double m_close_start_time;
};
typedef std::shared_ptr<AppCloseItem> AppCloseItemPtr;
typedef std::list<AppCloseItemPtr> AppCloseItemList;

#endif
