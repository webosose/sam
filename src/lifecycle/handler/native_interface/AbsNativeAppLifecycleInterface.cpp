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

#include "../NativeAppLifeHandler.h"
#include "setting/Settings.h"
#include <boost/lexical_cast.hpp>
#include <lifecycle/handler/native_interface/AbsNativeAppLifecycleInterface.h>
#include "util/LinuxProcess.h"
#include "util/Time.h"

AbsNativeAppLifecycleInterface::AbsNativeAppLifecycleInterface()
{
}

AbsNativeAppLifecycleInterface::~AbsNativeAppLifecycleInterface()
{
}

void AbsNativeAppLifecycleInterface::launch(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    // Check launch condition
    if (canLaunch(item) == false) {
        NativeAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());
        return;
    }

    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(item->getAppId(), item->getDisplay());
    if (runningInfoPtr == nullptr) {
        runningInfoPtr = RunningInfoManager::getInstance().addRunningInfo(item->getAppId(), item->getDisplay());
    }
    switch (runningInfoPtr->getRuntimeStatus()) {
    case RuntimeStatus::STOP:
        launchFromStop(client, item);
        break;

    case RuntimeStatus::RUNNING:
        launchFromRunning(client, item);
        break;

    case RuntimeStatus::REGISTERED:
        launchFromRegistered(client, item);
        break;

    case RuntimeStatus::CLOSING:
        launchFromClosing(client, item);
        break;

    default:
        string errorText = "internal error";
        Logger::error(getClassName(), __FUNCTION__, item->getAppId(), errorText);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, errorText);
        return;
    }
}

void AbsNativeAppLifecycleInterface::launchFromStop(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    if (item == NULL) {
        Logger::error(getClassName(), __FUNCTION__, item->getAppId(), "null_poiner");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        return;
    }

    Logger::info(getClassName(), __FUNCTION__, item->getAppId(), "start_native_handler");
    AppPackagePtr appDescPtr = AppPackageManager::getInstance().getAppById(item->getAppId());
    if (appDescPtr == NULL) {
        Logger::error(getClassName(), __FUNCTION__, item->getAppId(), "null_description");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        NativeAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());
        return;
    }

    // construct the path for launching
    std::string path = appDescPtr->getMain();
    if (path.find("file://", 0) != std::string::npos)
        path = path.substr(7);

    // [ Template method ]
    // Handle Differently based on Ver1/Ver2 policy
    std::string strParams;
    getLaunchParams(item, appDescPtr, strParams);

    const char* fork_params[10] = { 0, };
    const char* jailer_type = "";
    bool isNojailApp = SettingsImpl::getInstance().isInNoJailApps(appDescPtr->getAppId());

    if (AppType::AppType_Native_AppShell == appDescPtr->getAppType()) {
        fork_params[0] = SettingsImpl::getInstance().m_appshellRunnerPath.c_str();
        fork_params[1] = "--appid";
        fork_params[2] = appDescPtr->getAppId().c_str();
        fork_params[3] = "--folder";
        fork_params[4] = appDescPtr->getFolderPath().c_str();
        fork_params[5] = "--params";
        fork_params[6] = strParams.c_str();
        fork_params[7] = NULL;

        Logger::info(getClassName(), __FUNCTION__, appDescPtr->getAppId(), "launch with appshell_runner");
    } else if (AppType::AppType_Native_Qml == appDescPtr->getAppType()) {
        fork_params[0] = SettingsImpl::getInstance().m_qmlRunnerPath.c_str();
        fork_params[1] = strParams.c_str();
        fork_params[2] = NULL;

        Logger::info(getClassName(), __FUNCTION__, appDescPtr->getAppId(), "launch with qml_runner");
    } else if (SettingsImpl::getInstance().m_isJailMode && !isNojailApp) {
        if (AppTypeByDir::AppTypeByDir_Dev == appDescPtr->getTypeByDir()) {
            jailer_type = "native_devmode";
        } else {
            switch (appDescPtr->getAppType()) {
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

        fork_params[0] = SettingsImpl::getInstance().m_jailerPath.c_str();
        fork_params[1] = "-t";
        fork_params[2] = jailer_type;
        fork_params[3] = "-i";
        fork_params[4] = appDescPtr->getAppId().c_str();
        fork_params[5] = "-p";
        fork_params[6] = appDescPtr->getFolderPath().c_str();
        fork_params[7] = path.c_str();
        fork_params[8] = strParams.c_str();
        fork_params[9] = NULL;

        Logger::info(getClassName(), __FUNCTION__, appDescPtr->getAppId(), "launch with jail");
    } else {
        // This log shows whether native app's launched via Jailer or not
        // Do not remove this log, until jailer become stable
        fork_params[0] = path.c_str();
        fork_params[1] = strParams.c_str();
        fork_params[2] = NULL;

        Logger::info(getClassName(), __FUNCTION__, appDescPtr->getAppId(), "launch with root");
    }

    if (item->getPreload().empty())
        NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::LAUNCHING);
    else
        NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::PRELOADING);

    int pid = LinuxProcess::forkProcess(fork_params, NULL);

    if (pid <= 0) {
        Logger::error(getClassName(), __FUNCTION__, appDescPtr->getAppId(), Logger::format("pid(%d) path(%s)", pid, path.c_str()));
        NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::STOP);
        item->setErrCodeText(APP_ERR_NATIVE_SPAWN, "Failed to launch process");
        NativeAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());
        return;
    }

    // set watcher for the child's
    g_child_watch_add(pid, (GChildWatchFunc) NativeAppLifeHandler::onKillChildProcess, NULL);

    double now = Time::getCurrentTime();
    double collapse = now - item->launchStartTime();

    Logger::info(getClassName(), __FUNCTION__, appDescPtr->getAppId(), Logger::format("pid(%d) collapse(%f)", pid, collapse));

    std::string new_pid = boost::lexical_cast<std::string>(pid);
    item->setPid(new_pid);
    client->setPid(new_pid);
    client->startTimerForCheckingRegistration();

    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(item->getAppId(), item->getDisplay());
    if (runningInfoPtr)
        runningInfoPtr->setLastLaunchTime(now);

    NativeAppLifeHandler::getInstance().EventRunningAppAdded(item->getAppId(), new_pid, "");
    NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::RUNNING);
    NativeAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());
}

void AbsNativeAppLifecycleInterface::launchFromRegistered(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    Logger::info(getClassName(), __FUNCTION__, client->getAppId());

    pbnjson::JValue payload = pbnjson::Object();
    AppPackagePtr appDescPtr = AppPackageManager::getInstance().getAppById(item->getAppId());

    getRelaunchParams(item, appDescPtr, payload);

    NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::LAUNCHING);

    if (client->sendEvent(payload) == false) {
        Logger::warning(getClassName(), __FUNCTION__, client->getAppId(), "failed_to_send_relaunch_event");
    }

    NativeAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());
}
