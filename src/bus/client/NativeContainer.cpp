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

#include "NativeContainer.h"

#include "base/AppDescription.h"
#include "base/LunaTaskList.h"
#include "base/AppDescriptionList.h"
#include "base/RunningAppList.h"
#include "conf/SAMConf.h"
#include "conf/RuntimeInfo.h"

const string NativeContainer::KEY_NATIVE_RUNNING_APPS = "nativeRunningApps";
int NativeContainer::s_instanceCounter = 1;

void NativeContainer::onKillChildProcess(GPid pid, gint status, gpointer data)
{
    Logger::info(getInstance().getClassName(), __FUNCTION__, Logger::format("Process(%d) was killed with status(%d)", pid, status));
    g_spawn_close_pid(pid);

    getInstance().removeItem(pid);
    if (!RunningAppList::getInstance().removeByPid(pid)) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "Failed to remove RunningApp (Internal Error)");
    }
}

NativeContainer::NativeContainer()
{
    setClassName("NativeContainer");
}

NativeContainer::~NativeContainer()
{

}

void NativeContainer::initialize()
{
    // Getting global environments
    gchar **variables = g_listenv();
    gsize size = variables ? g_strv_length(variables) : 0;
    for (uint i = 0; i < size; i++) {
    const gchar *value = g_getenv(variables[i]);
        if (value != NULL) {
            m_environments[variables[i]] = value;
        }
    }
    g_strfreev(variables);

    // Load already running native apps
    if (!RuntimeInfo::getInstance().getValue(KEY_NATIVE_RUNNING_APPS, m_nativeRunninApps)) {
        m_nativeRunninApps = pbnjson::Array();
        return;
    }
    size = m_nativeRunninApps.arraySize();
    for (gsize i = 0; i < size; ++i) {
        RunningAppPtr runningApp = RunningAppList::getInstance().createByJson(m_nativeRunninApps[i]);
        if (runningApp == nullptr) continue;

        // SAM doesn't know the proper status of already running native applications.
        // However, 'BACKGROUND' is reasonable status because 'FOREGROUND' event will be received from LSM
        runningApp->setLifeStatus(LifeStatus::LifeStatus_BACKGROUND);
        RunningAppList::getInstance().add(runningApp);
    }
}

void NativeContainer::launch(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    switch (runningApp.getLifeStatus()) {
    case LifeStatus::LifeStatus_SPLASHED:
    case LifeStatus::LifeStatus_STOP:
        launchFromStop(runningApp, lunaTask);
        break;

    case LifeStatus::LifeStatus_PRELOADED:
    case LifeStatus::LifeStatus_FOREGROUND:
    case LifeStatus::LifeStatus_BACKGROUND:
        if (!runningApp.isRegistered()) {
            runningApp.setLifeStatus(LifeStatus::LifeStatus_CLOSING);
            runningApp.getLinuxProcess().term();
        } else {
            launchFromRegistered(runningApp, lunaTask);
        }
        break;

    case LifeStatus::LifeStatus_CLOSING:
        runningApp.getLinuxProcess().kill();
        break;

    default:
        string errorText = string("Invalid LifeStatus:") + RunningApp::toString(runningApp.getLifeStatus());
        Logger::error(getClassName(), __FUNCTION__, lunaTask->getAppId(), errorText);
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, errorText);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
}

void NativeContainer::close(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    runningApp.toJson(lunaTask->getResponsePayload());

    if (LifeStatus::LifeStatus_STOP == runningApp.getLifeStatus()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid status of runningApp");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    if (runningApp.getProcessId() <= 0) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid processId");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    if (!runningApp.isRegistered()) {
        if (!runningApp.getLinuxProcess().kill()) {
            lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Cannot kill native application");
        }
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    JValue subscriptionPayload = pbnjson::Object();
    subscriptionPayload.put("event", "close");
    subscriptionPayload.put("reason", lunaTask->getReason());
    subscriptionPayload.put("returnValue", true);
    runningApp.sendEvent(subscriptionPayload);

    if (!runningApp.getLinuxProcess().term()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "not found any pids to kill");
    }
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void NativeContainer::launchFromStop(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    // construct the path for launching
    string path = runningApp.getLaunchPoint()->getAppDesc()->getAbsMain();
    if (path.find("file://", 0) != string::npos)
        path = path.substr(7);

    string appId = runningApp.getAppId();
    AppType appType = runningApp.getLaunchPoint()->getAppDesc()->getAppType();
    AppLocation appLocation = runningApp.getLaunchPoint()->getAppDesc()->getAppLocation();
    string strParams = runningApp.getLaunchParams(lunaTask);

    bool isNojailApp = SAMConf::getInstance().isNoJailApp(runningApp.getAppId());
    if (AppType::AppType_Native_AppShell == appType) {
        runningApp.getLinuxProcess().setCommand(SAMConf::getInstance().getAppShellRunnerPath());
        runningApp.getLinuxProcess().addArgument("--appid", appId);
        runningApp.getLinuxProcess().addArgument("--folder", runningApp.getLaunchPoint()->getAppDesc()->getFolderPath());
        runningApp.getLinuxProcess().addArgument("--params", strParams);
        Logger::info(getClassName(), __FUNCTION__, runningApp.getAppId(), "launch with appshell_runner");
    } else if (AppType::AppType_Native_Qml == appType) {
        runningApp.getLinuxProcess().setCommand(SAMConf::getInstance().getQmlRunnerPath());
        runningApp.getLinuxProcess().addArgument("--appid", appId);
        runningApp.getLinuxProcess().addArgument(strParams);
        Logger::info(getClassName(), __FUNCTION__, runningApp.getAppId(), "launch with qml_runner");
    } else if (SAMConf::getInstance().isJailerDisabled() && !isNojailApp) {
        const char* jailerType = "";
        if (AppLocation::AppLocation_Devmode == appLocation) {
            jailerType = "native_devmode";
        } else {
            switch (appType) {
            case AppType::AppType_Native: {
                jailerType = "native";
                break;
            }
            case AppType::AppType_Native_Builtin: {
                jailerType = "native_builtin";
                break;
            }
            case AppType::AppType_Native_Mvpd: {
                jailerType = "native_mvpd";
                break;
            }
            default:
                jailerType = "default";
                break;
            }
        }

        runningApp.getLinuxProcess().setCommand(SAMConf::getInstance().getJailerPath());
        runningApp.getLinuxProcess().addArgument("-t", jailerType);
        runningApp.getLinuxProcess().addArgument("-i", runningApp.getAppId());
        runningApp.getLinuxProcess().addArgument("-p", runningApp.getLaunchPoint()->getAppDesc()->getFolderPath());
        runningApp.getLinuxProcess().addArgument(path);
        runningApp.getLinuxProcess().addArgument(strParams);
        Logger::info(getClassName(), __FUNCTION__, runningApp.getAppId(), "launch with jail");
    } else {
        // This log shows whether native app's launched via Jailer or not
        // Do not remove this log, until jailer become stable
        runningApp.getLinuxProcess().setCommand(path);
        runningApp.getLinuxProcess().addArgument(strParams);
        Logger::info(getClassName(), __FUNCTION__, runningApp.getAppId(), "launch with root");
    }

    runningApp.getLinuxProcess().addEnv(m_environments);
    runningApp.getLinuxProcess().addEnv("instanceId", runningApp.getInstanceId());
    runningApp.getLinuxProcess().addEnv("launchPointId", runningApp.getLaunchPointId());
    runningApp.getLinuxProcess().addEnv("appId", runningApp.getAppId());
    runningApp.getLinuxProcess().addEnv("busName", Logger::format("%s-%d", runningApp.getAppId().c_str(), s_instanceCounter));
    runningApp.getLinuxProcess().addEnv("displayId", std::to_string(runningApp.getDisplayId()));
    runningApp.getLinuxProcess().openLogfile(Logger::format("%s/%s-%d", "/var/log", runningApp.getAppId().c_str(), s_instanceCounter++));
    runningApp.setLifeStatus(LifeStatus::LifeStatus_LAUNCHING);

    if (!runningApp.getLinuxProcess().run()) {
        runningApp.setLifeStatus(LifeStatus::LifeStatus_STOP);
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Failed to launch process");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    g_child_watch_add(runningApp.getLinuxProcess().getPid(), onKillChildProcess, nullptr);
    addItem(runningApp.getInstanceId(), runningApp.getLaunchPointId(), runningApp.getProcessId(), runningApp.getDisplayId());
    Logger::info(getClassName(), __FUNCTION__, appId, Logger::format("Launch Time: %f seconds", lunaTask->getTimeStamp()));

    lunaTask->getResponsePayload().put("returnValue", true);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void NativeContainer::launchFromRegistered(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    runningApp.setLifeStatus(LifeStatus::LifeStatus_LAUNCHING);
    Logger::info(getClassName(), __FUNCTION__, runningApp.getInstanceId());
    JValue payload = runningApp.getRelaunchParams(lunaTask);
    if (!runningApp.sendEvent(payload)) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Failed to send relaunch event");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    lunaTask->getResponsePayload().put("returnValue", true);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void NativeContainer::removeItem(GPid pid)
{
    gsize size = getInstance().m_nativeRunninApps.arraySize();
    for (gsize i = 0; i < size; ++i) {
        if (m_nativeRunninApps[i]["processId"].asNumber<int>() == pid) {
            m_nativeRunninApps.remove(i);
            break;
        }
    }
    RuntimeInfo::getInstance().setValue(KEY_NATIVE_RUNNING_APPS, m_nativeRunninApps);
}

void NativeContainer::addItem(const string& instanceId, const string& launchPointId, const int processId, const int displayId)
{
    JValue item = pbnjson::Object();
    item.put("instanceId", instanceId);
    item.put("launchPointId", launchPointId);
    item.put("processId", processId);
    item.put("displayId", displayId);
    m_nativeRunninApps.append(item);
    RuntimeInfo::getInstance().setValue(KEY_NATIVE_RUNNING_APPS, m_nativeRunninApps);
}

