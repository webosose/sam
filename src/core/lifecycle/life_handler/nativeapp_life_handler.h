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

#ifndef NATIVEAPP_LIFE_HANDLER_H_
#define NATIVEAPP_LIFE_HANDLER_H_

#include <list>
#include <memory>
#include <vector>

#include "core/lifecycle/life_handler/life_handler_interface.h"

typedef std::vector<pid_t> PidVector;

class KillingData {
public:
    KillingData(const std::string& app_id, const std::string& pid, const PidVector& all_pids)
        : m_appId(app_id),
          m_pid(pid),
          m_allPids(all_pids),
          m_timerSource(0)
    {
    }
    ~KillingData()
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

class NativeClientInfo;
typedef std::shared_ptr<NativeClientInfo> NativeClientInfoPtr;
typedef boost::function<void(NativeClientInfoPtr, AppLaunchingItemPtr)> NativeAppLaunchHandler;

class NativeAppLifeHandler;

// DesignPattern: Template Method
class NativeAppLifeCycleInterface {
public:
    NativeAppLifeCycleInterface(NativeAppLifeHandler* parent);
    virtual ~NativeAppLifeCycleInterface();

    // Main methods
    void launch(NativeClientInfoPtr client, AppLaunchingItemPtr item);
    void close(NativeClientInfoPtr client, AppCloseItemPtr item, std::string& err_text);
    void pause(NativeClientInfoPtr, const pbnjson::JValue& params, std::string& err_text, bool send_life_event);

    NativeAppLifeHandler* parent() const
    {
        return m_parent;
    }
    void addLaunchHandler(RuntimeStatus status, NativeAppLaunchHandler handler);

    // launch template method
    void launchAsCommon(NativeClientInfoPtr client, AppLaunchingItemPtr item);
    virtual void launchAfterClosedAsPolicy(NativeClientInfoPtr client, AppLaunchingItemPtr item) = 0;
    virtual void launchNotRegisteredAppAsPolicy(NativeClientInfoPtr client, AppLaunchingItemPtr item) = 0;
    virtual bool checkLaunchCondition(AppLaunchingItemPtr item) = 0;
    virtual std::string makeForkArguments(AppLaunchingItemPtr item, AppDescPtr app_desc) = 0;

    // relaunch template method
    void relaunchAsCommon(NativeClientInfoPtr client, AppLaunchingItemPtr item);
    virtual void makeRelaunchParams(AppLaunchingItemPtr item, pbnjson::JValue& j_params) = 0;

    // close template method
    virtual void closeAsPolicy(NativeClientInfoPtr client, AppCloseItemPtr item, std::string& err_text) = 0;

    // pause template method
    virtual void pauseAsPolicy(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& err_text, bool send_life_event) = 0;

private:
    NativeAppLifeHandler* m_parent;
    std::map<RuntimeStatus, NativeAppLaunchHandler> m_launchHandlerMap;

};

class NativeAppLifeCycleInterfaceVer1: public NativeAppLifeCycleInterface {
public:
    NativeAppLifeCycleInterfaceVer1(NativeAppLifeHandler* parent);
    virtual ~NativeAppLifeCycleInterfaceVer1()
    {
    }

private:
    // launch
    virtual bool checkLaunchCondition(AppLaunchingItemPtr item);
    virtual std::string makeForkArguments(AppLaunchingItemPtr item, AppDescPtr app_desc);
    virtual void launchAfterClosedAsPolicy(NativeClientInfoPtr client, AppLaunchingItemPtr item);
    virtual void launchNotRegisteredAppAsPolicy(NativeClientInfoPtr client, AppLaunchingItemPtr item);

    // relaunch
    virtual void makeRelaunchParams(AppLaunchingItemPtr item, pbnjson::JValue& j_params);

    // close
    virtual void closeAsPolicy(NativeClientInfoPtr client, AppCloseItemPtr item, std::string& err_text);

    // pause
    virtual void pauseAsPolicy(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& err_text, bool send_life_event);
};

class NativeAppLifeCycleInterfaceVer2: public NativeAppLifeCycleInterface {
public:
    NativeAppLifeCycleInterfaceVer2(NativeAppLifeHandler* parent);
    virtual ~NativeAppLifeCycleInterfaceVer2()
    {
    }

private:
    // launch
    virtual bool checkLaunchCondition(AppLaunchingItemPtr item);
    virtual std::string makeForkArguments(AppLaunchingItemPtr item, AppDescPtr app_desc);
    virtual void launchAfterClosedAsPolicy(NativeClientInfoPtr client, AppLaunchingItemPtr item);
    virtual void launchNotRegisteredAppAsPolicy(NativeClientInfoPtr client, AppLaunchingItemPtr item);

    // relaunch
    virtual void makeRelaunchParams(AppLaunchingItemPtr item, pbnjson::JValue& j_params);

    // close
    virtual void closeAsPolicy(NativeClientInfoPtr client, AppCloseItemPtr item, std::string& err_text);

    // pause
    virtual void pauseAsPolicy(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& err_text, bool send_life_event);
};

class NativeClientInfo {
public:
    NativeClientInfo(const std::string& app_id);
    ~NativeClientInfo();

    void Register(LSMessage* lsmsg);
    void Unregister();

    void StartTimerForCheckingRegistration();
    void StopTimerForCheckingRegistration();
    static gboolean CheckRegistration(gpointer user_data);

    void SetLifeCycleHandler(int ver, NativeAppLifeCycleInterface* handler);
    NativeAppLifeCycleInterface* GetLifeCycleHandler() const
    {
        return m_lifeCycleHandler;
    }
    bool SendEvent(pbnjson::JValue& payload);
    void SetAppId(const std::string& app_id)
    {
        m_appId = app_id;
    }
    void SetPid(const std::string& pid)
    {
        m_pid = pid;
    }

    const std::string& AppId() const
    {
        return m_appId;
    }
    const std::string& Pid() const
    {
        return m_pid;
    }
    int InterfaceVersion() const
    {
        return m_interfaceVersion;
    }
    bool IsRegistered() const
    {
        return m_isRegistered;
    }
    bool IsRegistrationExpired() const
    {
        return m_isRegistrationExpired;
    }

private:
    std::string m_appId;
    std::string m_pid;
    int m_interfaceVersion;
    bool m_isRegistered;
    bool m_isRegistrationExpired;
    LSMessage* m_lsmsg;
    guint m_registrationCheckTimerSource;
    double m_registrationCheckStartTime;
    NativeAppLifeCycleInterface* m_lifeCycleHandler;
};

class NativeAppLifeHandler: public AppLifeHandlerInterface {
public:
    NativeAppLifeHandler();
    virtual ~NativeAppLifeHandler();

    virtual void launch(AppLaunchingItemPtr item) override;
    virtual void close(AppCloseItemPtr item, std::string& err_text) override;
    virtual void pause(const std::string& app_id, const pbnjson::JValue& params, std::string& err_text, bool send_life_event = true) override;

    void RegisterApp(const std::string& app_id, LSMessage* lsmsg, std::string& err_text);
    bool ChangeRunningAppId(const std::string& current_id, const std::string& target_id, ErrorInfo& err_info);

    boost::signals2::signal<void(const std::string& app_id, const std::string& uid, const RuntimeStatus& life_status)> signal_app_life_status_changed;
    boost::signals2::signal<void(const std::string& app_id, const std::string& pid, const std::string& webprocid)> signal_running_app_added;
    boost::signals2::signal<void(const std::string& app_id)> signal_running_app_removed;
    boost::signals2::signal<void(const std::string& uid)> signal_launching_done;

private:
    // declare friends for template methods pattern
    friend class NativeAppLifeCycleInterface;
    friend class NativeAppLifeCycleInterfaceVer1;
    friend class NativeAppLifeCycleInterfaceVer2;

    // functions
    void AddLaunchingItemIntoPendingQ(AppLaunchingItemPtr item);
    AppLaunchingItemPtr GetLaunchPendingItem(const std::string& app_id);
    void RemoveLaunchPendingItem(const std::string& app_id);

    KillingDataPtr GetKillingDataByAppId(const std::string& app_id);
    void RemoveKillingData(const std::string& app_id);

    bool FindPidsAndSendSystemSignal(const std::string& pid, int signame);
    bool SendSystemSignal(const PidVector& pids, int signame);

    void StartTimerToKillApp(const std::string& app_id, const std::string& pid, const PidVector& all_pids, guint timeout);
    void StopTimerToKillApp(const std::string& app_id);
    static gboolean KillAppOnTimeout(gpointer user_data);

    NativeClientInfoPtr MakeNewClientInfo(const std::string& app_id);
    NativeClientInfoPtr GetNativeClientInfo(const std::string& app_id, bool make_new = false);
    NativeClientInfoPtr GetNativeClientInfoByPid(const std::string& pid);
    NativeClientInfoPtr GetNativeClientInfoOrMakeNew(const std::string& app_id);
    void RemoveNativeClientInfo(const std::string& app_id);
    void PrintNativeClients();

    static void ChildProcessWatcher(GPid pid, gint status, gpointer data);
    void HandleClosedPid(const std::string& pid, gint status);

    void HandlePendingQOnRegistered(const std::string& app_id);
    void HandlePendingQOnClosed(const std::string& app_id);
    void CancelLaunchPendingItemAndMakeItDone(const std::string& app_id);

    // variables
    std::list<KillingDataPtr> m_killingList;
    std::list<AppLaunchingItemPtr> m_launchPendingQueue;
    std::vector<NativeClientInfoPtr> m_activeClients;

    NativeAppLifeCycleInterfaceVer1 m_nativeHandlerV1;
    NativeAppLifeCycleInterfaceVer2 m_nativeHandlerV2;
};

#endif
