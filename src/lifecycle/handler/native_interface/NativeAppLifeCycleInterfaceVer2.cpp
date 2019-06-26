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
#include "util/LinuxProcess.h"
#include <lifecycle/handler/NativeAppLifeHandler.h>

NativeAppLifeCycleInterfaceVer2::NativeAppLifeCycleInterfaceVer2()
{
    addLaunchHandler(RuntimeStatus::STOP, boost::bind(&INativeApp::launchAsCommon, this, _1, _2));

    addLaunchHandler(RuntimeStatus::RUNNING, boost::bind(&NativeAppLifeCycleInterfaceVer2::launchNotRegisteredAppAsPolicy, this, _1, _2));

    addLaunchHandler(RuntimeStatus::REGISTERED, boost::bind(&INativeApp::relaunchAsCommon, this, _1, _2));

    addLaunchHandler(RuntimeStatus::CLOSING, boost::bind(&NativeAppLifeCycleInterfaceVer2::launchAfterClosedAsPolicy, this, _1, _2));
}

std::string NativeAppLifeCycleInterfaceVer2::makeForkArguments(LaunchAppItemPtr item, AppDescPtr app_desc)
{
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("event", "launch");
    payload.put("reason", item->getReason());
    payload.put("appId", item->getAppId());
    payload.put("interfaceVersion", 2);
    payload.put("interfaceMethod", "registerApp");
    payload.put("parameters", item->getParams());
    payload.put("@system_native_app", true);

    if (!item->getPreload().empty())
        payload.put("preload", item->getPreload());

    if (AppType::AppType_Native_Qml == app_desc->getAppType())
        payload.put("main", app_desc->getEntryPoint());

    return payload.stringify();
}

bool NativeAppLifeCycleInterfaceVer2::checkLaunchCondition(LaunchAppItemPtr item)
{
    return true;
}

void NativeAppLifeCycleInterfaceVer2::makeRelaunchParams(LaunchAppItemPtr item, pbnjson::JValue& payload)
{
    payload.put("event", "relaunch");
    payload.put("reason", item->getReason());
    payload.put("appId", item->getAppId());
    payload.put("parameters", item->getParams());
    payload.put("returnValue", true);
}

void NativeAppLifeCycleInterfaceVer2::launchAfterClosedAsPolicy(NativeClientInfoPtr client, LaunchAppItemPtr item)
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

void NativeAppLifeCycleInterfaceVer2::launchNotRegisteredAppAsPolicy(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    if (client->isRegistrationExpired()) {
        // clean up previous launch reqeust from pending queue
        NativeAppLifeHandler::getInstance().cancelLaunchPendingItemAndMakeItDone(client->getAppId());

        NativeAppLifeHandler::getInstance().signal_app_life_status_changed(item->getAppId(), item->getUid(), RuntimeStatus::CLOSING);

        (void) NativeAppLifeHandler::getInstance().findPidsAndSendSystemSignal(client->getPid(), SIGKILL);
    }

    NativeAppLifeHandler::getInstance().addLaunchingItemIntoPendingQ(item);
}

void NativeAppLifeCycleInterfaceVer2::closeAsPolicy(NativeClientInfoPtr client, CloseAppItemPtr item, std::string& err_text)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    NativeAppLifeHandler::getInstance().signal_app_life_status_changed(client->getAppId(), "", RuntimeStatus::CLOSING);

    // if registered
    if (client->isRegistered()) {
        pbnjson::JValue payload = pbnjson::Object();
        payload.put("event", "close");
        payload.put("reason", item->getReason());
        payload.put("returnValue", true);

        if (client->sendEvent(payload) == false) {
            LOG_WARNING(MSGID_APPCLOSE_ERR, 1,
                        PMLOGKS("app_id", client->getAppId().c_str()),
                        "failed_to_send_close_event");
        }

        PidVector all_pids = LinuxProcess::FindChildPids(client->getPid());
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
        PidVector all_pids = LinuxProcess::FindChildPids(client->getPid());
        (void) NativeAppLifeHandler::getInstance().sendSystemSignal(all_pids, SIGKILL);
    }
}

void NativeAppLifeCycleInterfaceVer2::pauseAsPolicy(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& err_text, bool send_life_event)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    if (client->isRegistered()) {
        pbnjson::JValue payload = pbnjson::Object();
        payload.put("event", "pause");
        payload.put("reason", "keepAlive");
        payload.put("parameters", params);
        payload.put("returnValue", true);

        if (send_life_event)
            NativeAppLifeHandler::getInstance().signal_app_life_status_changed(client->getAppId(), "", RuntimeStatus::PAUSING);

        if (client->sendEvent(payload) == false) {
            LOG_WARNING(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", client->getAppId().c_str()), "failed_to_send_pause_event");
        }
    } else {
        // clean up pending queue
        NativeAppLifeHandler::getInstance().cancelLaunchPendingItemAndMakeItDone(client->getAppId());

        if (send_life_event)
            NativeAppLifeHandler::getInstance().signal_app_life_status_changed(client->getAppId(), "", RuntimeStatus::CLOSING);

        (void) NativeAppLifeHandler::getInstance().findPidsAndSendSystemSignal(client->getPid(), SIGKILL);
    }
}
