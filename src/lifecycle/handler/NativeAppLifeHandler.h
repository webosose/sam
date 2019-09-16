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

#include <lifecycle/handler/native_interface/AbsNativeAppLifecycleInterface.h>
#include <lifecycle/IAppLifeHandler.h>
#include <list>
#include <memory>
#include <vector>
#include "interface/ISingleton.h"
#include "interface/IClassName.h"
#include "util/LinuxProcess.h"

class KillingData {
public:
    KillingData(const std::string& appId, const std::string& pid, const PidVector& allPids)
        : m_appId(appId),
          m_pid(pid),
          m_allPids(allPids),
          m_timerSource(0)
    {
    }

    virtual ~KillingData()
    {
        if (m_timerSource != 0) {
            g_source_remove(m_timerSource);
            m_timerSource = 0;
        }
    }

public:
    std::string m_appId;
    std::string m_pid;
    PidVector m_allPids;
    guint m_timerSource;
};

typedef std::shared_ptr<KillingData> KillingDataPtr;

class NativeAppLifeHandler: public IAppLifeHandler,
                            public ISingleton<NativeAppLifeHandler>,
                            public IClassName {
friend class AbsNativeAppLifecycleInterface;
friend class NativeAppLifeCycleInterfaceVer1;
friend class NativeAppLifeCycleInterfaceVer2;
friend class ISingleton<NativeAppLifeHandler>;
public:
    virtual ~NativeAppLifeHandler();

    virtual void launch(LaunchAppItemPtr item) override;
    virtual void close(CloseAppItemPtr item, std::string& errorText) override;
    virtual void pause(const std::string& appId, const pbnjson::JValue& params, std::string& errorText, bool send_life_event = true) override;

    void registerApp(const std::string& appId, LSMessage* message, std::string& errorText);

    boost::signals2::signal<void(const std::string& appId, const std::string& uid, const RuntimeStatus& life_status)> EventAppLifeStatusChanged;
    boost::signals2::signal<void(const std::string& appId, const std::string& pid, const std::string& webprocid)> EventRunningAppAdded;
    boost::signals2::signal<void(const std::string& appId)> EventRunningAppRemoved;
    boost::signals2::signal<void(const std::string& uid)> EventLaunchingDone;

private:
    NativeAppLifeHandler();

    // functions
    void addLaunchingItemIntoPendingQ(LaunchAppItemPtr item);
    LaunchAppItemPtr getLaunchPendingItem(const std::string& appId);
    void removeLaunchPendingItem(const std::string& appId);

    KillingDataPtr getKillingDataByAppId(const std::string& appId);
    void removeKillingData(const std::string& appId);

    bool findPidsAndSendSystemSignal(const std::string& pid, int signame);
    bool sendSystemSignal(const PidVector& pids, int signame);

    void startTimerToKillApp(const std::string& appId, const std::string& pid, const PidVector& all_pids, guint timeout);
    void stopTimerToKillApp(const std::string& appId);
    static gboolean killAppOnTimeout(gpointer context);

    NativeClientInfoPtr makeNewClientInfo(const std::string& appId);
    NativeClientInfoPtr getNativeClientInfo(const std::string& appId, bool make_new = false);
    NativeClientInfoPtr getNativeClientInfoByPid(const std::string& pid);
    NativeClientInfoPtr getNativeClientInfoOrMakeNew(const std::string& appId);
    void removeNativeClientInfo(const std::string& appId);
    void printNativeClients();

    static void onKillChildProcess(GPid pid, gint status, gpointer data);
    void handleClosedPid(const std::string& pid, gint status);

    void handlePendingQOnRegistered(const std::string& appId);
    void handlePendingQOnClosed(const std::string& appId);
    void cancelLaunchPendingItemAndMakeItDone(const std::string& appId);

    // variables
    std::list<KillingDataPtr> m_killingList;
    std::list<LaunchAppItemPtr> m_launchPendingQueue;
    std::vector<NativeClientInfoPtr> m_activeClients;

};

#endif
