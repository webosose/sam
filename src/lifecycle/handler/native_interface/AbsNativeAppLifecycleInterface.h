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

#ifndef LIFECYCLE_HANDLER_NATIVE_INTERFACE_ABSNATIVEAPPLIFECYCLEINTERFACE_H_
#define LIFECYCLE_HANDLER_NATIVE_INTERFACE_ABSNATIVEAPPLIFECYCLEINTERFACE_H_

#include <lifecycle/LifecycleRouter.h>
#include <lifecycle/RunningInfoManager.h>
#include <iostream>
#include "lifecycle/stage/appitem/LaunchAppItem.h"
#include "lifecycle/stage/appitem/CloseAppItem.h"
#include "lifecycle/handler/NativeClientInfo.h"

#define TIME_LIMIT_OF_APP_LAUNCHING 3000000000u // 3 secs

class AbsNativeAppLifecycleInterface {
public:
    AbsNativeAppLifecycleInterface();
    virtual ~AbsNativeAppLifecycleInterface();

    virtual void launch(NativeClientInfoPtr client, LaunchAppItemPtr item);
    virtual void close(NativeClientInfoPtr client, CloseAppItemPtr item, std::string& errorText) = 0;
    virtual void pause(NativeClientInfoPtr, const pbnjson::JValue& params, std::string& errrorText, bool sendLifeEvent) = 0;

protected:
    virtual void launchFromStop(NativeClientInfoPtr client, LaunchAppItemPtr item);
    virtual void launchFromRegistered(NativeClientInfoPtr client, LaunchAppItemPtr item);
    virtual void launchFromRunning(NativeClientInfoPtr client, LaunchAppItemPtr item) = 0;
    virtual void launchFromClosing(NativeClientInfoPtr client, LaunchAppItemPtr item) = 0;

    virtual bool canLaunch(LaunchAppItemPtr item) = 0;
    virtual bool getLaunchParams(LaunchAppItemPtr item, AppPackagePtr appDescPtr, std::string& params) = 0;
    virtual bool getRelaunchParams(LaunchAppItemPtr item, AppPackagePtr appDescPtr, pbnjson::JValue& params) = 0;

    static const int TIMEOUT_FOR_FORCE_KILL = 1000;

};

#endif /* LIFECYCLE_HANDLER_NATIVE_INTERFACE_ABSNATIVEAPPLIFECYCLEINTERFACE_H_ */
