// Copyright (c) 2019-2020 LG Electronics, Inc.
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

#include "bus/client/AbsLifeHandler.h"
#include "bus/service/ApplicationManager.h"
#include "conf/SAMConf.h"

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

string RunningApp::generateInstanceId(int displayId)
{
    string instanceId = Time::generateUid();
    instanceId += std::to_string(displayId);
    return instanceId;
}

int RunningApp::getDisplayId(const string& instanceId)
{
    int displayId = instanceId.back() - '0';
    if (displayId < 0 || displayId > 10)
        displayId = 0;
    return displayId;
}

RunningApp::RunningApp(LaunchPointPtr launchPoint)
    : m_launchPoint(launchPoint),
      m_instanceId(""),
      m_displayId(-1),
      m_webprocessid(""),
      m_isFullWindow(true),
      m_lifeStatus(LifeStatus::LifeStatus_STOP),
      m_isFirstLaunch(true),
      m_killingTimer(0),
      m_keepAlive(false),
      m_noSplash(true),
      m_spinner(true),
      m_launchedHidden(false),
      m_token(0),
      m_context(0),
      m_ls2name(""),
      m_isRegistered(false)
{
    m_startTime = Time::getCurrentTime();
}

RunningApp::~RunningApp()
{
    stopKillingTimer();
}

void RunningApp::registerApp(LunaTaskPtr lunaTask)
{
    if (m_isRegistered) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "The app is already registered");
        lunaTask->error(lunaTask);
        return;
    }

    m_registeredApp = lunaTask->getRequest();
    m_isRegistered = true;

    JValue payload = pbnjson::Object();
    payload.put("event", "registered");
    payload.put("message", "registered");// TODO this should be removed. Let's use event only.

    if (!sendEvent(payload)) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Failed to register application");
        lunaTask->error(lunaTask);
        m_isRegistered = false;
        return;
    }
    Logger::info(CLASS_NAME, __FUNCTION__, m_instanceId, "Application is registered");
}

void RunningApp::restoreIds(LunaTaskPtr lunaTask)
{
    lunaTask->setInstanceId(getInstanceId());
    lunaTask->setLaunchPointId(getLaunchPointId());
    lunaTask->setAppId(getAppId());
    lunaTask->setDisplayId(getDisplayId());
}

bool RunningApp::sendEvent(JValue& responsePayload)
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

void RunningApp::setLifeStatus(LifeStatus lifeStatus)
{
    if (m_lifeStatus == lifeStatus) {
        return;
    }

    // CLOSING is special transition. It should be allowed all cases
    if (isTransition(m_lifeStatus) && isTransition(lifeStatus) && lifeStatus != LifeStatus::LifeStatus_CLOSING) {
        Logger::warning(CLASS_NAME, __FUNCTION__, m_instanceId,
                        Logger::format("Warning: %s (%s ==> %s)", getAppId().c_str(), toString(m_lifeStatus), toString(lifeStatus)));
        return;
    }

    // First launching is completed
    if (lifeStatus == LifeStatus::LifeStatus_FOREGROUND ||
        lifeStatus == LifeStatus::LifeStatus_BACKGROUND ||
        lifeStatus == LifeStatus::LifeStatus_PAUSED ||
        lifeStatus == LifeStatus::LifeStatus_PRELOADED)
        m_isFirstLaunch = false;

    switch (lifeStatus) {
    case LifeStatus::LifeStatus_STOP:
        // LifeStatus_STOP should not be set directly. Only RunningAppList can set this status.
        if (m_lifeStatus == LifeStatus::LifeStatus_CLOSING)
            Logger::info(CLASS_NAME, __FUNCTION__, m_instanceId, "Closed by SAM");
        else
            Logger::info(CLASS_NAME, __FUNCTION__, m_instanceId, "Closed by Itself");
        break;

    case LifeStatus::LifeStatus_LAUNCHING:
        if (m_lifeStatus == LifeStatus::LifeStatus_FOREGROUND) {
            Logger::info(CLASS_NAME, __FUNCTION__, m_instanceId,
                         Logger::format("Changed: %s (%s ==> %s)", getAppId().c_str(), toString(m_lifeStatus), toString(LifeStatus::LifeStatus_RELAUNCHING)));
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
                 Logger::format("Changed: %s (%s ==> %s)", getAppId().c_str(), toString(m_lifeStatus), toString(lifeStatus)));
    m_lifeStatus = lifeStatus;

    // Normally, transition should be completed within timeout sec
    // However, sometimes, it takes more than 10 seconds to launch the target app.
    // For example, youtube app can be foreground after 30 seconds at boottime
    // See more info here PLAT-101882.
    if (isTransition(m_lifeStatus) && m_lifeStatus != LifeStatus::LifeStatus_LAUNCHING) {
        startKillingTimer(TIMEOUT_TRANSITION);
    } else {
        stopKillingTimer();
    }

    ApplicationManager::getInstance().postGetAppLifeStatus(*this);
    ApplicationManager::getInstance().postGetAppLifeEvents(*this);
}

gboolean RunningApp::onKillingTimer(gpointer context)
{
    RunningApp* self = static_cast<RunningApp*>(context);
    if (self == nullptr) {
        return G_SOURCE_REMOVE;
    }
    RunningAppPtr runningApp = RunningAppList::getInstance().getByInstanceId(self->getInstanceId());
    if (runningApp == nullptr) {
        return G_SOURCE_REMOVE;
    }
    Logger::warning(CLASS_NAME, __FUNCTION__, self->m_instanceId, "Transition is timeout");

    AbsLifeHandler::getLifeHandler(runningApp).kill(runningApp);

    // It tries to kill the app continually
    return G_SOURCE_CONTINUE;
}

void RunningApp::startKillingTimer(guint timeout)
{
    stopKillingTimer();
    m_killingTimer = g_timeout_add(timeout, onKillingTimer, this);
}

void RunningApp::stopKillingTimer()
{
    if (m_killingTimer > 0) {
        g_source_remove(m_killingTimer);
        m_killingTimer = 0;
    }
}
