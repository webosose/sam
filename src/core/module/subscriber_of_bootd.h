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

#ifndef CORE_MODULE_SUBSCRIBER_OF_BOOTD_H_
#define CORE_MODULE_SUBSCRIBER_OF_BOOTD_H_

#include <luna-service2/lunaservice.h>
#include <boost/signals2.hpp>
#include <core/util/singleton.h>
#include <pbnjson.hpp>


class BootdSubscriber: public Singleton<BootdSubscriber> {
public:
    BootdSubscriber();
    ~BootdSubscriber();

    void Init();
    boost::signals2::connection SubscribeBootStatus(boost::function<void(const pbnjson::JValue&)> func);

    void OnServerStatusChanged(bool connection);
    static bool OnBootStatusCallback(LSHandle* handle, LSMessage* lsmsg, void* user_data);
    const std::string& getBootStatus() const
    {
        return boot_status_str_;
    }

private:
    friend class Singleton<BootdSubscriber> ;

    void RequestBootStatus();

    LSMessageToken token_boot_status_;
    std::string boot_status_str_;
    boost::signals2::signal<void(const pbnjson::JValue&)> notify_boot_status;
};

#endif  // CORE_MODULE_SUBSCRIBER_OF_BOOTD_H_

