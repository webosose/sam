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

#ifndef MODULE_CONFIGDSUBSCRIBER_H_
#define MODULE_CONFIGDSUBSCRIBER_H_

#include <luna-service2/lunaservice.h>
#include <boost/signals2.hpp>
#include <pbnjson.hpp>
#include <util/Singleton.h>


class ConfigdSubscriber: public Singleton<ConfigdSubscriber> {
public:
    ConfigdSubscriber();
    ~ConfigdSubscriber();

    void initialize();
    void addRequiredKey(const std::string& key)
    {
        m_configKeys.push_back(key);
    }
    boost::signals2::connection subscribeConfigInfo(boost::function<void(pbnjson::JValue)> func);

    void onServerStatusChanged(bool connection);
    static bool configInfoCallback(LSHandle* handle, LSMessage* lsmsg, void* user_data);

private:
    friend class Singleton<ConfigdSubscriber> ;

    void requestConfigInfo();

    std::vector<std::string> m_configKeys;
    LSMessageToken m_tokenConfigInfo;
    boost::signals2::signal<void(const pbnjson::JValue&)> notify_config_info;
};

#endif  // MODULE_CONFIGDSUBSCRIBER_H_

