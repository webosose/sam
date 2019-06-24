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

    void Init();
    void AddRequiredKey(const std::string& key)
    {
        config_keys_.push_back(key);
    }
    boost::signals2::connection SubscribeConfigInfo(boost::function<void(pbnjson::JValue)> func);

    void OnServerStatusChanged(bool connection);
    static bool ConfigInfoCallback(LSHandle* handle, LSMessage* lsmsg, void* user_data);

private:
    friend class Singleton<ConfigdSubscriber> ;

    void RequestConfigInfo();

    std::vector<std::string> config_keys_;
    LSMessageToken token_config_info_;
    boost::signals2::signal<void(const pbnjson::JValue&)> notify_config_info;
};

#endif  // MODULE_CONFIGDSUBSCRIBER_H_

