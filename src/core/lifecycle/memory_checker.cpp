// Copyright (c) 2012-2018 LG Electronics, Inc.
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

#include <core/util/logging.h>
#include "core/lifecycle/memory_checker.h"


MemoryChecker::MemoryChecker()
{
}

MemoryChecker::~MemoryChecker()
{
}

void MemoryChecker::add_item(AppLaunchingItemPtr item)
{
    m_item_queue.push_back(item);
}

void MemoryChecker::remove_item(const std::string& item_uid)
{
    auto it = std::find_if(m_item_queue.begin(), m_item_queue.end(), [&item_uid](AppLaunchingItemPtr it) {return (it->uid() == item_uid);});
    if (it != m_item_queue.end())
        m_item_queue.erase(it);
}

void MemoryChecker::run()
{
    auto it = m_item_queue.begin();

    if (it == m_item_queue.end())
        return;

    std::string uid = (*it)->uid();

    signal_memory_checking_done((*it)->uid());

    remove_item(uid);
}

void MemoryChecker::cancel_all()
{
}
