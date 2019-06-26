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

#ifndef LIFECYCLE_HANDLER_NATIVE_INTERFACE_NATIVEAPPLIFECYCLEINTERFACEVER2_H_
#define LIFECYCLE_HANDLER_NATIVE_INTERFACE_NATIVEAPPLIFECYCLEINTERFACEVER2_H_

#include <lifecycle/handler/native_interface/INativeApp.h>

class NativeAppLifeCycleInterfaceVer2: public INativeApp {
public:
    NativeAppLifeCycleInterfaceVer2();
    virtual ~NativeAppLifeCycleInterfaceVer2()
    {
    }

private:
    static const int TIMEOUT_FOR_NOT_RESPONDING = 10000;
    // launch
    virtual bool checkLaunchCondition(LaunchAppItemPtr item);
    virtual std::string makeForkArguments(LaunchAppItemPtr item, AppDescPtr app_desc);
    virtual void launchAfterClosedAsPolicy(NativeClientInfoPtr client, LaunchAppItemPtr item);
    virtual void launchNotRegisteredAppAsPolicy(NativeClientInfoPtr client, LaunchAppItemPtr item);

    // relaunch
    virtual void makeRelaunchParams(LaunchAppItemPtr item, pbnjson::JValue& j_params);

    // close
    virtual void closeAsPolicy(NativeClientInfoPtr client, CloseAppItemPtr item, std::string& err_text);

    // pause
    virtual void pauseAsPolicy(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& err_text, bool send_life_event);
};

#endif /* LIFECYCLE_HANDLER_NATIVE_INTERFACE_NATIVEAPPLIFECYCLEINTERFACEVER2_H_ */
