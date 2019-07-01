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

#include <lifecycle/stage/MemoryChecker.h>

MemoryChecker::MemoryChecker()
{

}

MemoryChecker::~MemoryChecker()
{
}

void MemoryChecker::add_item(LaunchAppItemPtr item)
{
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "null_pointer"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }

    auto it = std::find_if(m_queue.begin(), m_queue.end(), [&item](LaunchAppItemPtr it) {
        return (it->getUid() == item->getUid());
    });
    if (it != m_queue.end()) {
        LOG_WARNING(MSGID_APPLAUNCH_ERR, 4,
                    PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                    PMLOGKS("uid", item->getUid().c_str()),
                    PMLOGKS(LOG_KEY_REASON, "already_in_queue"),
                    PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }

    LaunchAppItemPtr new_item = std::static_pointer_cast<LaunchAppItem>(item);
    if (new_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                  PMLOGKS(LOG_KEY_REASON, "pointer_cast_error"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }

    m_queue.push_back(new_item);
}

void MemoryChecker::remove_item(const std::string& item_uid)
{
    auto it = std::find_if(m_queue.begin(), m_queue.end(), [&item_uid](LaunchAppItemPtr it) {
        return (it->getUid() == item_uid);
    });
    if (it != m_queue.end())
        m_queue.erase(it);
}

void MemoryChecker::run()
{
    while (!m_queue.empty()) {
        auto it = m_queue.begin();

        (*it)->setSubStage(static_cast<int>(AppLaunchingStage::MEMORY_CHECK_DONE));

        std::string uid = (*it)->getUid();
        EventMemoryCheckingDone((*it)->getUid());
        remove_item(uid);
    }
}

void MemoryChecker::cancel_all()
{
    for (auto& launching_item : m_queue) {
        LOG_INFO(MSGID_APPLAUNCH, 2,
                 PMLOGKS(LOG_KEY_APPID, launching_item->getAppId().c_str()),
                 PMLOGKS("status", "cancel_launching"), "");
        launching_item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "cancel all request");
        EventMemoryCheckingDone(launching_item->getUid());
    }

    m_queue.clear();
}
