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

#ifndef PRELAUNCHER_4_BASE_H
#define PRELAUNCHER_4_BASE_H

#include "extensions/webos_base/lifecycle/app_launching_item_4_base.h"
#include "interface/lifecycle/prelauncher_interface.h"

class Prelauncher4Base: public PrelauncherInterface {
public:
    Prelauncher4Base();
    virtual ~Prelauncher4Base();

    virtual void addItem(AppLaunchingItemPtr item);
    virtual void removeItem(const std::string& app_uid);
    virtual void inputBridgedReturn(AppLaunchingItemPtr item, const pbnjson::JValue& jmsg);
    virtual void cancelAll();

private:
    static bool cbReturnLSCall(LSHandle* handle, LSMessage* lsmsg, void* user_data);
    static bool cbReturnLSCallForBridgedRequest(LSHandle* handle, LSMessage* lsmsg, void* user_data);

    void runStages(AppLaunchingItem4BasePtr item);
    void handleStages(AppLaunchingItem4BasePtr prelaunching_item);

    void redirectToAnother(AppLaunchingItem4BasePtr prelaunching_item);
    void finishPrelaunching(AppLaunchingItem4BasePtr prelaunching_item);

    AppLaunchingItem4BasePtr getLSCallRequestItemByToken(const LSMessageToken& token);
    AppLaunchingItem4BasePtr getItemByUid(const std::string& uid);
    void removeItemFromLSCallRequestList(const std::string& uid);

private:
    AppLaunchingItem4BaseList m_itemQueue;
    AppLaunchingItem4BaseList m_lscallRequestList;

};

#endif
