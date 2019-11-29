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

#include "NativeContainer.h"

#include <ext/stdio_filebuf.h>
#include <boost/lexical_cast.hpp>

#include "base/AppDescription.h"
#include "base/LunaTaskList.h"
#include "base/AppDescriptionList.h"
#include "base/RunningAppList.h"
#include "conf/SAMConf.h"

void NativeContainer::onKillChildProcess(GPid pid, gint status, gpointer data)
{
    Logger::info(getInstance().getClassName(), __FUNCTION__, Logger::format("Process(%d) was killed with status(%d)", pid, status));
    g_spawn_close_pid(pid);

    LunaTaskPtr lunaTask = LunaTaskList::getInstance().getByToken(pid);
    string pidStr = boost::lexical_cast<string>(pid);

    if (RunningAppList::getInstance().removeByPid(pidStr)) {
        if (lunaTask) {
            lunaTask->getResponsePayload().put("returnValue", true);
            LunaTaskList::getInstance().removeAfterReply(lunaTask);
        }
    } else {
        if (lunaTask) {
            lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Failed to remove RunningApp (Internal Error)");
            LunaTaskList::getInstance().removeAfterReply(lunaTask);
        }
    }
}

NativeContainer::NativeContainer()
{
    setClassName("NativeContainer");
}

NativeContainer::~NativeContainer()
{
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
            LinuxProcess::sendSigTerm(runningApp.getProcessId());
        } else {
            launchFromRegistered(runningApp, lunaTask);
        }
        break;

    case LifeStatus::LifeStatus_CLOSING:
        LinuxProcess::sendSigKill(runningApp.getProcessId());
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
    if (LifeStatus::LifeStatus_STOP == runningApp.getLifeStatus()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "native app is not running");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    lunaTask->setToken(stoi(runningApp.getProcessId()));
    if (!runningApp.isRegistered()) {
        if (!LinuxProcess::sendSigKill(runningApp.getProcessId())) {
            lunaTask->setErrCodeAndText(ErrCode_GENERAL, "not found any pids to kill");
            LunaTaskList::getInstance().removeAfterReply(lunaTask);
        }
        return;
    }

    JValue subscriptionPayload = pbnjson::Object();
    subscriptionPayload.put("event", "close");
    subscriptionPayload.put("reason", lunaTask->getReason());
    subscriptionPayload.put("returnValue", true);
    runningApp.sendEvent(subscriptionPayload);

    if (!LinuxProcess::sendSigTerm(runningApp.getProcessId())) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "not found any pids to kill");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
}

void NativeContainer::launchFromStop(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    // construct the path for launching
    string path = runningApp.getLaunchPoint()->getAppDesc()->getAbsMain();
    if (path.find("file://", 0) != string::npos)
        path = path.substr(7);

    const char* forkParams[10] = { 0, };
    const char* jailerType = "";
    bool isNojailApp = SAMConf::getInstance().isNoJailApp(runningApp.getAppId());

    string appId = runningApp.getAppId();
    AppType appType = runningApp.getLaunchPoint()->getAppDesc()->getAppType();
    AppLocation appLocation = runningApp.getLaunchPoint()->getAppDesc()->getAppLocation();
    string strParams = runningApp.getLaunchParams(lunaTask);

    if (AppType::AppType_Native_AppShell == appType) {
        forkParams[0] = SAMConf::getInstance().getAppShellRunnerPath().c_str();
        forkParams[1] = "--appid";
        forkParams[2] = appId.c_str();
        forkParams[3] = "--folder";
        forkParams[4] = runningApp.getLaunchPoint()->getAppDesc()->getFolderPath().c_str();
        forkParams[5] = "--params";
        forkParams[6] = strParams.c_str();
        forkParams[7] = NULL;

        Logger::info(getClassName(), __FUNCTION__, runningApp.getAppId(), "launch with appshell_runner");
    } else if (AppType::AppType_Native_Qml == appType) {
        forkParams[0] = SAMConf::getInstance().getQmlRunnerPath().c_str();
        forkParams[1] = strParams.c_str();
        forkParams[2] = NULL;

        Logger::info(getClassName(), __FUNCTION__, runningApp.getAppId(), "launch with qml_runner");
    } else if (SAMConf::getInstance().isJailerDisabled() && !isNojailApp) {
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

        forkParams[0] = SAMConf::getInstance().getJailerPath().c_str();
        forkParams[1] = "-t";
        forkParams[2] = jailerType;
        forkParams[3] = "-i";
        forkParams[4] = runningApp.getAppId().c_str();
        forkParams[5] = "-p";
        forkParams[6] = runningApp.getLaunchPoint()->getAppDesc()->getFolderPath().c_str();
        forkParams[7] = path.c_str();
        forkParams[8] = strParams.c_str();
        forkParams[9] = NULL;

        Logger::info(getClassName(), __FUNCTION__, runningApp.getAppId(), "launch with jail");
    } else {
        // This log shows whether native app's launched via Jailer or not
        // Do not remove this log, until jailer become stable
        forkParams[0] = path.c_str();
        forkParams[1] = strParams.c_str();
        forkParams[2] = NULL;

        Logger::info(getClassName(), __FUNCTION__, runningApp.getAppId(), "launch with root");
    }

    // TODO Native App Preloading is not supported yet
    runningApp.setLifeStatus(LifeStatus::LifeStatus_LAUNCHING);

    int pid = LinuxProcess::forkProcess(forkParams, NULL);
    if (pid <= 0) {
        Logger::error(getClassName(), __FUNCTION__, runningApp.getAppId(), Logger::format("pid(%d) path(%s)", pid, path.c_str()));
        runningApp.setLifeStatus(LifeStatus::LifeStatus_STOP);
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Failed to launch process");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    // set watcher for the child's
    g_child_watch_add(pid, (GChildWatchFunc) NativeContainer::onKillChildProcess, NULL);
    Logger::info(getClassName(), __FUNCTION__, appId, Logger::format("Launch Time: %f seconds", lunaTask->getTimeStamp()));

    string pidStr = boost::lexical_cast<string>(pid);
    runningApp.setProcessId(pidStr);
    runningApp.setWebprocid("");

    lunaTask->getResponsePayload().put("returnValue", true);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void NativeContainer::launchFromRegistered(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    runningApp.setLifeStatus(LifeStatus::LifeStatus_LAUNCHING);
    Logger::info(getClassName(), __FUNCTION__, runningApp.getInstanceId());
    pbnjson::JValue payload = runningApp.getRelaunchParams(lunaTask);
    if (!runningApp.sendEvent(payload)) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Failed to send relaunch event");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    lunaTask->getResponsePayload().put("returnValue", true);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}
