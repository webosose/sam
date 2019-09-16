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

#ifndef BASE_PREREQUISITEITEM_H_
#define BASE_PREREQUISITEITEM_H_

#include <glib.h>
#include <functional>
#include <memory>
#include <vector>

enum class PrerequisiteItemStatus : int8_t {
    READY = 1,
    DOING,
    PASSED,
    FAILED,
};

class PrerequisiteItem;

class PrerequisiteItem {
friend class PrerequisiteMonitor;
public:
    PrerequisiteItem()
        : m_status(PrerequisiteItemStatus::READY)
    {
    }
    virtual ~PrerequisiteItem()
    {
    }

    virtual void start() = 0;

    PrerequisiteItemStatus getStatus() const
    {
        return m_status;
    }

    boost::signals2::signal<void(PrerequisiteItem* item)> EventItemStatusChanged;

protected:
    void setStatus(PrerequisiteItemStatus status)
    {
        if (status == m_status)
            return;

        m_status = status;
        EventItemStatusChanged(this);
    }

    PrerequisiteItemStatus m_status;
};

#endif /* BASE_PREREQUISITEITEM_H_ */
