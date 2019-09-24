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
#include "util/Logger.h"

using namespace LS;
using namespace pbnjson;

class LSM : public ISingleton<LSM>,
            public AbsLunaClient {
friend class ISingleton<LSM>;
public:
    virtual ~LSM();

    boost::signals2::signal<void(const pbnjson::JValue&)> EventRecentsAppListChanged;

protected:
    // AbsLunaClient
    virtual void onInitialze();
    virtual void onServerStatusChanged(bool isConnected);

private:
    static const string NAME;
    static const string NAME_GET_FOREGROUND_APP_INFO;
    static const string NAME_GET_RECENTS_APP_LIST;

    static bool onServiceCategoryChanged(LSHandle* sh, LSMessage* message, void* context);
    static bool onGetForegroundAppInfo(LSHandle* sh, LSMessage* message, void* context);
    static bool onGetRecentsAppList(LSHandle* sh, LSMessage* message, void* context);

    LSM();

    bool isFullscreenWindowType(const pbnjson::JValue& foreground_info);
    void subscribeGetForegroundAppInfo();
    void subscribeGetRecentsAppList();

    Call m_registerServiceCategoryCall;
    Call m_getRecentsAppListCall;
    Call m_getForegroundAppInfoCall;

};

#endif

