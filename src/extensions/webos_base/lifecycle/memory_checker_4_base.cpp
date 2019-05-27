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

#include "extensions/webos_base/lifecycle/memory_checker_4_base.h"

#include "extensions/webos_base/base_logs.h"

MemoryChecker4Base::MemoryChecker4Base()
{

}

MemoryChecker4Base::~MemoryChecker4Base()
{
}

void MemoryChecker4Base::add_item(AppLaunchingItemPtr item)
{
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("reason", "null_pointer"), PMLOGKS("where", "memory_checking_add_item"), "");
        return;
    }

    auto it = std::find_if(app_launching_item_list_.begin(), app_launching_item_list_.end(), [&item](AppLaunchingItemPtr it) {return (it->uid() == item->uid());});
    if (it != app_launching_item_list_.end()) {
        LOG_WARNING(MSGID_APPLAUNCH_ERR, 4, PMLOGKS("app_id", item->appId().c_str()), PMLOGKS("uid", item->uid().c_str()), PMLOGKS("reason", "already_in_queue"),
                PMLOGKS("where", "memory_checking_add_item"), "");
        return;
    }

    AppLaunchingItem4BasePtr new_item = std::static_pointer_cast<AppLaunchingItem4Base>(item);
    if (new_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("app_id", item->appId().c_str()), PMLOGKS("reason", "pointer_cast_error"), PMLOGKS("where", "memory_checking_add_item"), "");
        return;
    }

    app_launching_item_list_.push_back(new_item);
}

void MemoryChecker4Base::remove_item(const std::string& item_uid)
{
    auto it = std::find_if(app_launching_item_list_.begin(), app_launching_item_list_.end(), [&item_uid](AppLaunchingItemPtr it) {return (it->uid() == item_uid);});
    if (it != app_launching_item_list_.end())
        app_launching_item_list_.erase(it);
}

void MemoryChecker4Base::run()
{
    while (!app_launching_item_list_.empty()) {
        auto it = app_launching_item_list_.begin();

        (*it)->setSubStage(static_cast<int>(AppLaunchingStage4Base::MEMORY_CHECK_DONE));

        std::string uid = (*it)->uid();
        signal_memory_checking_done((*it)->uid());
        remove_item(uid);
    }
}

void MemoryChecker4Base::cancel_all()
{
    for (auto& launching_item : app_launching_item_list_) {
        LOG_INFO(MSGID_APPLAUNCH, 2, PMLOGKS("app_id", launching_item->appId().c_str()), PMLOGKS("status", "cancel_launching"), "");
        launching_item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "cancel all request");
        signal_memory_checking_done(launching_item->uid());
    }

    app_launching_item_list_.clear();
}
