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

#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <lifecycle/stage/appitem/CloseAppItem.h>
#include <package/PackageManager.h>
#include <util/Logging.h>


CloseAppItem::CloseAppItem(const std::string& app_id, const std::string& pid, const std::string& caller, const std::string& reason)
    : AppItem(app_id, "", pid)
{
    setReason(reason);
    setCallerId(caller);

    LOG_INFO(MSGID_APPCLOSE, 3,
             PMLOGKS("appId", getAppId().c_str()),
             PMLOGKS("uid", getUid().c_str()),
             PMLOGKS("action", "created_close_item"), "");
}

CloseAppItem::~CloseAppItem()
{
    LOG_INFO(MSGID_APPCLOSE, 3,
             PMLOGKS("appId", getAppId().c_str()),
             PMLOGKS("uid", getUid().c_str()),
             PMLOGKS("action", "removed_close_item"), "");
}
