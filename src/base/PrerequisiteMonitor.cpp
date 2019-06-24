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

#include <base/PrerequisiteMonitor.h>
#include <util/Logging.h>


PrerequisiteItem::PrerequisiteItem() :
        m_status(PrerequisiteItemStatus::Ready)
{
    LOG_DEBUG("[PrerequisiteItem] Created");
}

PrerequisiteItem::~PrerequisiteItem()
{
    LOG_DEBUG("[PrerequisiteItem] Released");
}

void PrerequisiteItem::SetStatus(PrerequisiteItemStatus status)
{
    switch (status) {
    case PrerequisiteItemStatus::Ready:
    case PrerequisiteItemStatus::Doing:
        return;
        break;
    default:
        break;
    }
    m_status = status;
    if (m_status_notifier)
        m_status_notifier(m_status);
}

////////////////////////////////////
// Prerequisite Monitor
////////////////////////////////////
PrerequisiteMonitor::~PrerequisiteMonitor()
{
    LOG_DEBUG("[PrerequisiteMonitor] Released in destructor");
}

PrerequisiteMonitor& PrerequisiteMonitor::create(std::function<void(PrerequisiteResult)> result_handler)
{
    PrerequisiteMonitor* monitor = new PrerequisiteMonitor(result_handler);
    return *monitor;
}

void PrerequisiteMonitor::addItem(PrerequisiteItemPtr item)
{
    if (item == nullptr)
        return;

    LOG_DEBUG("[PrerequisiteMonitor] Item Added");
    item->m_status_notifier = std::bind(&PrerequisiteMonitor::handleItemStatus, this, std::placeholders::_1);
    m_prerequisites.push_back(item);
}

void PrerequisiteMonitor::run()
{
    if (m_prerequisites.empty()) {
        if (m_resultHandler != nullptr)
            m_resultHandler(PrerequisiteResult::Passed);
        return;
    }

    for (auto item : m_prerequisites) {
        LOG_DEBUG("[PrerequisiteMonitor] Item Starting");
        item->m_status = PrerequisiteItemStatus::Doing;
        item->start();
    }
}

void PrerequisiteMonitor::handleItemStatus(PrerequisiteItemStatus status)
{

    LOG_DEBUG("[PrerequisiteMonitor] Item Status Changed: %d", (int ) status);

    PrerequisiteResult result = PrerequisiteResult::Passed;

    for (auto item : m_prerequisites) {
        LOG_DEBUG("item checking: %d", (int ) item->status());
        switch (item->status()) {
        case PrerequisiteItemStatus::Ready:
        case PrerequisiteItemStatus::Doing:
            return;
            break;
        case PrerequisiteItemStatus::Failed:
            result = PrerequisiteResult::Failed;
            break;
        default:
            break;
        }
    }

    LOG_DEBUG("[PrerequisiteMonitor] All Item Ready. Now call result handler");
    m_resultHandler(result);

    LOG_DEBUG("[PrerequisiteMonitor] Done all duty. Now Releasing itself");
    m_prerequisites.clear();

    // TODO: PrerequisiteMonitor doesn't guarantee item's timing issue
    //       Currently it focuses on high level multi condition checking before doing something
    //       Please use this after understanding full flows
    //       Improving internal common libraries for more general uses is always welcome
    // g_timeout_add(0, AsyncDelete, (gpointer)this);

    delete this;
}

// For later use in case when it needs
gboolean PrerequisiteMonitor::asyncDelete(gpointer data)
{
    PrerequisiteMonitor *p = static_cast<PrerequisiteMonitor*>(data);
    if (!p)
        return FALSE;

    p->m_prerequisites.clear();

    delete p;
    return FALSE;
}
