// Copyright (c) 2019 LG Electronics, Inc.
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

#ifndef LIFECYCLE_HANDLER_NATIVE_INTERFACE_INATIVEAPP_H_
#define LIFECYCLE_HANDLER_NATIVE_INTERFACE_INATIVEAPP_H_

#include <iostream>
#include "lifecycle/stage/appitem/LaunchAppItem.h"
#include "lifecycle/stage/appitem/CloseAppItem.h"
#include "lifecycle/AppLifeStatus.h"
#include "lifecycle/AppInfoManager.h"
#include "lifecycle/handler/NativeClientInfo.h"

#define TIME_LIMIT_OF_APP_LAUNCHING 3000000000u // 3 secs

class NativeAppLifeHandler;

// DesignPattern: Template Method
class INativeApp {
public:
    INativeApp();
    virtual ~INativeApp();

    // Main methods
    void launch(NativeClientInfoPtr client, LaunchAppItemPtr item);
    void close(NativeClientInfoPtr client, CloseAppItemPtr item, std::string& errorText);
    void pause(NativeClientInfoPtr, const pbnjson::JValue& params, std::string& errrorText, bool sendLifeEvent);

    void addLaunchHandler(RuntimeStatus status, NativeAppLaunchHandler handler);

    // launch template method
    void launchAsCommon(NativeClientInfoPtr client, LaunchAppItemPtr item);
    virtual void launchAfterClosedAsPolicy(NativeClientInfoPtr client, LaunchAppItemPtr item) = 0;
    virtual void launchNotRegisteredAppAsPolicy(NativeClientInfoPtr client, LaunchAppItemPtr item) = 0;
    virtual bool checkLaunchCondition(LaunchAppItemPtr item) = 0;
    virtual std::string makeForkArguments(LaunchAppItemPtr item, AppDescPtr app_desc) = 0;

    // relaunch template method
    void relaunchAsCommon(NativeClientInfoPtr client, LaunchAppItemPtr item);
    virtual void makeRelaunchParams(LaunchAppItemPtr item, pbnjson::JValue& j_params) = 0;

    // close template method
    virtual void closeAsPolicy(NativeClientInfoPtr client, CloseAppItemPtr item, std::string& err_text) = 0;

    // pause template method
    virtual void pauseAsPolicy(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& err_text, bool send_life_event) = 0;

protected:
    static const int TIMEOUT_FOR_FORCE_KILL = 1000;

private:
    std::map<RuntimeStatus, NativeAppLaunchHandler> m_launchHandlerMap;

};

#endif /* LIFECYCLE_HANDLER_NATIVE_INTERFACE_INATIVEAPP_H_ */
