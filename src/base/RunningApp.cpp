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

#include "RunningApp.h"

#include "bus/client/WAM.h"
#include "bus/client/Booster.h"
#include "util/LinuxProcess.h"

const string RunningApp::CLASS_NAME = "RunningApp";

const char* RunningApp::toString(LifeStatus status)
{
    switch (status) {
    case LifeStatus::INVALID:
        return "invalid";

    case LifeStatus::STOP:
        return "stop";

    case LifeStatus::PRELOADING:
        return "preloading";

    case LifeStatus::LAUNCHING:
        return "launching";

    case LifeStatus::RELAUNCHING:
        return "relaunching";

    case LifeStatus::FOREGROUND:
        return "foreground";

    case LifeStatus::BACKGROUND:
        return "background";

    case LifeStatus::CLOSING:
        return "closing";

    case LifeStatus::PAUSING:
        return "pausing";

    case LifeStatus::RUNNING:
        return "running";
    }
    return "unknown";
}

const char* RunningApp::toString(LifeEvent status)
{
    switch (status) {
    case LifeEvent::INVALID:
        return "invalid";

    case LifeEvent::SPLASH:
        return "splash";

    case LifeEvent::PRELOAD:
        return "preload";

    case LifeEvent::LAUNCH:
        return "launch";

    case LifeEvent::FOREGROUND:
        return "foreground";

    case LifeEvent::BACKGROUND:
        return "background";

    case LifeEvent::PAUSE:
        return "pause";

    case LifeEvent::CLOSE:
        return "close";

    case LifeEvent::STOP:
        return "stop";

    }
    return "unknown";
}

const char* RunningApp::toString(RuntimeStatus status)
{
    switch (status) {
    case RuntimeStatus::STOP:
        return "stop";

    case RuntimeStatus::LAUNCHING:
        return "launching";

    case RuntimeStatus::PRELOADING:
        return "preloading";

    case RuntimeStatus::RUNNING:
        return "running";

    case RuntimeStatus::REGISTERED:
        return "registered";

    case RuntimeStatus::CLOSING:
        return "closing";

    case RuntimeStatus::PAUSING:
        return "pausing";
    }
    return "unknown";
}

RunningApp::RunningApp(LaunchPointPtr launchPoint)
    : m_launchPoint(launchPoint),
      m_isPreloadMode(false),
      m_interfaceVersion(1),
      m_isRegistered(false),
      m_killingTimer(0),
      m_lastLaunchTime(0),
      m_lifeStatus(LifeStatus::STOP),
      m_runtimeStatus(RuntimeStatus::STOP)
{
    m_requestPayload = pbnjson::Object();
}

RunningApp::~RunningApp()
{
    stopKillingTimer();
}

bool RunningApp::launch(LunaTaskPtr lunaTask)
{
    LifeHandlerType type = getLaunchPoint()->getAppDesc()->getHandlerType();

    if (RuntimeStatus::LAUNCHING == getRuntimeStatus()) {
        // TODO this is already launching
        return false;
    }

    switch(type) {
    case LifeHandlerType::LifeHandlerType_Native:
        break;

    case LifeHandlerType::LifeHandlerType_Web:
        WAM::getInstance().launchApp(lunaTask);
        break;

    case LifeHandlerType::LifeHandlerType_Booster:
        break;

    default:
        break;
    }
    return true;
}

bool RunningApp::close(LunaTaskPtr lunaTask)
{
    LifeHandlerType type = getLaunchPoint()->getAppDesc()->getHandlerType();

    if (RuntimeStatus::CLOSING == getRuntimeStatus()) {
        // TODO this is already closing status
        return false;
    }

//    SAM::getInstance().EventAppLifeStatusChanged(lunaTask->getAppId(), lunaTask->getInstanceId(), RuntimeStatus::CLOSING);
//    SAM::getInstance().startTimerToKillApp(client->getAppId(), client->getPid(), allPids, TIMEOUT_1_SECOND);
    pbnjson::JValue payload = pbnjson::Object();
    switch(type) {
    case LifeHandlerType::LifeHandlerType_Native:
        if (!m_isRegistered) {
            if (!LinuxProcess::sendSigKill(m_pid)) {
                lunaTask->reply(Logger::ERRCODE_GENERAL, "not found any pids to kill");
                return false;
            }
            break;
        }

        payload.put("event", "close");
        payload.put("reason", lunaTask->getReason());
        payload.put("returnValue", true);
        sendEvent(payload);

        if (!LinuxProcess::sendSigTerm(m_pid)) {
            lunaTask->reply(Logger::ERRCODE_GENERAL, "not found any pids to kill");
            return false;
        }

        if (lunaTask->getReason() == "memoryReclaim") {
            startKillingTimer(1000);
        } else {
            startKillingTimer(10000);
        }
        break;

    case LifeHandlerType::LifeHandlerType_Web:
        WAM::getInstance().killApp(lunaTask);
        break;

    case LifeHandlerType::LifeHandlerType_Booster:
        break;

    default:
        break;
    }
    return true;
}

bool RunningApp::registerApp(LunaTaskPtr lunaTask)
{
    if (RuntimeStatus::RUNNING != getRuntimeStatus() &&
        RuntimeStatus::REGISTERED != getRuntimeStatus()) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "invalid status");
        return false;
    }

//    NativeAppLifeHandler::getInstance().registerApp(appId, message, errorText);
//
//
//    clientInfo->Register(message);
//
//    pbnjson::JValue payload = pbnjson::Object();
//    payload.put("returnValue", true);
//
//    if (clientInfo->getInterfaceVersion() == 2)
//        payload.put("event", "registered");
//    else
//        payload.put("message", "registered");
//
//    if (clientInfo->sendEvent(payload) == false) {
//        Logger::warning(getClassName(), __FUNCTION__, appId, "failed_to_send_registered_event");
//    }
//
//    Logger::info(getClassName(), __FUNCTION__, appId, "connected");
//
//    // update life status
//    EventAppLifeStatusChanged(appId, "", RuntimeStatus::REGISTERED);
//
//    handlePendingQOnRegistered(appId);

    return true;
}

bool RunningApp::sendEvent(pbnjson::JValue& payload)
{
    if (!m_isRegistered) {
        Logger::warning(CLASS_NAME, __FUNCTION__, m_instanceId, "app_is_not_registered");
        return false;
    }

    if (payload.isObject() && payload.hasKey("returnValue") == false) {
        payload.put("returnValue", true);
    }

    LSErrorSafe lserror;
//    if (!LSMessageRespond(m_request, payload.stringify().c_str(), &lserror)) {
//        Logger::error(CLASS_NAME, __FUNCTION__, m_instanceId, "respond");
//        return false;
//    }

    return true;
}

string RunningApp::getLaunchParams(LunaTaskPtr item)
{
    JValue params = pbnjson::Object();
    AppType type = this->getLaunchPoint()->getAppDesc()->getAppType();

    if (AppType::AppType_Native_Qml == type) {
        params.put("main", this->getLaunchPoint()->getAppDesc()->getAbsMain());
    }
    if (!item->getPreload().empty()) {
        params.put("preload", item->getPreload());
    }

    if (m_interfaceVersion == 1) {
        if (AppType::AppType_Native_Qml == type) {
            params.put("appId", this->getLaunchPoint()->getAppDesc()->getAppId());
            params.put("params", item->getParams());
        } else {
            params = item->getParams().duplicate();
            params.put("nid", item->getAppId());
            params.put("@system_native_app", true);
        }
    } else {
        params.put("event", "launch");
        params.put("reason", item->getReason());
        params.put("appId", item->getAppId());
        params.put("interfaceVersion", 2);
        params.put("interfaceMethod", "registerApp");
        params.put("parameters", item->getParams());
        params.put("@system_native_app", true);
    }
    return params.stringify();
}

JValue RunningApp::getRelaunchParams(LunaTaskPtr lunaTask)
{
    JValue params = pbnjson::Object();
    params.put("event", "relaunch");
    params.put("parameters", lunaTask->getParams());
    params.put("returnValue", true);

    if (m_interfaceVersion == 2) {
        params.put("reason", lunaTask->getReason());
        params.put("appId", lunaTask->getAppId());
    }
    return params;
}

gboolean RunningApp::onKillingTimer(gpointer context)
{
    RunningApp* runningApp = static_cast<RunningApp*>(context);
    if (runningApp == nullptr) {
        return FALSE;
    }

    LinuxProcess::sendSigKill(runningApp->m_pid);
    return FALSE;
}

void RunningApp::startKillingTimer(guint timeout)
{
    stopKillingTimer();
    m_killingTimer = g_timeout_add(timeout, onKillingTimer, this);
}

void RunningApp::stopKillingTimer()
{
    if (m_killingTimer != 0) {
        g_source_remove(m_killingTimer);
        m_killingTimer = 0;
    }
}
