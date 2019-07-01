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

#ifndef BUS_CLIENT_CONFIGD_H_
#define BUS_CLIENT_CONFIGD_H_

#include "AbsLunaClient.h"
#include "interface/ISingleton.h"
#include <luna-service2/lunaservice.hpp>
#include <boost/signals2.hpp>
#include <pbnjson.hpp>

using namespace LS;
using namespace pbnjson;

class Configd : public ISingleton<Configd>,
                public AbsLunaClient {
friend class ISingleton<Configd>;
public:
    virtual ~Configd();

    // AbsLunaClient
    virtual void onInitialze();
    virtual void onServerStatusChanged(bool isConnected);

    void addRequiredKey(const std::string& key)
    {
        m_configKeys.push_back(key);
    }

    static bool configInfoCallback(LSHandle* handle, LSMessage* lsmsg, void* userData);

    boost::signals2::signal<void(const pbnjson::JValue&)> EventConfigInfo;

private:
    Configd();

    void requestConfigInfo();

    std::vector<std::string> m_configKeys;
    LSMessageToken m_tokenConfigInfo;
};

#endif  // BUS_CLIENT_CONFIGD_H_

