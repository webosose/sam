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

#include <ext/stdio_filebuf.h>
#include <boost/lexical_cast.hpp>

#include <base/AppDescription.h>
#include <bus/client/SAM.h>
#include <conf/SAMConf.h>
#include "base/LunaTaskList.h"
#include "base/AppDescriptionList.h"
#include "base/RunningAppList.h"

SAM::SAM()
{
}

SAM::~SAM()
{
}

void SAM::launch(LunaTaskPtr lunaTask)
{
    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(lunaTask->getAppId(), true);
    if (runningApp == nullptr) {
        Logger::error(getClassName(), __FUNCTION__, lunaTask->getAppId(), "no_client");
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "no_client");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    switch (runningApp->getLifeStatus()) {
    case LifeStatus::LifeStatus_STOP:
        launchFromStop(lunaTask);
        break;

    case LifeStatus::LifeStatus_RUNNING:
        if (!runningApp->isRegistered())
            launchFromRunning(lunaTask);
        else
            launchFromRegistered(lunaTask);
        break;

    case LifeStatus::LifeStatus_CLOSING:
        // 강제로 죽이고 실행해야 하는거...
        // 멀티플 인스턴스가 나오면...그냥 실행으로 가야한다...
        launchFromClosing(lunaTask);
        break;

    default:
        string errorText = "internal error";
        Logger::error(getClassName(), __FUNCTION__, lunaTask->getAppId(), errorText);
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, errorText);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
}

void SAM::close(LunaTaskPtr lunaTask)
{
    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(lunaTask->getAppId());
    if (runningApp == nullptr) {
        Logger::error(getClassName(), __FUNCTION__, lunaTask->getAppId(), "no_client");
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "native app is not running");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    Logger::info(getClassName(), __FUNCTION__, runningApp->getInstanceId());
    runningApp->close(lunaTask);

    if (LifeStatus::LifeStatus_STOP == runningApp->getLifeStatus()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "native app is not running");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    runningApp->close(lunaTask);
}

void SAM::onKillChildProcess(GPid pid, gint status, gpointer data)
{
    g_spawn_close_pid(pid);
    string pidStr = boost::lexical_cast<std::string>(pid);

    Logger::info(getInstance().getClassName(), __FUNCTION__, "closed_pid");
    RunningAppPtr runningApp = RunningAppList::getInstance().getByPid(pidStr);
    if (runningApp == nullptr) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "empty_client_info");
        return;
    }
    if (LifeStatus::LifeStatus_CLOSING == runningApp->getLifeStatus()) {
        Logger::info(getInstance().getClassName(), __FUNCTION__, "received event closed by sam");
    } else {
        Logger::info(getInstance().getClassName(), __FUNCTION__, "received event closed by itself");
    }

    string appId = runningApp->getLaunchPoint()->getAppDesc()->getAppId();
    Logger::info(getInstance().getClassName(), __FUNCTION__, appId, Logger::format("exit_status(%d)", status));
    RunningAppList::getInstance().remove(runningApp);
    // RunningAppList::getInstance().removeByAppId(appId);
    runningApp->setLifeStatus(LifeStatus::LifeStatus_STOP);
}

void SAM::launchFromStop(LunaTaskPtr lunaTask)
{
    if (lunaTask == NULL) {
        Logger::error(getClassName(), __FUNCTION__, lunaTask->getAppId(), "null_poiner");
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "internal error");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    Logger::info(getClassName(), __FUNCTION__, lunaTask->getAppId(), "start_native_handler");
    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(lunaTask->getAppId());
    if (appDesc == NULL) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "null_description");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    // construct the path for launching
    std::string path = appDesc->getAbsMain();
    if (path.find("file://", 0) != std::string::npos)
        path = path.substr(7);

    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(lunaTask->getAppId());
    std::string strParams = runningApp->getLaunchParams(lunaTask);

    const char* forkParams[10] = { 0, };
    const char* jailer_type = "";
    bool isNojailApp = SAMConf::getInstance().isNoJailApp(appDesc->getAppId());

    if (AppType::AppType_Native_AppShell == appDesc->getAppType()) {
        forkParams[0] = SAMConf::getInstance().getAppShellRunnerPath().c_str();
        forkParams[1] = "--appid";
        forkParams[2] = appDesc->getAppId().c_str();
        forkParams[3] = "--folder";
        forkParams[4] = appDesc->getFolderPath().c_str();
        forkParams[5] = "--params";
        forkParams[6] = strParams.c_str();
        forkParams[7] = NULL;

        Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId(), "launch with appshell_runner");
    } else if (AppType::AppType_Native_Qml == appDesc->getAppType()) {
        forkParams[0] = SAMConf::getInstance().getQmlRunnerPath().c_str();
        forkParams[1] = strParams.c_str();
        forkParams[2] = NULL;

        Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId(), "launch with qml_runner");
    } else if (SAMConf::getInstance().isJailerDisabled() && !isNojailApp) {
        if (AppLocation::AppLocation_Devmode == appDesc->getAppLocation()) {
            jailer_type = "native_devmode";
        } else {
            switch (appDesc->getAppType()) {
            case AppType::AppType_Native: {
                jailer_type = "native";
                break;
            }
            case AppType::AppType_Native_Builtin: {
                jailer_type = "native_builtin";
                break;
            }
            case AppType::AppType_Native_Mvpd: {
                jailer_type = "native_mvpd";
                break;
            }
            default:
                jailer_type = "default";
                break;
            }
        }

        forkParams[0] = SAMConf::getInstance().getJailerPath().c_str();
        forkParams[1] = "-t";
        forkParams[2] = jailer_type;
        forkParams[3] = "-i";
        forkParams[4] = appDesc->getAppId().c_str();
        forkParams[5] = "-p";
        forkParams[6] = appDesc->getFolderPath().c_str();
        forkParams[7] = path.c_str();
        forkParams[8] = strParams.c_str();
        forkParams[9] = NULL;

        Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId(), "launch with jail");
    } else {
        // This log shows whether native app's launched via Jailer or not
        // Do not remove this log, until jailer become stable
        forkParams[0] = path.c_str();
        forkParams[1] = strParams.c_str();
        forkParams[2] = NULL;

        Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId(), "launch with root");
    }

    if (lunaTask->getPreload().empty())
        runningApp->setLifeStatus(LifeStatus::LifeStatus_LAUNCHING);
    else
        runningApp->setLifeStatus(LifeStatus::LifeStatus_PRELOADING);

    int pid = LinuxProcess::forkProcess(forkParams, NULL);

    if (pid <= 0) {
        Logger::error(getClassName(), __FUNCTION__, appDesc->getAppId(), Logger::format("pid(%d) path(%s)", pid, path.c_str()));
        runningApp->setLifeStatus(LifeStatus::LifeStatus_STOP);
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Failed to launch process");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    // set watcher for the child's
    g_child_watch_add(pid, (GChildWatchFunc) SAM::onKillChildProcess, NULL);

    lunaTask->stopTime();
    Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId(), Logger::format("pid(%d) collapse(%f)", pid, lunaTask->getTotalTime()));

    std::string new_pid = boost::lexical_cast<std::string>(pid);
    lunaTask->setPid(new_pid);
    runningApp->setPid(new_pid);
    runningApp->setWebprocid("");

    RunningAppPtr runningInfo = RunningAppList::getInstance().getByAppId(lunaTask->getAppId());
    if (runningInfo)
        runningInfo->saveLastLaunchTime();

    runningApp->setLifeStatus(LifeStatus::LifeStatus_RUNNING);

    LunaTaskList::getInstance().removeAfterReply(lunaTask);
    //NativeAppLifeHandler::getInstance().EventLaunchingDone(lunaTask->getInstanceId());
}

void SAM::launchFromRegistered(LunaTaskPtr lunaTask)
{
    Logger::info(getClassName(), __FUNCTION__, lunaTask->getAppId());

    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(lunaTask->getAppId());
    pbnjson::JValue payload = runningApp->getRelaunchParams(lunaTask);
    Logger::info(getClassName(), __FUNCTION__, runningApp->getInstanceId());
    runningApp->setLifeStatus(LifeStatus::LifeStatus_LAUNCHING);

    if (runningApp->sendEvent(payload) == false) {
        Logger::warning(getClassName(), __FUNCTION__, runningApp->getInstanceId(), "failed_to_send_relaunch_event");
    }

    lunaTask->setErrCodeAndText(ErrCode_APP_LOCKED, "app is locked");
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
    //NativeAppLifeHandler::getInstance().EventLaunchingDone(item->getInstanceId());
}

void SAM::launchFromClosing(LunaTaskPtr lunaTask)
{
    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(lunaTask->getAppId());
    Logger::info(getClassName(), __FUNCTION__, runningApp->getInstanceId(), "wait_until_being_closed");
}

void SAM::launchFromRunning(LunaTaskPtr item)
{
    Logger::info(getClassName(), __FUNCTION__, item->getAppId());
    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(item->getAppId());

    if (!runningApp->isRegistered()) {
        // clean up previous launch reqeust from pending queue
        runningApp->setLifeStatus(LifeStatus::LifeStatus_CLOSING);
        LinuxProcess::sendSigKill(runningApp->getPid());
    }
}
