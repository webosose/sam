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

#include <lifecycle/handler/native_interface/NativeAppLifeCycleInterfaceVer2.h>
#include <lifecycle/handler/NativeAppLifeHandler.h>
#include "util/LinuxProcess.h"

NativeAppLifeCycleInterfaceVer2::NativeAppLifeCycleInterfaceVer2()
{
}

NativeAppLifeCycleInterfaceVer2::~NativeAppLifeCycleInterfaceVer2()
{
}

void NativeAppLifeCycleInterfaceVer2::close(NativeClientInfoPtr client, CloseAppItemPtr item, std::string& errorText)
{
    Logger::info(getClassName(), __FUNCTION__, client->getAppId());
    NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(client->getAppId(), "", RuntimeStatus::CLOSING);

    // if registered
    if (client->isRegistered()) {
        pbnjson::JValue payload = pbnjson::Object();
        payload.put("event", "close");
        payload.put(Logger::LOG_KEY_REASON, item->getReason());
        payload.put("returnValue", true);

        if (client->sendEvent(payload) == false) {
            Logger::warning(getClassName(), __FUNCTION__, client->getAppId(), "failed_to_send_close_event");
        }

        PidVector all_pids = LinuxProcess::findChildPids(client->getPid());
        if (item->isMemoryReclaim()) {
            //start force kill timer()
            NativeAppLifeHandler::getInstance().startTimerToKillApp(client->getAppId(), client->getPid(), all_pids, TIMEOUT_FOR_FORCE_KILL);
        } else {
            NativeAppLifeHandler::getInstance().startTimerToKillApp(client->getAppId(), client->getPid(), all_pids, TIMEOUT_FOR_NOT_RESPONDING);
        }
        // if not registered
    } else {
        //  clean up pending queue
        NativeAppLifeHandler::getInstance().cancelLaunchPendingItemAndMakeItDone(client->getAppId());

        // send sigkill
        PidVector all_pids = LinuxProcess::findChildPids(client->getPid());
        NativeAppLifeHandler::getInstance().sendSystemSignal(all_pids, SIGKILL);
    }
}

void NativeAppLifeCycleInterfaceVer2::pause(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& errorText, bool sendLifeEvent)
{
    Logger::info(getClassName(), __FUNCTION__, client->getAppId());

    if (client->isRegistered()) {
        pbnjson::JValue payload = pbnjson::Object();
        payload.put("event", "pause");
        payload.put(Logger::LOG_KEY_REASON, "keepAlive");
        payload.put("parameters", params);
        payload.put("returnValue", true);

        if (sendLifeEvent)
            NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(client->getAppId(), "", RuntimeStatus::PAUSING);

        if (client->sendEvent(payload) == false) {
            Logger::warning(getClassName(), __FUNCTION__, client->getAppId(), "failed_to_send_pause_event");
        }
    } else {
        // clean up pending queue
        NativeAppLifeHandler::getInstance().cancelLaunchPendingItemAndMakeItDone(client->getAppId());

        if (sendLifeEvent)
            NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(client->getAppId(), "", RuntimeStatus::CLOSING);

        NativeAppLifeHandler::getInstance().findPidsAndSendSystemSignal(client->getPid(), SIGKILL);
    }
}

void NativeAppLifeCycleInterfaceVer2::launchFromClosing(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(item->getAppId());
    Logger::info(getClassName(), __FUNCTION__, item->getAppId(), "app_is_closing: wait_until_being_closed");
    NativeAppLifeHandler::getInstance().addLaunchingItemIntoPendingQ(item);
}

void NativeAppLifeCycleInterfaceVer2::launchFromRunning(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    Logger::info(getClassName(), __FUNCTION__, client->getAppId());

    if (client->isRegistrationExpired()) {
        // clean up previous launch reqeust from pending queue
        NativeAppLifeHandler::getInstance().cancelLaunchPendingItemAndMakeItDone(client->getAppId());

        NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::CLOSING);

        (void) NativeAppLifeHandler::getInstance().findPidsAndSendSystemSignal(client->getPid(), SIGKILL);
    }

    NativeAppLifeHandler::getInstance().addLaunchingItemIntoPendingQ(item);
}

bool NativeAppLifeCycleInterfaceVer2::canLaunch(LaunchAppItemPtr item)
{
    return true;
}

bool NativeAppLifeCycleInterfaceVer2::getLaunchParams(LaunchAppItemPtr item, AppPackagePtr appDescPtr, std::string& params)
{
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("event", "launch");
    payload.put(Logger::LOG_KEY_REASON, item->getReason());
    payload.put(Logger::LOG_KEY_APPID, item->getAppId());
    payload.put("interfaceVersion", 2);
    payload.put("interfaceMethod", "registerApp");
    payload.put("parameters", item->getParams());
    payload.put("@system_native_app", true);

    if (!item->getPreload().empty())
        payload.put("preload", item->getPreload());

    if (!item->getDisplay().empty())
        payload.put("display", item->getDisplay());

    if (AppType::AppType_Native_Qml == appDescPtr->getAppType())
        payload.put("main", appDescPtr->getMain());

    params = payload.stringify();
    return true;
}

bool NativeAppLifeCycleInterfaceVer2::getRelaunchParams(LaunchAppItemPtr item, AppPackagePtr appDescPtr, pbnjson::JValue& params)
{
    params.put("event", "relaunch");
    params.put(Logger::LOG_KEY_REASON, item->getReason());
    params.put(Logger::LOG_KEY_APPID, item->getAppId());
    params.put("parameters", item->getParams());
    params.put("returnValue", true);
    return true;
}
