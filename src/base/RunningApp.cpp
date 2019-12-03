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
#include "bus/client/NativeContainer.h"
#include "util/LinuxProcess.h"

const string RunningApp::CLASS_NAME = "RunningApp";

const char* RunningApp::toString(LifeStatus status)
{
    switch (status) {
    case LifeStatus::LifeStatus_STOP:
        return "stop";

    case LifeStatus::LifeStatus_PRELOADING:
        return "preloading";

    case LifeStatus::LifeStatus_PRELOADED:
        return "preloaded";

    case LifeStatus::LifeStatus_SPLASHING:
        return "splashing";

    case LifeStatus::LifeStatus_SPLASHED:
        return "splashed";

    case LifeStatus::LifeStatus_LAUNCHING:
        return "launching";

    case LifeStatus::LifeStatus_RELAUNCHING:
        return "relaunching";

    case LifeStatus::LifeStatus_FOREGROUND:
        return "foreground";

    case LifeStatus::LifeStatus_BACKGROUND:
        return "background";

    case LifeStatus::LifeStatus_PAUSING:
        return "pausing";

    case LifeStatus::LifeStatus_PAUSED:
        return "paused";

    case LifeStatus::LifeStatus_CLOSING:
        return "closing";
    }
    return "unknown";
}

bool RunningApp::isTransition(LifeStatus status)
{
    switch (status) {
    case LifeStatus::LifeStatus_STOP:
        return false;

    case LifeStatus::LifeStatus_PRELOADING:
        return true;

    case LifeStatus::LifeStatus_PRELOADED:
        return false;

    case LifeStatus::LifeStatus_SPLASHING:
        return true;

    case LifeStatus::LifeStatus_SPLASHED:
        return false;

    case LifeStatus::LifeStatus_LAUNCHING:
        return true;

    case LifeStatus::LifeStatus_RELAUNCHING:
        return true;

    case LifeStatus::LifeStatus_FOREGROUND:
        return false;

    case LifeStatus::LifeStatus_BACKGROUND:
        return false;

    case LifeStatus::LifeStatus_PAUSING:
        return true;

    case LifeStatus::LifeStatus_PAUSED:
        return false;

    case LifeStatus::LifeStatus_CLOSING:
        return true;
    }
    return false;
}

RunningApp::RunningApp(LaunchPointPtr launchPoint)
    : m_launchPoint(launchPoint),
      m_instanceId(""),
      m_processId(""),
      m_webprocessid(""),
      m_displayId(-1),
      m_interfaceVersion(1),
      m_isRegistered(false),
      m_lifeStatus(LifeStatus::LifeStatus_STOP),
      m_killingTimer(0),
      m_keepAlive(false),
      m_noSplash(true),
      m_spinner(true),
      m_isLaunchedHidden(false),
      m_isFirstLaunch(true)
{
}

RunningApp::~RunningApp()
{
    stopKillingTimer();
}

void RunningApp::launch(LunaTaskPtr lunaTask)
{
    if (LifeStatus::LifeStatus_LAUNCHING == m_lifeStatus) {
        Logger::warning(CLASS_NAME, __FUNCTION__, m_instanceId, "The instance is already launching");
        lunaTask->getResponsePayload().put("returnValue", true);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    LifeHandlerType type = getLaunchPoint()->getAppDesc()->getLifeHandlerType();
    switch(type) {
    case LifeHandlerType::LifeHandlerType_Native:
        NativeContainer::getInstance().launch(*this, lunaTask);
        break;

    case LifeHandlerType::LifeHandlerType_Web:
        WAM::getInstance().launchApp(*this, lunaTask);
        break;

    case LifeHandlerType::LifeHandlerType_Booster:
        Booster::getInstance().launch(*this, lunaTask);
        break;

    default:
        break;
    }
}

gboolean RunningApp::onKillingTimer(gpointer context)
{
    RunningApp* runningApp = static_cast<RunningApp*>(context);
    if (runningApp == nullptr) {
        return FALSE;
    }
    Logger::warning(CLASS_NAME, __FUNCTION__, runningApp->m_instanceId, "Transition is timeout");
    runningApp->m_killingTimer = 0;

    LifeHandlerType type = runningApp->getLaunchPoint()->getAppDesc()->getLifeHandlerType();
    switch(type) {
    case LifeHandlerType::LifeHandlerType_Native:
        break;

    case LifeHandlerType::LifeHandlerType_Web:
        WAM::getInstance().killApp(*runningApp);
        break;

    case LifeHandlerType::LifeHandlerType_Booster:
        break;

    default:
        break;
    }
    return FALSE;
}

void RunningApp::close(LunaTaskPtr lunaTask)
{
    if (m_lifeStatus == LifeStatus::LifeStatus_CLOSING) {
        Logger::warning(CLASS_NAME, __FUNCTION__, m_instanceId, "The instance is already closing");
        lunaTask->getResponsePayload().put("returnValue", true);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    LifeHandlerType type = getLaunchPoint()->getAppDesc()->getLifeHandlerType();
    switch(type) {
    case LifeHandlerType::LifeHandlerType_Native:
        setLifeStatus(LifeStatus::LifeStatus_CLOSING);
        NativeContainer::getInstance().close(*this, lunaTask);
        break;

    case LifeHandlerType::LifeHandlerType_Web:
        WAM::getInstance().close(*this, lunaTask);
        break;

    case LifeHandlerType::LifeHandlerType_Booster:
        setLifeStatus(LifeStatus::LifeStatus_CLOSING);
        Booster::getInstance().close(*this, lunaTask);
        break;

    default:
        break;
    }
    return;
}

void RunningApp::registerApp(LunaTaskPtr lunaTask)
{
    if (m_isRegistered) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "The app is already registered");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    m_registeredApp = lunaTask->getRequest();
    m_isRegistered = true;

    JValue payload = pbnjson::Object();
    if (m_interfaceVersion == 1) {
        payload.put("message", "registered");
    } else if (m_interfaceVersion == 2) {
        payload.put("event", "registered");
    }

    if (!sendEvent(payload)) {
        Logger::warning(CLASS_NAME, __FUNCTION__, m_instanceId, "Failed to register application");
        m_isRegistered = false;
        return;
    }
    Logger::info(CLASS_NAME, __FUNCTION__, m_instanceId, "Application is registered");
}

bool RunningApp::sendEvent(pbnjson::JValue& responsePayload)
{
    if (!m_isRegistered) {
        Logger::warning(CLASS_NAME, __FUNCTION__, m_instanceId, "RunningApp is not registered");
        return false;
    }

    responsePayload.put("returnValue", true);
    Logger::logAPIResponse(CLASS_NAME, __FUNCTION__, m_registeredApp, responsePayload);
    m_registeredApp.respond(responsePayload.stringify().c_str());
    return true;
}

string RunningApp::getLaunchParams(LunaTaskPtr lunaTask)
{
    JValue params = pbnjson::Object();
    AppType type = this->getLaunchPoint()->getAppDesc()->getAppType();

    if (AppType::AppType_Native_Qml == type) {
        params.put("main", this->getLaunchPoint()->getAppDesc()->getAbsMain());
    }
    if (!m_preload.empty()) {
        params.put("preload", m_preload);
    }
    if (m_interfaceVersion == 1) {
        if (AppType::AppType_Native_Qml == type) {
            params.put("appId", this->getLaunchPoint()->getAppDesc()->getAppId());
            params.put("params", lunaTask->getParams());
        } else {
            params = lunaTask->getParams().duplicate();
            params.put("nid", lunaTask->getAppId());
            params.put("@system_native_app", true);
        }
    } else {
        params.put("event", "launch");
        params.put("reason", lunaTask->getReason());
        params.put("appId", lunaTask->getAppId());
        params.put("interfaceVersion", 2);
        params.put("interfaceMethod", "registerApp");
        params.put("parameters", lunaTask->getParams());
        params.put("@system_native_app", true);
    }
    return params.stringify();
}

JValue RunningApp::getRelaunchParams(LunaTaskPtr lunaTask)
{
    JValue params = pbnjson::Object();
    params.put("returnValue", true);

    if (m_interfaceVersion == 1) {
        params.put("message", "relaunch");
        params.put("parameters", lunaTask->getParams());
    } else if (m_interfaceVersion == 2) {
        params.put("event", "relaunch");
        params.put("parameters", lunaTask->getParams());
        params.put("reason", lunaTask->getReason());
        params.put("appId", lunaTask->getAppId());
    }
    return params;
}

bool RunningApp::setLifeStatus(LifeStatus lifeStatus)
{
    if (m_lifeStatus == lifeStatus) {
        Logger::debug(CLASS_NAME, __FUNCTION__, m_instanceId,
                      Logger::format("Ignored: %s(%s ==> %s)", getAppId().c_str(), toString(m_lifeStatus), toString(lifeStatus)));
        return true;
    }

    // CLOSING is special transition. It should be allowed all cases
    if (lifeStatus != LifeStatus::LifeStatus_CLOSING && isTransition(m_lifeStatus) && isTransition(lifeStatus)) {
        Logger::warning(CLASS_NAME, __FUNCTION__, m_instanceId,
                        Logger::format("Warning: %s(%s ==> %s)", getAppId().c_str(), toString(m_lifeStatus), toString(lifeStatus)));
        return false;
    }

    switch (lifeStatus) {
    case LifeStatus::LifeStatus_STOP:
        if (m_lifeStatus == LifeStatus::LifeStatus_CLOSING)
            Logger::info(CLASS_NAME, __FUNCTION__, m_instanceId, "Closed by SAM");
        else
            Logger::info(CLASS_NAME, __FUNCTION__, m_instanceId, "Closed by itself");
        break;

    case LifeStatus::LifeStatus_LAUNCHING:
        if (m_lifeStatus == LifeStatus::LifeStatus_FOREGROUND) {
            Logger::info(CLASS_NAME, __FUNCTION__, m_instanceId,
                         Logger::format("Changed: %s(%s ==> %s)", getAppId().c_str(), toString(m_lifeStatus), toString(LifeStatus::LifeStatus_RELAUNCHING)));
            m_lifeStatus = LifeStatus::LifeStatus_RELAUNCHING;
            ApplicationManager::getInstance().postGetAppLifeStatus(*this);
            lifeStatus = LifeStatus::LifeStatus_FOREGROUND;
        } else if (m_lifeStatus == LifeStatus::LifeStatus_BACKGROUND ||
                   m_lifeStatus == LifeStatus::LifeStatus_PAUSED ||
                   m_lifeStatus == LifeStatus::LifeStatus_PRELOADED) {
            lifeStatus = LifeStatus::LifeStatus_RELAUNCHING;
        }
        break;

    default:
        break;
    }

    Logger::info(CLASS_NAME, __FUNCTION__, m_instanceId,
                 Logger::format("Changed: %s(%s ==> %s)", getAppId().c_str(), toString(m_lifeStatus), toString(lifeStatus)));
    m_lifeStatus = lifeStatus;

    if (isTransition(m_lifeStatus)) {
        // Transition should be done under 5 seconds
        startKillingTimer(TIMEOUT_TRANSITION);
    } else {
        stopKillingTimer();
    }

    ApplicationManager::getInstance().postGetAppLifeStatus(*this);
    ApplicationManager::getInstance().postGetAppLifeEvents(*this);
    return true;
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
