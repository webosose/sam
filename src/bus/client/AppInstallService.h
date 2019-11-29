// Copyright (c) 2017-2019 LG Electronics, Inc.
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

#ifndef BUS_CLIENT_APPINSTALLSERVICE_H_
#define BUS_CLIENT_APPINSTALLSERVICE_H_

#include <luna-service2/lunaservice.hpp>
#include <boost/signals2.hpp>
#include <pbnjson.hpp>

#include "AbsLunaClient.h"
#include "interface/ISingleton.h"
#include "util/JValueUtil.h"
#include "util/Logger.h"

using namespace LS;
using namespace pbnjson;

class AppInstallService : public ISingleton<AppInstallService>,
                          public AbsLunaClient {
friend class ISingleton<AppInstallService>;
public:
    virtual ~AppInstallService();

    static bool onRemove(LSHandle* sh, LSMessage *message, void* context);
    Call remove(const string& appId);

protected:
    // AbsLunaClient
    virtual void onInitialzed() override;
    virtual void onFinalized() override;
    virtual void onServerStatusChanged(bool isConnected) override;

private:
    static bool onStatus(LSHandle* sh, LSMessage* message, void* context);

    AppInstallService();

    Call m_statusCall;
};

#endif  // BUS_CLIENT_APPINSTALLSERVICE_H_

