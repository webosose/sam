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
    addLaunchHandler(RuntimeStatus::STOP, boost::bind(&INativeApp::launchAsCommon, this, _1, _2));

    addLaunchHandler(RuntimeStatus::RUNNING, boost::bind(&NativeAppLifeCycleInterfaceVer1::launchNotRegisteredAppAsPolicy, this, _1, _2));

    addLaunchHandler(RuntimeStatus::REGISTERED, boost::bind(&INativeApp::relaunchAsCommon, this, _1, _2));

    addLaunchHandler(RuntimeStatus::CLOSING, boost::bind(&NativeAppLifeCycleInterfaceVer1::launchAfterClosedAsPolicy, this, _1, _2));
}

std::string NativeAppLifeCycleInterfaceVer1::makeForkArguments(LaunchAppItemPtr item, AppDescPtr app_desc)
{
    pbnjson::JValue payload = pbnjson::Object();

    if (AppType::AppType_Native_Qml == app_desc->getAppType()) {
        payload.put("main", app_desc->getEntryPoint());
        payload.put("appId", app_desc->getAppId());
        payload.put("params", item->getParams());
    } else {
        payload = item->getParams().duplicate();
        payload.put("nid", item->getAppId());
        payload.put("@system_native_app", true);
        if (!item->getPreload().empty())
            payload.put("preload", item->getPreload());
    }

    return payload.stringify();
}

bool NativeAppLifeCycleInterfaceVer1::checkLaunchCondition(LaunchAppItemPtr item)
{
    if (NativeAppLifeHandler::getInstance().getLaunchPendingItem(item->getAppId()) != NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", item->getAppId().c_str()),
                  PMLOGKS("reason", "already_in_queue"),
                  PMLOGKS("where", "launch_nativeapp"), "");
        item->setErrCodeText(APP_ERR_NATIVE_IS_LAUNCHING, item->getAppId() + std::string(" is already launching"));
        return false;
    }
    return true;
}

void NativeAppLifeCycleInterfaceVer1::makeRelaunchParams(LaunchAppItemPtr item, pbnjson::JValue& payload)
{
    payload.put("message", "relaunch");
    payload.put("parameters", item->getParams());
    payload.put("returnValue", true);
}

void NativeAppLifeCycleInterfaceVer1::launchAfterClosedAsPolicy(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", item->getAppId().c_str()),
             PMLOGKS("status", "app_is_closing"),
             PMLOGKS("action", "wait_until_being_closed"),
             "life_status: %d", (int )AppInfoManager::instance().runtimeStatus(item->getAppId()));

    NativeAppLifeHandler::getInstance().addLaunchingItemIntoPendingQ(item);
}

void NativeAppLifeCycleInterfaceVer1::launchNotRegisteredAppAsPolicy(NativeClientInfoPtr client, LaunchAppItemPtr item)
{

    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    double last_launch_time = AppInfoManager::instance().lastLaunchTime(item->getAppId());
    double current_time = getCurrentTime();
    bool preload_mode_on = AppInfoManager::instance().preloadModeOn(item->getAppId());

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", item->getAppId().c_str()),
             PMLOGKS("preload_mode_on", (preload_mode_on ? "on":"off")),
             PMLOGKS("preload_mode", (item->getPreload().empty() ? "empty":item->getPreload().c_str())),
             "in close_and_launch");

    // if less than 3 sec, just skip
    if ((current_time - last_launch_time) < TIME_LIMIT_OF_APP_LAUNCHING) {
        if (preload_mode_on && item->getPreload().empty()) {
            // should close and launch if currently being launched in hidden mode
        } else {
            LOG_INFO(MSGID_APPLAUNCH, 3,
                     PMLOGKS("app_id", item->getAppId().c_str()),
                     PMLOGKS("status", "running"),
                     PMLOGKS("action", "skip_by_launching_time"),
                     "life_cycle: %d", (int )AppInfoManager::instance().runtimeStatus(item->getAppId()));
            item->setPid(AppInfoManager::instance().getPid(item->getAppId()));
            NativeAppLifeHandler::getInstance().signal_launching_done(item->getUid());
            return;
        }
    }

    std::string pid = AppInfoManager::instance().getPid(item->getAppId());
    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", item->getAppId().c_str()),
             PMLOGKS("status", "running"),
             PMLOGKS("action", "close_and_launch"),
             "life_cycle: %d, pid: %s", (int )AppInfoManager::instance().runtimeStatus(item->getAppId()), pid.c_str());

    PidVector all_pids = LinuxProcess::FindChildPids(client->getPid());
    (void) NativeAppLifeHandler::getInstance().sendSystemSignal(all_pids, SIGTERM);
    NativeAppLifeHandler::getInstance().signal_app_life_status_changed(item->getAppId(), item->getUid(), RuntimeStatus::CLOSING);
    NativeAppLifeHandler::getInstance().startTimerToKillApp(client->getAppId(), client->getPid(), all_pids, TIMEOUT_FOR_FORCE_KILL);

    NativeAppLifeHandler::getInstance().addLaunchingItemIntoPendingQ(item);
}

void NativeAppLifeCycleInterfaceVer1::closeAsPolicy(NativeClientInfoPtr client, CloseAppItemPtr item, std::string& err_text)
{

    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    RuntimeStatus current_status = AppInfoManager::instance().runtimeStatus(client->getAppId());
    if (RuntimeStatus::CLOSING == current_status) {
        LOG_INFO(MSGID_APPCLOSE, 3,
                 PMLOGKS("app_id", client->getAppId().c_str()),
                 PMLOGKS("pid", client->getPid().c_str()),
                 PMLOGKS("action", "wait_closing"), "already being closed");
        return;
    }

    if (client->getPid().empty()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2, PMLOGKS("reason", "empty_pid"), PMLOGKS("where", __FUNCTION__), "");
        err_text = "empty pid";
        NativeAppLifeHandler::getInstance().signal_app_life_status_changed(client->getAppId(), "", RuntimeStatus::STOP);
        return;
    }

    PidVector all_pids = LinuxProcess::FindChildPids(client->getPid());
    if (!NativeAppLifeHandler::getInstance().sendSystemSignal(all_pids, SIGTERM)) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS("reason", "empty_pids"),
                  PMLOGKS("where", __FUNCTION__), "");
        err_text = "not found any pids to kill";
        NativeAppLifeHandler::getInstance().signal_app_life_status_changed(client->getAppId(), "", RuntimeStatus::STOP);
        return;
    }

    NativeAppLifeHandler::getInstance().signal_app_life_status_changed(client->getAppId(), "", RuntimeStatus::CLOSING);

    NativeAppLifeHandler::getInstance().startTimerToKillApp(client->getAppId(), client->getPid(), all_pids, TIMEOUT_FOR_FORCE_KILL);

    LOG_INFO(MSGID_APPCLOSE, 3,
             PMLOGKS("app_id", client->getAppId().c_str()),
             PMLOGKS("pid", client->getPid().c_str()),
             PMLOGKS("action", "sigterm"), "");
}

void NativeAppLifeCycleInterfaceVer1::pauseAsPolicy(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& err_text, bool send_life_event)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    LOG_INFO(MSGID_APPCLOSE, 3,
             PMLOGKS("app_id", client->getAppId().c_str()),
             PMLOGKS("pid", client->getPid().c_str()),
             PMLOGKS("action", "sigterm_to_pause"), "");

    PidVector all_pids = LinuxProcess::FindChildPids(client->getPid());
    (void) NativeAppLifeHandler::getInstance().sendSystemSignal(all_pids, SIGTERM);
    if (send_life_event)
        NativeAppLifeHandler::getInstance().signal_app_life_status_changed(client->getAppId(), "", RuntimeStatus::CLOSING);
    NativeAppLifeHandler::getInstance().startTimerToKillApp(client->getAppId(), client->getPid(), all_pids, TIMEOUT_FOR_FORCE_KILL);
}
