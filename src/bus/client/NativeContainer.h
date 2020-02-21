// Copyright (c) 2012-2020 LG Electronics, Inc.
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

#ifndef BUS_CLIENT_NATIVECONTAINER_H_
#define BUS_CLIENT_NATIVECONTAINER_H_

#include <list>
#include <memory>
#include <vector>

#include "base/LaunchPointList.h"
#include "base/LunaTaskList.h"
#include "base/RunningAppList.h"
#include "interface/ISingleton.h"
#include "interface/IClassName.h"
#include "AbsLifeHandler.h"
#include "util/NativeProcess.h"

class NativeContainer : public ISingleton<NativeContainer>,
                        public IClassName,
                        public AbsLifeHandler {
friend class ISingleton<NativeContainer>;
public:
    static void onKillChildProcess(GPid pid, gint status, gpointer data);

    virtual ~NativeContainer();

    virtual void initialize();

    // AbsLifeHandler
    virtual bool launch(RunningApp& runningApp, LunaTaskPtr lunaTask) override;
    virtual bool relaunch(RunningApp& runningApp, LunaTaskPtr lunaTask) override;
    virtual bool pause(RunningApp& runningApp, LunaTaskPtr lunaTask) override;
    virtual bool term(RunningApp& runningApp, LunaTaskPtr lunaTask) override;
    virtual bool kill(RunningApp& runningApp) override;

private:
    static const string KEY_NATIVE_RUNNING_APPS;

    static int s_instanceCounter;

    NativeContainer();

    virtual void launchFromStop(RunningApp& runningApp, LunaTaskPtr item);
    virtual void launchFromRegistered(RunningApp& runningApp, LunaTaskPtr item);

    virtual void removeItem(GPid pid);
    virtual void addItem(const string& instanceId, const string& launchPointId, const int processId, const int displayId);

    map<string, string> m_environments;
    JValue m_nativeRunninApps;

};

#endif
