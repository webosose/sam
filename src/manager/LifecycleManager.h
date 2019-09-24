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

#ifndef APP_LIFE_MANAGER_H_
#define APP_LIFE_MANAGER_H_

#include <base/AppDescription.h>
#include <base/LunaTask.h>
#include <bus/client/SAM.h>
#include <lifecycle/LifecycleRouter.h>
#include <luna-service2/lunaservice.h>
#include "interface/IClassName.h"
#include "interface/ISingleton.h"
#include <tuple>

typedef std::tuple<std::string, AppType, double> LoadingAppItem;

class LifecycleManager : public ISingleton<LifecycleManager>,
                         public IClassName {
friend class ISingleton<LifecycleManager>;
public:
    static std::string toString(const LifeStatus& status);

    virtual ~LifecycleManager();

    void initialize();

    void registerApp(const std::string& appId, LSMessage* message, std::string& errorText);
    void registerNativeApp(const std::string& appId, LSMessage* message, std::string& errorText);
    void setAppLifeStatus(const std::string& appId, const std::string& uid, LifeStatus new_status);


    void generateLifeCycleEvent(const std::string& appId, const std::string& uid, LifeEvent event);

    boost::signals2::signal<void(const std::string& appId, const LifeStatus& life_status)> EventLifeStatusChanged;
    boost::signals2::signal<void(const pbnjson::JValue& foreground_info)> EventForegroundExtraInfoChanged;
    boost::signals2::signal<void(LunaTaskPtr)> EventLaunchingFinished;

private:
    LifecycleManager();

    void onWAMStatusChanged(bool isConnected);

    // app life cycle interfaces
    void onRunningAppAdded(const std::string& appId, const std::string& pid, const std::string& webprocid);
    void onRunningAppRemoved(const std::string& appId);
    void onRuntimeStatusChanged(const std::string& appId, const std::string& uid, const RuntimeStatus& life_status);

    void replySubscriptionOnLaunch(const std::string& appId, bool show_splash);
    void replySubscriptionForAppLifeStatus(const std::string& appId, const std::string& uid, const LifeStatus& life_status);

private:
    LifecycleRouter m_lifecycleRouter;

    std::map<std::string, std::string> m_closeReasonInfo;
};

#endif

