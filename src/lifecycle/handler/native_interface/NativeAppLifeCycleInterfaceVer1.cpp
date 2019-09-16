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

#include <lifecycle/handler/native_interface/NativeAppLifeCycleInterfaceVer1.h>
#include <lifecycle/handler/NativeAppLifeHandler.h>
#include "util/LinuxProcess.h"
#include "util/Time.h"

NativeAppLifeCycleInterfaceVer1::NativeAppLifeCycleInterfaceVer1()
{
}

NativeAppLifeCycleInterfaceVer1::~NativeAppLifeCycleInterfaceVer1()
{
}

void NativeAppLifeCycleInterfaceVer1::close(NativeClientInfoPtr client, CloseAppItemPtr item, std::string& errorText)
{
    Logger::info(getClassName(), __FUNCTION__, client->getAppId());

    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(client->getAppId());
    if (RuntimeStatus::CLOSING == runningInfoPtr->getRuntimeStatus()) {
        Logger::info(getClassName(), __FUNCTION__, client->getAppId(), "already being closed. wait_closing");
        return;
    }

    if (client->getPid().empty()) {
        errorText = "empty pid";
        Logger::error(getClassName(), __FUNCTION__, client->getAppId(), errorText);
        NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(client->getAppId(), "", RuntimeStatus::STOP);
        return;
    }

    PidVector all_pids = LinuxProcess::findChildPids(client->getPid());

    if (!NativeAppLifeHandler::getInstance().sendSystemSignal(all_pids, SIGTERM)) {
        errorText = "not found any pids to kill";
        Logger::error(getClassName(), __FUNCTION__, client->getAppId(), errorText);
        NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(client->getAppId(), "", RuntimeStatus::STOP);
        return;
    }

    NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(client->getAppId(), "", RuntimeStatus::CLOSING);

    NativeAppLifeHandler::getInstance().startTimerToKillApp(client->getAppId(), client->getPid(), all_pids, TIMEOUT_FOR_FORCE_KILL);

    Logger::info(getClassName(), __FUNCTION__, client->getAppId(), "sigterm");
}

void NativeAppLifeCycleInterfaceVer1::pause(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& errorText, bool sendLifeEvent)
{
    Logger::info(getClassName(), __FUNCTION__, client->getAppId(), "sigterm_to_pause");

    PidVector all_pids = LinuxProcess::findChildPids(client->getPid());
    (void) NativeAppLifeHandler::getInstance().sendSystemSignal(all_pids, SIGTERM);
    if (sendLifeEvent)
        NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(client->getAppId(), "", RuntimeStatus::CLOSING);
    NativeAppLifeHandler::getInstance().startTimerToKillApp(client->getAppId(), client->getPid(), all_pids, TIMEOUT_FOR_FORCE_KILL);
}

void NativeAppLifeCycleInterfaceVer1::launchFromClosing(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(item->getAppId());
    Logger::info(getClassName(), __FUNCTION__, client->getAppId(), "wait_until_being_closed");
    NativeAppLifeHandler::getInstance().addLaunchingItemIntoPendingQ(item);
}

void NativeAppLifeCycleInterfaceVer1::launchFromRunning(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(item->getAppId());

    Logger::info(getClassName(), __FUNCTION__, client->getAppId(), "in close_and_launch");

    // if less than 3 sec, just skip
    double now = Time::getCurrentTime();
    if ((now - runningInfoPtr->getLastLaunchTime()) < TIME_LIMIT_OF_APP_LAUNCHING) {
        if (runningInfoPtr->getPreloadMode() && item->getPreload().empty()) {
            // should close and launch if currently being launched in hidden mode
        } else {
            Logger::info(getClassName(), __FUNCTION__, item->getAppId(), "skip_by_launching_time");
            item->setPid(runningInfoPtr->getPid());
            NativeAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());
            return;
        }
    }

    Logger::info(getClassName(), __FUNCTION__, item->getAppId(), "close_and_launch");

    PidVector all_pids = LinuxProcess::findChildPids(client->getPid());
    NativeAppLifeHandler::getInstance().sendSystemSignal(all_pids, SIGTERM);
    NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::CLOSING);
    NativeAppLifeHandler::getInstance().startTimerToKillApp(client->getAppId(), client->getPid(), all_pids, TIMEOUT_FOR_FORCE_KILL);
    NativeAppLifeHandler::getInstance().addLaunchingItemIntoPendingQ(item);
}

bool NativeAppLifeCycleInterfaceVer1::canLaunch(LaunchAppItemPtr item)
{
    if (NativeAppLifeHandler::getInstance().getLaunchPendingItem(item->getAppId()) != NULL) {
        Logger::error(getClassName(), __FUNCTION__, item->getAppId(), "already_in_queue");
        item->setErrCodeText(APP_ERR_NATIVE_IS_LAUNCHING, item->getAppId() + std::string(" is already launching"));
        return false;
    }
    return true;
}

bool NativeAppLifeCycleInterfaceVer1::getLaunchParams(LaunchAppItemPtr item, AppPackagePtr appDescPtr, std::string& params)
{
    pbnjson::JValue payload = pbnjson::Object();

    if (AppType::AppType_Native_Qml == appDescPtr->getAppType()) {
        payload.put("main", appDescPtr->getMain());
        payload.put(Logger::LOG_KEY_APPID, appDescPtr->getAppId());
        payload.put("params", item->getParams());
    } else {
        payload = item->getParams().duplicate();
        payload.put("nid", item->getAppId());
        payload.put("@system_native_app", true);

        if (!item->getPreload().empty())
            payload.put("preload", item->getPreload());
    }

    if (!item->getDisplay().empty())
        payload.put("display", item->getDisplay());

    params = payload.stringify();
    return true;
}

bool NativeAppLifeCycleInterfaceVer1::getRelaunchParams(LaunchAppItemPtr item, AppPackagePtr appDescPtr, pbnjson::JValue& params)
{
    params.put("message", "relaunch");
    params.put("parameters", item->getParams());
    params.put("returnValue", true);
    return true;
}
