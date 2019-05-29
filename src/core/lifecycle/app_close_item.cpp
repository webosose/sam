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

#include "core/lifecycle/app_close_item.h"

#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <core/package/package_manager.h>
#include <core/util/logging.h>


AppCloseItem::AppCloseItem(const std::string& app_id, const std::string& pid, const std::string& caller, const std::string& reason) :
        m_app_id(app_id), m_pid(pid), m_caller(caller), m_is_memory_reclaim(false), m_reason(reason), m_close_start_time(0)
{

    boost::uuids::uuid uid = boost::uuids::random_generator()();
    m_uid = boost::lexical_cast<std::string>(uid);

    if (m_reason == "memoryReclaim") {
        m_is_memory_reclaim = true;
    }

    LOG_INFO(MSGID_APPCLOSE, 3, PMLOGKS("app_id", m_app_id.c_str()), PMLOGKS("uid", m_uid.c_str()), PMLOGKS("action", "created_close_item"), "");
}

AppCloseItem::~AppCloseItem()
{
    LOG_INFO(MSGID_APPCLOSE, 3, PMLOGKS("app_id", m_app_id.c_str()), PMLOGKS("uid", m_uid.c_str()), PMLOGKS("action", "removed_close_item"), "");
}
