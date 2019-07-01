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

#ifndef QMLAPP_LIFE_HANDLER_H_
#define QMLAPP_LIFE_HANDLER_H_

#include <lifecycle/IAppLifeHandler.h>

class QmlAppLifeHandler: public IAppLifeHandler {
public:
    QmlAppLifeHandler();
    virtual ~QmlAppLifeHandler();

    virtual void launch(LaunchAppItemPtr item) override;
    virtual void close(CloseAppItemPtr item, std::string& err_text) override;
    virtual void pause(const std::string& app_id, const pbnjson::JValue& params, std::string& err_text, bool send_life_event = true) override;

    LaunchAppItemPtr getLSCallRequestItemByToken(const LSMessageToken& token);
    void removeItemFromLSCallRequestList(const std::string& uid);

    void onServiceReady();

private:
    void initialize();
    static bool onReturnBoosterLaunch(LSHandle* handle, LSMessage* lsmsg, void* user_data);
    static bool onReturnBoosterClose(LSHandle* handle, LSMessage* lsmsg, void* user_data);
    static bool onQMLProcessFinished(LSHandle* handle, LSMessage* lsmsg, void* user_data);

    static bool onQMLProcessWatcher(LSHandle* handle, LSMessage* lsmsg, void* user_data);

public:
    boost::signals2::signal<void(const std::string& app_id, const std::string& uid, const RuntimeStatus& life_status)> EventAppLifeStatusChanged;
    boost::signals2::signal<void(const std::string& app_id, const std::string& pid, const std::string& webprocid)> EventRunningAppAdded;
    boost::signals2::signal<void(const std::string& app_id)> EventRunningAppRemoved;
    boost::signals2::signal<void(const std::string& uid)> EventLaunchingDone;

private:
    LaunchItemList m_LSCallRequestList;
};

#endif
