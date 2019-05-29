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

#include <core/util/jutil.h>
#include <core/util/logging.h>
#include <core/util/lsutils.h>
#include "core/module/subscriber_of_bootd.h"

#include <pbnjson.hpp>

#include "core/bus/appmgr_service.h"
#include "core/module/service_observer.h"

BootdSubscriber::BootdSubscriber() :
        token_boot_status_(0)
{
}

BootdSubscriber::~BootdSubscriber()
{
}

void BootdSubscriber::Init()
{
    ServiceObserver::instance().Add(WEBOS_SERVICE_BOOTMGR, std::bind(&BootdSubscriber::OnServerStatusChanged, this, std::placeholders::_1));
}

boost::signals2::connection BootdSubscriber::SubscribeBootStatus(boost::function<void(const pbnjson::JValue&)> func)
{
    return notify_boot_status.connect(func);
}

void BootdSubscriber::OnServerStatusChanged(bool connection)
{
    if (connection) {
        RequestBootStatus();
    } else {
        if (0 != token_boot_status_) {
            (void) LSCallCancel(AppMgrService::instance().serviceHandle(), token_boot_status_, NULL);
            token_boot_status_ = 0;
        }
    }
}

void BootdSubscriber::RequestBootStatus()
{
    if (token_boot_status_ != 0)
        return;

    std::string method = std::string("luna://") + WEBOS_SERVICE_BOOTMGR + std::string("/getBootStatus");

    LSErrorSafe lserror;
    if (!LSCall(AppMgrService::instance().serviceHandle(), method.c_str(), "{\"subscribe\":true}", OnBootStatusCallback, this, &token_boot_status_, &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "lscall"), PMLOGJSON("payload", "{\"subscribe\":true}"), PMLOGKS("where", __FUNCTION__), "err: %s", lserror.message);
    }
}

bool BootdSubscriber::OnBootStatusCallback(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    BootdSubscriber* subscriber = static_cast<BootdSubscriber*>(user_data);
    if (!subscriber)
        return false;

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull())
        return false;

    if (jmsg.hasKey("bootStatus")) {
        // factory, normal, firstUse
        subscriber->boot_status_str_ = jmsg["bootStatus"].asString();
    }

    subscriber->notify_boot_status(jmsg);
    return true;
}
