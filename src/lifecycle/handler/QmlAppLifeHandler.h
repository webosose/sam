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

#ifndef QMLAPP_LIFE_HANDLER_H_
#define QMLAPP_LIFE_HANDLER_H_

#include <lifecycle/IAppLifeHandler.h>
#include "interface/ISingleton.h"

class QmlAppLifeHandler : public ISingleton<QmlAppLifeHandler>,
                          public IAppLifeHandler {
friend class ISingleton<QmlAppLifeHandler>;
public:
    virtual ~QmlAppLifeHandler();

    virtual void launch(LaunchAppItemPtr item) override;
    virtual void close(CloseAppItemPtr item, std::string& errorText) override;
    virtual void pause(const std::string& appId, const pbnjson::JValue& params, std::string& errorText, bool send_life_event = true) override;

    LaunchAppItemPtr getLSCallRequestItemByToken(const LSMessageToken& token);
    void removeItemFromLSCallRequestList(const std::string& uid);

    void onServiceReady();

    boost::signals2::signal<void(const std::string& appId, const std::string& uid, const RuntimeStatus& life_status)> EventAppLifeStatusChanged;
    boost::signals2::signal<void(const std::string& appId, const std::string& pid, const std::string& webprocid)> EventRunningAppAdded;
    boost::signals2::signal<void(const std::string& appId)> EventRunningAppRemoved;
    boost::signals2::signal<void(const std::string& uid)> EventLaunchingDone;

private:
    static const string LOG_NAME;

    static bool onReturnBoosterLaunch(LSHandle* sh, LSMessage* message, void* context);
    static bool onReturnBoosterClose(LSHandle* sh, LSMessage* message, void* context);
    static bool onQMLProcessFinished(LSHandle* sh, LSMessage* message, void* context);
    static bool onQMLProcessWatcher(LSHandle* sh, LSMessage* message, void* context);

    QmlAppLifeHandler();

    void initialize();

    LaunchItemList m_LSCallRequestList;
};

#endif
