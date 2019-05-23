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

#include <list>
#include <pbnjson.hpp>

#include "core/lifecycle/application_errors.h"
#include "core/package/application_description.h"

class AppCloseItem {
public:
    AppCloseItem(const std::string& app_id, const std::string& pid, const std::string& caller, const std::string& reason);
    virtual ~AppCloseItem();

    const std::string& Uid() const
    {
        return uid_;
    }
    const std::string& AppId() const
    {
        return app_id_;
    }
    const std::string& Pid() const
    {
        return pid_;
    }
    const std::string& Caller() const
    {
        return caller_;
    }
    bool IsMemoryReclaim() const
    {
        return is_memory_reclaim_;
    }
    const std::string& Reason() const
    {
        return reason_;
    }
    const double& CloseStartTime() const
    {
        return close_start_time_;
    }

private:
    std::string uid_;
    std::string app_id_;
    std::string pid_;
    std::string caller_;
    bool is_memory_reclaim_;
    std::string reason_;
    double close_start_time_;
};
typedef std::shared_ptr<AppCloseItem> AppCloseItemPtr;
typedef std::list<AppCloseItemPtr> AppCloseItemList;

#endif
