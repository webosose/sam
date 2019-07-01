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

NativeAppLifeCycleInterfaceVer1::NativeAppLifeCycleInterfaceVer1()
{
}

NativeAppLifeCycleInterfaceVer1::~NativeAppLifeCycleInterfaceVer1()
{
}

void NativeAppLifeCycleInterfaceVer1::close(NativeClientInfoPtr client, CloseAppItemPtr item, std::string& errorText)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS(LOG_KEY_APPID, client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(client->getAppId());
    if (RuntimeStatus::CLOSING == runningInfoPtr->getRuntimeStatus()) {
        LOG_INFO(MSGID_APPCLOSE, 3,
                 PMLOGKS(LOG_KEY_APPID, client->getAppId().c_str()),
                 PMLOGKS("pid", client->getPid().c_str()),
                 PMLOGKS(LOG_KEY_ACTION, "wait_closing"),
                 "already being closed");
        return;
    }

    if (client->getPid().empty()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "empty_pid"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        errorText = "empty pid";
        NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(client->getAppId(), "", RuntimeStatus::STOP);
        return;
    }

    PidVector all_pids = LinuxProcess::FindChildPids(client->getPid());
    if (!NativeAppLifeHandler::getInstance().sendSystemSignal(all_pids, SIGTERM)) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "empty_pids"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        errorText = "not found any pids to kill";
        NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(client->getAppId(), "", RuntimeStatus::STOP);
        return;
    }

    NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(client->getAppId(), "", RuntimeStatus::CLOSING);

    NativeAppLifeHandler::getInstance().startTimerToKillApp(client->getAppId(), client->getPid(), all_pids, TIMEOUT_FOR_FORCE_KILL);

    LOG_INFO(MSGID_APPCLOSE, 3,
             PMLOGKS(LOG_KEY_APPID, client->getAppId().c_str()),
             PMLOGKS("pid", client->getPid().c_str()),
             PMLOGKS(LOG_KEY_ACTION, "sigterm"), "");
}

void NativeAppLifeCycleInterfaceVer1::pause(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& err_text, bool sendLifeEvent)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS(LOG_KEY_APPID, client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    LOG_INFO(MSGID_APPCLOSE, 3,
             PMLOGKS(LOG_KEY_APPID, client->getAppId().c_str()),
             PMLOGKS("pid", client->getPid().c_str()),
             PMLOGKS(LOG_KEY_ACTION, "sigterm_to_pause"), "");

    PidVector all_pids = LinuxProcess::FindChildPids(client->getPid());
    (void) NativeAppLifeHandler::getInstance().sendSystemSignal(all_pids, SIGTERM);
    if (sendLifeEvent)
        NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(client->getAppId(), "", RuntimeStatus::CLOSING);
    NativeAppLifeHandler::getInstance().startTimerToKillApp(client->getAppId(), client->getPid(), all_pids, TIMEOUT_FOR_FORCE_KILL);
}

void NativeAppLifeCycleInterfaceVer1::launchFromClosing(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS(LOG_KEY_APPID, client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(item->getAppId());
    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
             PMLOGKS("status", "app_is_closing"),
             PMLOGKS(LOG_KEY_ACTION, "wait_until_being_closed"),
             "life_status: %d", runningInfoPtr->getRuntimeStatus());

    NativeAppLifeHandler::getInstance().addLaunchingItemIntoPendingQ(item);
}

void NativeAppLifeCycleInterfaceVer1::launchFromRunning(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS(LOG_KEY_APPID, client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(item->getAppId());
    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
             PMLOGKS("preload_mode_on", (runningInfoPtr->getPreloadMode() ? "on":"off")),
             PMLOGKS("preload_mode", (item->getPreload().empty() ? "empty":item->getPreload().c_str())),
             "in close_and_launch");

    // if less than 3 sec, just skip
    double now = getCurrentTime();
    if ((now - runningInfoPtr->getLastLaunchTime()) < TIME_LIMIT_OF_APP_LAUNCHING) {
        if (runningInfoPtr->getPreloadMode() && item->getPreload().empty()) {
            // should close and launch if currently being launched in hidden mode
        } else {
            LOG_INFO(MSGID_APPLAUNCH, 3,
                     PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                     PMLOGKS("status", "running"),
                     PMLOGKS(LOG_KEY_ACTION, "skip_by_launching_time"),
                     "life_cycle: %d", runningInfoPtr->getRuntimeStatus());
            item->setPid(runningInfoPtr->getPid());
            NativeAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());
            return;
        }
    }

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
             PMLOGKS("status", "running"),
             PMLOGKS(LOG_KEY_ACTION, "close_and_launch"),
             "life_cycle: %d, pid: %s", runningInfoPtr->getRuntimeStatus(), runningInfoPtr->getPid().c_str());

    PidVector all_pids = LinuxProcess::FindChildPids(client->getPid());
    NativeAppLifeHandler::getInstance().sendSystemSignal(all_pids, SIGTERM);
    NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::CLOSING);
    NativeAppLifeHandler::getInstance().startTimerToKillApp(client->getAppId(), client->getPid(), all_pids, TIMEOUT_FOR_FORCE_KILL);
    NativeAppLifeHandler::getInstance().addLaunchingItemIntoPendingQ(item);
}

bool NativeAppLifeCycleInterfaceVer1::canLaunch(LaunchAppItemPtr item)
{
    if (NativeAppLifeHandler::getInstance().getLaunchPendingItem(item->getAppId()) != NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                  PMLOGKS(LOG_KEY_REASON, "already_in_queue"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
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
        payload.put(LOG_KEY_APPID, appDescPtr->getAppId());
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
