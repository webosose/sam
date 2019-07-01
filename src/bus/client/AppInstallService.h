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

#include "AbsLunaClient.h"
#include "interface/ISingleton.h"
#include <luna-service2/lunaservice.hpp>
#include <boost/signals2.hpp>
#include <pbnjson.hpp>

using namespace LS;
using namespace pbnjson;

enum class PackageStatus : int8_t {
    Unknown = 0,
    Installed,
    InstallFailed,
    Uninstalled,
    UninstallFailed,
    AboutToUninstall,
    ResetToDefault,
};

class AppInstallService : public ISingleton<AppInstallService>,
                          public AbsLunaClient {
friend class ISingleton<AppInstallService>;
public:
    virtual ~AppInstallService();

    // AbsLunaClient
    virtual void onInitialze();
    virtual void onServerStatusChanged(bool isConnected);

    boost::signals2::signal<void(const std::string&, const PackageStatus&)> EventStatusChanged;

private:
    static bool onStatus(LSHandle* handle, LSMessage* lsmsg, void* userData);

    AppInstallService();

    Call m_statusCall;
};

#endif  // BUS_CLIENT_APPINSTALLSERVICE_H_

