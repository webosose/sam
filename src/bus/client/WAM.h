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

#ifndef BUS_CLIENT_WAM_H_
#define BUS_CLIENT_WAM_H_

#include "AbsLunaClient.h"
#include "interface/ISingleton.h"
#include "interface/IClassName.h"
#include <luna-service2/lunaservice.hpp>
#include <boost/signals2.hpp>
#include <pbnjson.hpp>
#include "util/Logger.h"

using namespace LS;
using namespace pbnjson;

class WAM : public ISingleton<WAM>,
            public AbsLunaClient,
            public IClassName {
friend class ISingleton<WAM>;
public:
    WAM();
    virtual ~WAM();

    // AbsLunaClient
    virtual void onInitialze();
    virtual void onServerStatusChanged(bool isConnected);

    boost::signals2::signal<void(JValue&)> EventListRunningAppsChanged;

private:
    static bool onListRunningApps(LSHandle* sh, LSMessage* message, void* context);

    Call m_listRunningAppsCall;
};

#endif /* BUS_CLIENT_WAM_H_ */
