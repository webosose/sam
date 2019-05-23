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

#include "core/base/logging.h"
#include "core/package/application_manager.h"

AppCloseItem::AppCloseItem(const std::string& app_id, const std::string& pid, const std::string& caller, const std::string& reason) :
        app_id_(app_id), pid_(pid), caller_(caller), is_memory_reclaim_(false), reason_(reason), close_start_time_(0)
{

    boost::uuids::uuid uid = boost::uuids::random_generator()();
    uid_ = boost::lexical_cast<std::string>(uid);

    if (reason_ == "memoryReclaim") {
        is_memory_reclaim_ = true;
    }

    LOG_INFO(MSGID_APPCLOSE, 3, PMLOGKS("app_id", app_id_.c_str()), PMLOGKS("uid", uid_.c_str()), PMLOGKS("action", "created_close_item"), "");
}

AppCloseItem::~AppCloseItem()
{
    LOG_INFO(MSGID_APPCLOSE, 3, PMLOGKS("app_id", app_id_.c_str()), PMLOGKS("uid", uid_.c_str()), PMLOGKS("action", "removed_close_item"), "");
}
