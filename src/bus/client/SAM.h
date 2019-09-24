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

#ifndef NATIVEAPP_LIFE_HANDLER_H_
#define NATIVEAPP_LIFE_HANDLER_H_

#include <list>
#include <memory>
#include <vector>
#include <boost/signals2.hpp>

#include "base/LaunchPointList.h"
#include "base/LunaTaskList.h"
#include "base/RunningAppList.h"
#include "interface/ISingleton.h"
#include "interface/IClassName.h"
#include "util/LinuxProcess.h"

class SAM : public ISingleton<SAM>,
            public IClassName {
friend class AbsInterface;
friend class InterfaceVer1;
friend class InterfaceVer2;
friend class ISingleton<SAM>;
public:
    virtual ~SAM();

    virtual void launch(LunaTaskPtr item);
    virtual void close(LunaTaskPtr item);

    boost::signals2::signal<void(const std::string& appId, const std::string& uid, const RuntimeStatus& life_status)> EventAppLifeStatusChanged;
    boost::signals2::signal<void(const std::string& appId, const std::string& pid, const std::string& webprocid)> EventRunningAppAdded;
    boost::signals2::signal<void(const std::string& appId)> EventRunningAppRemoved;

private:
    static const int TIMEOUT_1_SECOND = 1000;
    static const int TIMEOUT_10_SECONDS = 10000;

    static void onKillChildProcess(GPid pid, gint status, gpointer data);

    SAM();

    virtual void launchFromStop(LunaTaskPtr item);
    virtual void launchFromRegistered(LunaTaskPtr item);
    virtual void launchFromClosing(LunaTaskPtr item);
    virtual void launchFromRunning(LunaTaskPtr item);

};

#endif
