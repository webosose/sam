// Copyright (c) 2012-2019 LG Electronics, Inc.
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

#ifndef LSM_SUBSCRIBER_H_
#define LSM_SUBSCRIBER_H_

#include "AbsLunaClient.h"
#include "interface/ISingleton.h"
#include <luna-service2/lunaservice.hpp>
#include <boost/signals2.hpp>
#include <pbnjson.hpp>

using namespace LS;
using namespace pbnjson;

class LSM : public ISingleton<LSM>,
            public AbsLunaClient {
friend class ISingleton<LSM>;
public:
    LSM();
    virtual ~LSM();

    // AbsLunaClient
    virtual void onInitialze();
    virtual void onServerStatusChanged(bool isConnected);

private:
    static bool categoryWatcher(LSHandle* handle, LSMessage* lsmsg, void* userData);
    static bool onForegroundInfo(LSHandle* handle, LSMessage* lsmsg, void* userData);
    static bool onRecentList(LSHandle* handle, LSMessage* lsmsg, void* userData);

    void subscribeForegroundInfo();
    void subscribeRecentList();

public:
    boost::signals2::signal<void(const pbnjson::JValue&)> EventForegroundInfoChanged;
    boost::signals2::signal<void(const pbnjson::JValue&)> signal_recent_list;

private:
    LSMessageToken m_token_category_watcher;
    LSMessageToken m_token_foreground_info;
    LSMessageToken m_token_recent_list;
};

#endif

