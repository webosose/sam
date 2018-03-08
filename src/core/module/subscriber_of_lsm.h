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

#ifndef LSM_SUBSCRIBER_H_
#define LSM_SUBSCRIBER_H_

#include <luna-service2/lunaservice.h>
#include <boost/signals2.hpp>
#include <pbnjson.hpp>

#include "core/base/singleton.h"

class LSMSubscriber: public Singleton<LSMSubscriber>
{
public:
    LSMSubscriber();
    ~LSMSubscriber();

    void init();
    void on_server_status_changed(bool connection);

    static bool category_watcher(LSHandle* handle, LSMessage* lsmsg, void* user_data);
    static bool cb_foreground_info(LSHandle* handle, LSMessage* lsmsg, void* user_data);
    static bool cb_recent_list(LSHandle* handle, LSMessage* lsmsg, void* user_data);

private:
    void subscribe_foreground_info();
    void subscribe_recent_list();

public:
    boost::signals2::signal<void (const pbnjson::JValue&)> signal_foreground_info;
    boost::signals2::signal<void (const pbnjson::JValue&)> signal_recent_list;

private:
    friend class Singleton<LSMSubscriber>;

    LSMessageToken m_token_category_watcher;
    LSMessageToken m_token_foreground_info;
    LSMessageToken m_token_recent_list;
};

#endif


