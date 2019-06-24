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

#ifndef BASE_PREREQUISITEMONITOR_H_
#define BASE_PREREQUISITEMONITOR_H_

#include <glib.h>
#include <functional>
#include <memory>
#include <vector>

enum class PrerequisiteItemStatus : int8_t {
    Ready = 1,
    Doing,
    Passed,
    Failed,
};

class PrerequisiteItem {
public:
    PrerequisiteItem();
    virtual ~PrerequisiteItem();

    virtual void start() = 0;
    PrerequisiteItemStatus status() const
    {
        return m_status;
    }

protected:
    void SetStatus(PrerequisiteItemStatus status);

private:
    friend class PrerequisiteMonitor;

    PrerequisiteItemStatus m_status;
    std::function<void(PrerequisiteItemStatus)> m_status_notifier;
};
typedef std::shared_ptr<PrerequisiteItem> PrerequisiteItemPtr;

enum class PrerequisiteResult : int8_t {
    Passed = 1,
    Failed,
};

class PrerequisiteMonitor {
public:
    PrerequisiteMonitor(std::function<void(PrerequisiteResult)> result_handler)
        : m_resultHandler(result_handler)
    {
    }
    virtual ~PrerequisiteMonitor();

    static PrerequisiteMonitor& create(std::function<void(PrerequisiteResult)> result_handler);

    void addItem(PrerequisiteItemPtr item);
    void run();

private:
    void handleItemStatus(PrerequisiteItemStatus status);

    static gboolean asyncDelete(gpointer data);

    std::vector<PrerequisiteItemPtr> m_prerequisites;
    std::function<void(PrerequisiteResult)> m_resultHandler;
};

#endif  //  BASE_PREREQUISITEMONITOR_H_
