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

#include <boost/signals2.hpp>
#include <unistd.h>

#include "bus/client/LSM.h"
#include "bus/client/MemoryManager.h"
#include "bus/service/ApplicationManager.h"
#include "base/RunningAppList.h"
#include "base/LaunchPointList.h"
#include "manager/LifecycleManager.h"
#include "setting/Settings.h"
#include "bus/client/WAM.h"
#include "base/LunaTaskList.h"
#include "base/AppDescriptionList.h"
#include "util/Time.h"
#include "util/JValueUtil.h"

#define SAM_INTERNAL_ID  "com.webos.applicationManager"
#define SLEEP_TIME_TO_CLOSE_FULLSCREEN_APP 500000

std::string LifecycleManager::toString(const LifeStatus& status)
{
    std::string str;

    switch (status) {
    case LifeStatus::PRELOADING:
        str = "preloading";
        break;

    case LifeStatus::LAUNCHING:
        str = "launching";
        break;

    case LifeStatus::RELAUNCHING:
        str = "relaunching";
        break;

    case LifeStatus::CLOSING:
        str = "closing";
        break;

    case LifeStatus::STOP:
        str = "stop";
        break;

    case LifeStatus::FOREGROUND:
        str = "foreground";
        break;

    case LifeStatus::BACKGROUND:
        str = "background";
        break;

    case LifeStatus::PAUSING:
        str = "pausing";
        break;

    default:
        str = "unknown";
        break;
    }

    return str;
}

LifecycleManager::LifecycleManager()
{
    setClassName("LifecycleManager");
}

LifecycleManager::~LifecycleManager()
{
}

void LifecycleManager::initialize()
{
    m_lifecycleRouter.initialize();

    WAM::getInstance().EventServiceStatusChanged.connect(boost::bind(&LifecycleManager::onWAMStatusChanged, this, _1));

    // receive signal on life status change
    SAM::getInstance().EventAppLifeStatusChanged.connect(boost::bind(&LifecycleManager::onRuntimeStatusChanged, this, _1, _2, _3));

    // receive signal on running list change
    SAM::getInstance().EventRunningAppAdded.connect(boost::bind(&LifecycleManager::onRunningAppAdded, this, _1, _2, _3));

    SAM::getInstance().EventRunningAppRemoved.connect(boost::bind(&LifecycleManager::onRunningAppRemoved, this, _1));
}

void LifecycleManager::onRuntimeStatusChanged(const std::string& appId, const std::string& uid, const RuntimeStatus& newStatus)
{
    if (appId.empty()) {
        Logger::error(getClassName(), __FUNCTION__, "empty_appId");
        return;
    }

    m_lifecycleRouter.setRuntimeStatus(appId, newStatus);
    LifeStatus newLifeStatus = m_lifecycleRouter.getLifeStatusFromRuntimeStatus(newStatus);
    setAppLifeStatus(appId, uid, newLifeStatus);
}

void LifecycleManager::setAppLifeStatus(const std::string& appId, const std::string& uid, LifeStatus newStatus)
{
    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId, true);

    const LifecycleRoutePolicy& routePolicy = m_lifecycleRouter.getLifeCycleRoutePolicy(runningApp->getLifeStatus(), newStatus);
    LifeStatus nextStatus;
    RouteAction routeAction;
    RouteLog routeLog;
    std::tie(nextStatus, routeAction, routeLog) = routePolicy;

    LifeEvent lifeEvent = m_lifecycleRouter.getLifeEventFromLifeStatus(nextStatus);

    // generate lifecycle event
    generateLifeCycleEvent(appId, uid, lifeEvent);

    if (RouteLog::CHECK == routeLog) {
        Logger::info(getClassName(), __FUNCTION__, appId, Logger::format("prev(%d) next(%d)", runningApp->getLifeStatus(), nextStatus));
    } else if (RouteLog::WARN == routeLog) {
        Logger::warning(getClassName(), __FUNCTION__, appId, Logger::format("handle exception: prev(%d) next(%d)", runningApp->getLifeStatus(), nextStatus));
    } else if (RouteLog::ERROR == routeLog) {
        Logger::error(getClassName(), __FUNCTION__, appId, Logger::format("unexpected transition: prev(%d) next(%d)", runningApp->getLifeStatus(), nextStatus));
    }

    if (RouteAction::IGNORE == routeAction)
        return;

    switch (nextStatus) {
    case LifeStatus::LAUNCHING:
    case LifeStatus::RELAUNCHING:
        runningApp->setPreloadMode(false);
        break;

    case LifeStatus::PRELOADING:
        runningApp->setPreloadMode(true);
        break;

    case LifeStatus::STOP:
    case LifeStatus::FOREGROUND:
        runningApp->setPreloadMode(false);
        break;

    case LifeStatus::PAUSING:
        break;

    default:
        break;
    }

    Logger::info(getClassName(), __FUNCTION__, appId,
            Logger::format("prev(%s) next(%s)", RunningApp::toString(runningApp->getLifeStatus()), RunningApp::toString(nextStatus)));

    // set life status
    runningApp->setLifeStatus(nextStatus);
    EventLifeStatusChanged(appId, nextStatus);
    replySubscriptionForAppLifeStatus(appId, uid, nextStatus);
}

void LifecycleManager::onWAMStatusChanged(bool isConnected)
{
    Logger::info(getClassName(), __FUNCTION__, "WAM_disconnected: remove_webapp_in_loading_list");
    // TODO: If WAN is killed, related requests should be failed
//    std::vector<LoadingAppItem> loading_apps = m_loadingAppList;
//    for (const auto& app : loading_apps) {
//        if (std::get<1>(app) == AppType::AppType_Web)
//            onRuntimeStatusChanged(std::get<0>(app), "", RuntimeStatus::STOP);
//    }
}

void LifecycleManager::onRunningAppAdded(const std::string& appId, const std::string& pid, const std::string& webprocid)
{
    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId, true);
    runningApp->setPid(pid);
    runningApp->setWebprocid(webprocid);
}

void LifecycleManager::onRunningAppRemoved(const std::string& appId)
{
    RunningAppList::getInstance().removeByAppId(appId);
}

void LifecycleManager::replySubscriptionForAppLifeStatus(const std::string& appId, const std::string& uid, const LifeStatus& lifeStatus)
{
    if (appId.empty()) {
        Logger::warning(getClassName(), __FUNCTION__, "null appId");
        return;
    }

    pbnjson::JValue subscriptionPayload = pbnjson::Object();
    if (subscriptionPayload.isNull()) {
        Logger::warning(getClassName(), __FUNCTION__, "make pbnjson failed");
        return;
    }

    subscriptionPayload.put("status", toString(lifeStatus));
    subscriptionPayload.put("appId", appId);

    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId);
    if (!runningApp->getPid().empty())
        subscriptionPayload.put("processId", runningApp->getPid());

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (appDesc != NULL)
        subscriptionPayload.put("type", AppDescription::toString(appDesc->getAppType()));

    LunaTaskPtr lunaTask = LunaTaskList::getInstance().getByInstanceId(uid);
    if (LifeStatus::LAUNCHING == lifeStatus || LifeStatus::RELAUNCHING == lifeStatus) {
        if (lunaTask) {
            subscriptionPayload.put("reason", lunaTask->getReason());
        }
    } else if (LifeStatus::FOREGROUND == lifeStatus) {
        pbnjson::JValue fg_info = pbnjson::JValue();
        RunningAppList::getInstance().getForegroundInfoById(appId, fg_info);
        if (!fg_info.isNull() && fg_info.isObject()) {
            for (auto it : fg_info.children()) {
                const std::string key = it.first.asString();
                if ("windowType" == key || "windowGroup" == key || "windowGroupOwner" == key || "windowGroupOwnerId" == key) {
                    subscriptionPayload.put(key, fg_info[key]);
                }
            }
        }
    } else if (LifeStatus::BACKGROUND == lifeStatus) {
        if (runningApp->getPreloadMode())
            subscriptionPayload.put("backgroundStatus", "preload");
        else
            subscriptionPayload.put("backgroundStatus", "normal");
    } else if (LifeStatus::STOP == lifeStatus || LifeStatus::CLOSING == lifeStatus) {
        subscriptionPayload.put("reason", (m_closeReasonInfo.count(appId) == 0) ? "undefined" : m_closeReasonInfo[appId]);

        if (LifeStatus::STOP == lifeStatus)
            m_closeReasonInfo.erase(appId);
    }

    ApplicationManager::getInstance().postGetAppLifeStatus(subscriptionPayload);
}

void LifecycleManager::generateLifeCycleEvent(const std::string& appId, const std::string& uid, LifeEvent event)
{
    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    LunaTaskPtr lunaTask = LunaTaskList::getInstance().getByInstanceId(uid);
    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId);

    pbnjson::JValue subscriptionPayload = pbnjson::Object();
    pbnjson::JValue info = pbnjson::JValue();
    subscriptionPayload.put("appId", appId);

    switch (event) {
    case LifeEvent::SPLASH:
        // generate splash event only for fresh launch case
        if (runningApp && !runningApp->isShowSplash())
            return;
        if (runningApp && LifeStatus::BACKGROUND == runningApp->getLifeStatus() && runningApp->getPreloadMode() == false)
            return;
        if (runningApp && LifeStatus::STOP != runningApp->getLifeStatus() && LifeStatus::PRELOADING != runningApp->getLifeStatus() && LifeStatus::BACKGROUND != runningApp->getLifeStatus())
            return;

        subscriptionPayload.put("event", "splash");
        subscriptionPayload.put("title", (appDesc ? appDesc->getTitle() : ""));
        subscriptionPayload.put("showSplash", (lunaTask && lunaTask->isShowSplash()));
        subscriptionPayload.put("showSpinner", (lunaTask && lunaTask->isShowSpinner()));

        if (lunaTask && lunaTask->isShowSplash())
            subscriptionPayload.put("splashBackground", (appDesc ? appDesc->getSplashBackground() : ""));
        break;

    case LifeEvent::PRELOAD:
        subscriptionPayload.put("event", "preload");
        if (lunaTask)
            subscriptionPayload.put("preload", lunaTask->getPreload());
        break;

    case LifeEvent::LAUNCH:
        subscriptionPayload.put("event", "launch");
        subscriptionPayload.put("reason", lunaTask->getReason());
        break;

    case LifeEvent::FOREGROUND:
        subscriptionPayload.put("event", "foreground");
        RunningAppList::getInstance().getForegroundInfoById(appId, info);
        if (!info.isNull() && info.isObject()) {
            for (auto it : info.children()) {
                const std::string key = it.first.asString();
                if ("windowType" == key ||
                    "windowGroup" == key ||
                    "windowGroupOwner" == key ||
                    "windowGroupOwnerId" == key) {
                    subscriptionPayload.put(key, info[key]);
                }
            }
        }
        break;

    case LifeEvent::BACKGROUND:
        subscriptionPayload.put("event", "background");
        if (runningApp->getPreloadMode())
            subscriptionPayload.put("status", "preload");
        else
            subscriptionPayload.put("status", "normal");
        break;

    case LifeEvent::PAUSE:
        subscriptionPayload.put("event", "pause");
        break;

    case LifeEvent::CLOSE:
        subscriptionPayload.put("event", "close");
        subscriptionPayload.put("reason", (m_closeReasonInfo.count(appId) == 0) ? "undefined" : m_closeReasonInfo[appId]);
        break;

    case LifeEvent::STOP:
        subscriptionPayload.put("event", "stop");
        subscriptionPayload.put("reason", (m_closeReasonInfo.count(appId) == 0) ? "undefined" : m_closeReasonInfo[appId]);
        break;

    default:
        return;
    }
    ApplicationManager::getInstance().postGetAppLifeEvents(subscriptionPayload);
}

void LifecycleManager::registerApp(const std::string& appId, LSMessage* message, std::string& errorText)
{

}

void LifecycleManager::registerNativeApp(const std::string& appId, LSMessage* message, std::string& errorText)
{

}
