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

#ifndef CORE_BASE_PREREQUISITE_MONITOR_H_
#define CORE_BASE_PREREQUISITE_MONITOR_H_

#include <glib.h>
#include <functional>
#include <memory>
#include <vector>

enum class PrerequisiteItemStatus
    : int8_t {
        Ready = 1, Doing, Passed, Failed,
};

class PrerequisiteItem {
public:
    PrerequisiteItem();
    virtual ~PrerequisiteItem();

    virtual void Start() = 0;
    PrerequisiteItemStatus Status() const
    {
        return status_;
    }

protected:
    void SetStatus(PrerequisiteItemStatus status);

private:
    friend class PrerequisiteMonitor;

    PrerequisiteItemStatus status_;
    std::function<void(PrerequisiteItemStatus)> status_notifier_;
};
typedef std::shared_ptr<PrerequisiteItem> PrerequisiteItemPtr;

enum class PrerequisiteResult
    : int8_t {
        Passed = 1, Failed,
};

class PrerequisiteMonitor {
public:
    PrerequisiteMonitor(std::function<void(PrerequisiteResult)> result_handler) :
            result_handler_(result_handler)
    {
    }
    ~PrerequisiteMonitor();

    static PrerequisiteMonitor& Create(std::function<void(PrerequisiteResult)> result_handler);

    void AddItem(PrerequisiteItemPtr item);
    void Run();

private:
    void HandleItemStatus(PrerequisiteItemStatus status);

    static gboolean AsyncDelete(gpointer data);

    std::vector<PrerequisiteItemPtr> prerequisites_;
    std::function<void(PrerequisiteResult)> result_handler_;
};

#endif  //  CORE_BASE_PREREQUISITE_MONITOR_H_
