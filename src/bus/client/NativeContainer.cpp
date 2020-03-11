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

    RunningAppPtr runningApp = RunningAppList::getInstance().getByPid(pid);
    LunaTaskPtr lunaTask = LunaTaskList::getInstance().getByToken(pid);
    if (runningApp == nullptr) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "Cannot find RunningApp");
        return;
    }

    getInstance().removeItem(pid);
    if (!RunningAppList::getInstance().removeByObject(runningApp)) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "Failed to remove RunningApp (Internal Error)");
    }
    if (lunaTask) {
        lunaTask->callback(lunaTask);
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

void NativeContainer::launch(RunningAppPtr runningApp, LunaTaskPtr lunaTask)
{
    AppType type = runningApp->getLaunchPoint()->getAppDesc()->getAppType();

    JValue params;
    if (AppType::AppType_Native_Qml == type) {
        params = pbnjson::Object();
        params.put("main", runningApp->getLaunchPoint()->getAppDesc()->getAbsMain());
        params.put("appId", runningApp->getLaunchPoint()->getAppDesc()->getAppId());
        params.put("params", lunaTask->getParams());
    } else {
        params = runningApp->getLaunchPoint()->getParams(lunaTask);
        params.put("event", "launch");
        params.put("reason", lunaTask->getReason());
        params.put("appId", lunaTask->getAppId());
        params.put("nid", lunaTask->getAppId());
        params.put("@system_native_app", true);
    }

    if (!runningApp->getPreload().empty()) {
        params.put("preload", runningApp->getPreload());
    }

    bool isNojailApp = SAMConf::getInstance().isNoJailApp(runningApp->getAppId());
    AppType appType = runningApp->getLaunchPoint()->getAppDesc()->getAppType();
    string path = runningApp->getLaunchPoint()->getAppDesc()->getAbsMain();
    if (path.find("file://", 0) != string::npos)
        path = path.substr(7);

    switch (appType) {
    case AppType::AppType_Native_AppShell:
        runningApp->getLinuxProcess().setCommand(SAMConf::getInstance().getAppShellRunnerPath());
        runningApp->getLinuxProcess().addArgument("--appid", runningApp->getAppId());
        runningApp->getLinuxProcess().addArgument("--folder", runningApp->getLaunchPoint()->getAppDesc()->getFolderPath());
        runningApp->getLinuxProcess().addArgument("--params", params.stringify());
        Logger::info(getClassName(), __FUNCTION__, runningApp->getAppId(), "launch with appshell_runner");
        break;

    case AppType::AppType_Native_Qml:
        runningApp->getLinuxProcess().setCommand(SAMConf::getInstance().getQmlRunnerPath());
        runningApp->getLinuxProcess().addArgument("--appid", runningApp->getAppId());
        runningApp->getLinuxProcess().addArgument(params.stringify());
        Logger::info(getClassName(), __FUNCTION__, runningApp->getAppId(), "launch with qml_runner");
        break;

    default: // Native Apps
        if (SAMConf::getInstance().isJailerDisabled() || isNojailApp) {
            runningApp->getLinuxProcess().setWorkingDirectory(runningApp->getLaunchPoint()->getAppDesc()->getFolderPath());
            runningApp->getLinuxProcess().setCommand(path);
            runningApp->getLinuxProcess().addArgument(params.stringify());
            Logger::info(getClassName(), __FUNCTION__, runningApp->getAppId(), "launch with root");
        } else {
            const char* jailerType = "";
            if (AppLocation::AppLocation_Devmode == runningApp->getLaunchPoint()->getAppDesc()->getAppLocation()) {
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

            runningApp->getLinuxProcess().setCommand(SAMConf::getInstance().getJailerPath());
            runningApp->getLinuxProcess().addArgument("-t", jailerType);
            runningApp->getLinuxProcess().addArgument("-i", runningApp->getAppId());
            runningApp->getLinuxProcess().addArgument("-p", runningApp->getLaunchPoint()->getAppDesc()->getFolderPath());
            runningApp->getLinuxProcess().addArgument(path);
            runningApp->getLinuxProcess().addArgument(params.stringify());
            Logger::info(getClassName(), __FUNCTION__, runningApp->getAppId(), "launch with jail");
        }
    }

    runningApp->getLinuxProcess().addEnv(m_environments);
    runningApp->getLinuxProcess().addEnv("INSTANCE_ID", runningApp->getInstanceId());
    runningApp->getLinuxProcess().addEnv("LAUNCHPOINT_ID", runningApp->getLaunchPointId());
    runningApp->getLinuxProcess().addEnv("APP_ID", runningApp->getAppId());
    runningApp->getLinuxProcess().addEnv("DISPLAY_ID", std::to_string(runningApp->getDisplayId()));
    runningApp->getLinuxProcess().addEnv("LS2_NAME", Logger::format("%s-%d", runningApp->getAppId().c_str(), s_instanceCounter));
    runningApp->getLinuxProcess().openLogfile(Logger::format("%s/%s-%d", "/var/log", runningApp->getAppId().c_str(), s_instanceCounter++));
    runningApp->setLifeStatus(LifeStatus::LifeStatus_LAUNCHING);

    if (!runningApp->getLinuxProcess().run()) {
        RunningAppList::getInstance().removeByObject(runningApp);
        LunaTaskList::getInstance().removeAfterReply(lunaTask, ErrCode_LAUNCH, "Failed to launch process");
        return;
    }

    g_child_watch_add(runningApp->getLinuxProcess().getPid(), onKillChildProcess, nullptr);
    runningApp->getLinuxProcess().track();

    addItem(runningApp->getInstanceId(), runningApp->getLaunchPointId(), runningApp->getProcessId(), runningApp->getDisplayId());
    Logger::info(getClassName(), __FUNCTION__, runningApp->getAppId(), Logger::format("Launch Time: %lld ms", runningApp->getTimeStamp()));
    lunaTask->callback(lunaTask);
}

void NativeContainer::pause(RunningAppPtr runningApp, LunaTaskPtr lunaTask)
{
    close(runningApp, lunaTask);
}

void NativeContainer::close(RunningAppPtr runningApp, LunaTaskPtr lunaTask)
{
    runningApp->setLifeStatus(LifeStatus::LifeStatus_CLOSING);
    if (!runningApp->getLinuxProcess().term()) {
        kill(runningApp);
        lunaTask->callback(lunaTask);
        return;
    }
    runningApp->setToken(runningApp->getProcessId());
    lunaTask->setToken(runningApp->getProcessId());

    if (!runningApp->getLinuxProcess().isTracked()) {
        NativeContainer::onKillChildProcess(runningApp->getProcessId(), 0, NULL);
        lunaTask->callback(lunaTask);
    }
}

void NativeContainer::kill(RunningAppPtr runningApp)
{
    runningApp->setLifeStatus(LifeStatus::LifeStatus_CLOSING);
    if (!runningApp->getLinuxProcess().kill()) {
        RunningAppList::getInstance().removeByObject(runningApp);
    }
    runningApp->setToken(runningApp->getProcessId());
}

void NativeContainer::removeItem(GPid pid)
{
    gsize size = getInstance().m_nativeRunninApps.arraySize();
    for (gsize i = 0; i < size; ++i) {
        if (m_nativeRunninApps[i]["processId"].asNumber<int>() == pid) {
            m_nativeRunninApps.remove(i);
            RuntimeInfo::getInstance().setValue(KEY_NATIVE_RUNNING_APPS, m_nativeRunninApps);
            break;
        }
    }
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

