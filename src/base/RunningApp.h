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

#ifndef RUNNING_APP_H
#define RUNNING_APP_H

#include <map>
#include <memory>
#include <string>
#include <pbnjson.hpp>

#include "base/LaunchPoint.h"
#include "base/LunaTask.h"
#include "base/LunaTaskList.h"
#include "conf/SAMConf.h"
#include "util/Logger.h"
#include "util/Time.h"
#include "util/NativeProcess.h"

//                  < RunningApp LIFECYCLES >
//
//                   (PRELOADING)------------->PRELOADED
//                      |                         |
//                      |                   (RELAUNCHING)   -----(PAUSING)------
//                      |                         |         |                  |
// STOP--(SPLASHING)-->SPLASHED--(LAUNCHING)-->FORGROUND----|                PAUSED
//                      |                         |         |                  |
//                      |                   (RELAUNCHING)   ----(RELAUNCHING)---
//                      |                         |
//                 (LAUNCHING:Hidden)---------->BACKGROUND

enum class LifeStatus : int8_t {
    LifeStatus_STOP,
    LifeStatus_PRELOADING, // ==> PRELOADED
    LifeStatus_PRELOADED, // ==> RELAUNCHING
    LifeStatus_SPLASHING, // ==> LAUNCHING
    LifeStatus_SPLASHED, // ==> LAUNCHING
    LifeStatus_LAUNCHING, // ==> FOREGROUND
    LifeStatus_RELAUNCHING, // ==> FOREGROUND
    LifeStatus_FOREGROUND, // ==> BACKGROUND
    LifeStatus_BACKGROUND, // ==> PAUSING
    LifeStatus_PAUSING, // ==> PAUSED
    LifeStatus_PAUSED, // ==> CLOSING
    LifeStatus_CLOSING, // ==> STOP
};

class RunningApp {
friend class RunningAppList;
public:
    static const char* toString(LifeStatus status);
    static bool isTransition(LifeStatus status);
    static string generateInstanceId(int displayId);
    static int getDisplayId(const string& instanceId);

    RunningApp(LaunchPointPtr launchPoint);
    virtual ~RunningApp();

    // APIs
    void registerApp(LunaTaskPtr lunaTask);

    bool sendEvent(JValue& payload);

    string getAppId() const
    {
        return m_launchPoint->getAppDesc()->getAppId();
    }

    string getLaunchPointId() const
    {
        return m_launchPoint->getLaunchPointId();
    }

    const string& getInstanceId() const
    {
        return m_instanceId;
    }
    void setInstanceId(const string& instanceId)
    {
        if (instanceId.empty()) {
            // TODO WAM should support 'instanceId' for other platforms
            // SAM just consider 0 as displayId for default.
            m_instanceId = generateInstanceId(0);
        } else {
            m_instanceId = instanceId;
        }
    }

    const string& getLS2Name() const
    {
        return m_ls2name;
    }
    void setLS2Name(const string& name)
    {
        m_ls2name = name;
    }

    LaunchPointPtr getLaunchPoint() const
    {
        return m_launchPoint;
    }
    void setLaunchPoint(LaunchPointPtr launchPoint)
    {
        m_launchPoint = std::move(launchPoint);
    }

    const string& getWindowId() const
    {
        return m_windowId;
    }
    void setWindowId(const string& windowId)
    {
        m_windowId = windowId;
    }

    int getDisplayId() const
    {
        return m_displayId;
    }
    void setDisplayId(const int displayId)
    {
        // TODO This is temp solution for support all platforms.
        if (displayId < 0)
            m_displayId = 0;
        else
            m_displayId = displayId;
    }

    bool isFullWindow() const
    {
        return m_isFullWindow;
    }
    void setFullWindow(const bool fullWindow)
    {
        m_isFullWindow = fullWindow;
    }

    pid_t getProcessId() const
    {
        return m_nativePocess.getPid();
    }
    void setProcessId(pid_t pid)
    {
        m_nativePocess.setPid(pid);
    }

    const string& getWebprocessid() const
    {
        return m_webprocessid;
    }
    void setWebprocid(const string& webprocid)
    {
        m_webprocessid = webprocid;
    }

    bool isRegistered()
    {
        if (!SAMConf::getInstance().isAppHandlingSupported())
            return false;
        return m_isRegistered;
    }

    LifeStatus getLifeStatus() const
    {
        return m_lifeStatus;
    }
    void setLifeStatus(LifeStatus lifeStatus);

    bool isTransition()
    {
        return RunningApp::isTransition(m_lifeStatus);
    }

    void loadRequestPayload(const JValue requestPayload)
    {
        if (!JValueUtil::getValue(requestPayload, "noSplash", m_noSplash)) {
            m_noSplash = this->getLaunchPoint()->getAppDesc()->isNoSplashOnLaunch();
        }
        if (!JValueUtil::getValue(requestPayload, "spinner", m_spinner)) {
            m_spinner = this->getLaunchPoint()->getAppDesc()->isSpinnerOnLaunch();
        }
        JValueUtil::getValue(requestPayload, "preload", m_preload);
        JValueUtil::getValue(requestPayload, "params", "launchedHidden", m_launchedHidden);

        JValueUtil::getValue(requestPayload, "keepAlive", m_keepAlive);
        if (m_keepAlive || SAMConf::getInstance().isKeepAliveApp(this->getAppId())) {
            m_keepAlive = true;
        }
    }

    string getPreload() const
    {
        // full, semi-full, partial, minimal
        return m_preload;
    }

    bool isKeepAlive() const
    {
        return m_keepAlive;
    }

    bool isShowSplash()
    {
        return !m_noSplash;
    }

    bool isShowSpinner() const
    {
        return m_spinner;
    }

    bool isLaunchedHidden() const
    {
        return m_launchedHidden;
    }

    bool isFirstLaunch()
    {
        return m_isFirstLaunch;
    }

    long long getTimeStamp() const
    {
        long long now = Time::getCurrentTime();
        return (now - m_startTime);
    }

    const string& getReason() const
    {
        return m_reason;
    }
    void setReason(const string& reason)
    {
        m_reason = reason;
    }

    LSMessageToken getToken() const
    {
        return m_token;
    }
    void setToken(LSMessageToken token)
    {
        m_token = token;
    }

    int getContext() const
    {
        return m_context;
    }
    void setContext(int context)
    {
        m_context = context;
    }

    NativeProcess& getLinuxProcess()
    {
        return m_nativePocess;
    }

    void toEventJson(JValue& json, LunaTaskPtr lunaTask, const string& event)
    {
        json.put("returnValue", true);
        json.put("subscribed", true);
        json.put("event", event);
        json.put("appId", getAppId());
        json.put("parameters", lunaTask->getParams());
        json.put("reason", lunaTask->getReason());
    }

    void toAPIJson(JValue& json, bool isRunningList)
    {
        json.put("instanceId", m_instanceId);
        json.put("launchPointId", m_launchPoint->getLaunchPointId());

        if (m_displayId != -1)
            json.put("displayId", m_displayId);

        // processId should be 'string' for backward compatibilty
        json.put("processid", std::to_string(m_nativePocess.getPid()));
        json.put("webprocessid", m_webprocessid);

        if (isRunningList) {
            json.put("id", m_launchPoint->getAppId());
            json.put("defaultWindowType", m_launchPoint->getAppDesc()->getDefaultWindowType());
            json.put("appType", AppDescription::toString(m_launchPoint->getAppDesc()->getAppType()));
        } else {
            json.put("appId", m_launchPoint->getAppId());
            json.put("status", toString(m_lifeStatus));
            json.put("reason", m_reason);
            json.put("type", AppDescription::toString(m_launchPoint->getAppDesc()->getAppType()));
        }
    }



private:
    static const string CLASS_NAME;
    static const int TIMEOUT_CLOSE = 1000; // 1 second
    static const int TIMEOUT_TRANSITION = 10000; // 10 seconds

    RunningApp(const RunningApp&);
    RunningApp& operator=(const RunningApp&) const;

    static gboolean onKillingTimer(gpointer context);
    void startKillingTimer(guint timeout);
    void stopKillingTimer();

    LaunchPointPtr m_launchPoint;

    string m_instanceId;
    int m_displayId;

    string m_webprocessid;
    string m_windowId;
    bool m_isFullWindow;

    LifeStatus m_lifeStatus;
    bool m_isFirstLaunch;
    long long m_startTime;
    guint m_killingTimer;

    // initial parameter
    string m_preload;
    bool m_keepAlive;
    bool m_noSplash;
    bool m_spinner;
    bool m_launchedHidden;

    string m_reason;
    LSMessageToken m_token;
    int m_context;

    // for native app
    NativeProcess m_nativePocess;
    string m_ls2name;
    bool m_isRegistered;
    LS::Message m_registeredApp;

};

typedef shared_ptr<RunningApp> RunningAppPtr;

#endif
