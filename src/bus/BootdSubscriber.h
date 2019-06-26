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

#ifndef BUS_BOOTDSUBSCRIBER_H_
#define BUS_BOOTDSUBSCRIBER_H_

#include <luna-service2/lunaservice.h>
#include <boost/signals2.hpp>
#include <pbnjson.hpp>
#include <util/Singleton.h>


class BootdSubscriber: public Singleton<BootdSubscriber> {
public:
    BootdSubscriber();
    ~BootdSubscriber();

    void initialize();
    boost::signals2::connection subscribeBootStatus(boost::function<void(const pbnjson::JValue&)> func);

    void onServerStatusChanged(bool connection);
    static bool onBootStatusCallback(LSHandle* handle, LSMessage* lsmsg, void* user_data);
    const std::string& getBootStatus() const
    {
        return m_bootStatusStr;
    }

private:
    friend class Singleton<BootdSubscriber> ;

    void requestBootStatus();

    LSMessageToken m_tokenBootStatus;
    std::string m_bootStatusStr;
    boost::signals2::signal<void(const pbnjson::JValue&)> notify_boot_status;
};

#endif  // BUS_BOOTDSUBSCRIBER_H_

