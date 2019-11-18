// Copyright (c) 2017-2019 LG Electronics, Inc.
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

#ifndef BUS_SERVICE_APPLICATIONMANAGER_H_
#define BUS_SERVICE_APPLICATIONMANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <boost/function.hpp>
#include <boost/signals2.hpp>
#include <conf/SAMConf.h>
#include <pbnjson.hpp>
#include <luna-service2/lunaservice.hpp>

#include "base/AppDescriptionList.h"
#include "base/LunaTask.h"
#include "base/LaunchPoint.h"
#include "base/LaunchPointList.h"
#include "base/RunningApp.h"
#include "base/RunningAppList.h"
#include "bus/service/compat/ApplicationManagerCompat.h"
#include "interface/IClassName.h"
#include "interface/ISingleton.h"
#include "util/Logger.h"
#include "util/File.h"

#define SUBSKEY_RUNNING               "running"
#define SUBSKEY_DEV_RUNNING           "dev_running"
#define SUBSKEY_FOREGROUND_INFO       "foregroundAppInfo"
#define SUBSKEY_FOREGROUND_INFO_EX    "foregroundAppInfoEx"
#define SUBSKEY_GET_APP_LIFE_EVENTS   "getAppLifeEvents"
#define SUBSKEY_GET_APP_LIFE_STATUS   "getAppLifeStatus"
#define SUBSKEY_ON_LAUNCH             "onLaunch"
#define SUBSKEY_LIST_APPS             "listApps"
#define SUBSKEY_LIST_APPS_COMPACT     "listAppsCompact"
#define SUBSKEY_DEV_LIST_APPS         "listDevApps"
#define SUBSKEY_DEV_LIST_APPS_COMPACT "listDevAppsCompact"

using namespace LS;

typedef boost::function<void(LunaTaskPtr)> LunaApiHandler;

class ApplicationManager : public LS::Handle,
                           public ISingleton<ApplicationManager>,
                           public IClassName {
friend class ISingleton<ApplicationManager> ;
public:
    static const char* CATEGORY_ROOT;
    static const char* CATEGORY_DEV;

    static const char* METHOD_LAUNCH;
    static const char* METHOD_PAUSE;
    static const char* METHOD_CLOSE_BY_APPID;
    static const char* METHOD_RUNNING;
    static const char* METHOD_GET_APP_LIFE_EVENTS;
    static const char* METHOD_GET_APP_LIFE_STATUS;
    static const char* METHOD_GET_FOREGROUND_APPINFO;
    static const char* METHOD_LOCK_APP;
    static const char* METHOD_REGISTER_APP;

    static const char* METHOD_LIST_APPS;
    static const char* METHOD_GET_APP_STATUS;
    static const char* METHOD_GET_APP_INFO;
    static const char* METHOD_GET_APP_BASE_PATH;

    static const char* METHOD_ADD_LAUNCHPOINT;
    static const char* METHOD_UPDATE_LAUNCHPOINT;
    static const char* METHOD_REMOVE_LAUNCHPOINT;
    static const char* METHOD_MOVE_LAUNCHPOINT;
    static const char* METHOD_LIST_LAUNCHPOINTS;

    static const char* METHOD_MANAGER_INFO;

    virtual ~ApplicationManager();

    virtual bool attach(GMainLoop* gml);
    virtual void detach();

    // APIs
    void launch(LunaTaskPtr lunaTask);
    void pause(LunaTaskPtr lunaTask);
    void closeByAppId(LunaTaskPtr lunaTask);
    void running(LunaTaskPtr lunaTask);
    void runningForDev(LunaTaskPtr lunaTask);
    void getAppLifeEvents(LunaTaskPtr lunaTask);
    void getAppLifeStatus(LunaTaskPtr lunaTask);
    void getForegroundAppInfo(LunaTaskPtr lunaTask);
    void lockApp(LunaTaskPtr lunaTask);
    void registerApp(LunaTaskPtr lunaTask);
    void registerNativeApp(LunaTaskPtr lunaTask);

    void listApps(LunaTaskPtr lunaTask);
    void getAppStatus(LunaTaskPtr lunaTask);
    void getAppInfo(LunaTaskPtr lunaTask);
    void getAppBasePath(LunaTaskPtr lunaTask);

    void addLaunchPoint(LunaTaskPtr lunaTask);
    void updateLaunchPoint(LunaTaskPtr lunaTask);
    void removeLaunchPoint(LunaTaskPtr lunaTask);
    void moveLaunchPoint(LunaTaskPtr lunaTask);
    void listLaunchPoints(LunaTaskPtr lunaTask);

    void managerInfo(LunaTaskPtr lunaTask);

    void enableSubscription();
    void disableSubscription();

    void postGetAppLifeEvents(RunningApp& runningApp)
    {
        pbnjson::JValue subscriptionPayload = pbnjson::Object();
        pbnjson::JValue info = pbnjson::JValue();
        subscriptionPayload.put("appId", runningApp.getAppId());
        subscriptionPayload.put("instanceId", runningApp.getInstanceId());

        switch (runningApp.getLifeStatus()) {
        case LifeStatus::LifeStatus_STOP:
            subscriptionPayload.put("event", "stop");
            subscriptionPayload.put("reason", runningApp.getReason());
            break;

        case LifeStatus::LifeStatus_PRELOADING:
            subscriptionPayload.put("event", "preload");
            subscriptionPayload.put("preload", runningApp.getPreload());
            break;

        case LifeStatus::LifeStatus_LAUNCHING:
            // generate splash event only for fresh launch case
            if (!runningApp.isShowSplash())
                return;
            if (LifeStatus::LifeStatus_BACKGROUND == runningApp.getLifeStatus() && runningApp.getPreloadMode() == false)
                return;
            if (LifeStatus::LifeStatus_STOP != runningApp.getLifeStatus() &&
                LifeStatus::LifeStatus_PRELOADING != runningApp.getLifeStatus() &&
                LifeStatus::LifeStatus_BACKGROUND != runningApp.getLifeStatus())
                return;

            subscriptionPayload.put("event", "splash");
            subscriptionPayload.put("title", runningApp.getLaunchPoint()->getTitle());
            subscriptionPayload.put("showSplash", runningApp.isShowSplash());
            subscriptionPayload.put("showSpinner", runningApp.isShowSplash());

            if (runningApp.isShowSplash())
                subscriptionPayload.put("splashBackground", runningApp.getLaunchPoint()->getAppDesc()->getSplashBackground());

            // TODO Slash and launch를 나눠야함

        case LifeStatus::LifeStatus_RELAUNCHING:
            subscriptionPayload.put("event", "launch");
            subscriptionPayload.put("reason", runningApp.getReason());
            break;

        case LifeStatus::LifeStatus_FOREGROUND:
            subscriptionPayload.put("event", "foreground");
            RunningAppList::getInstance().getForegroundInfoById(runningApp.getAppId(), info);
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

        case LifeStatus::LifeStatus_BACKGROUND:
            subscriptionPayload.put("event", "background");
            if (runningApp.getPreloadMode())
                subscriptionPayload.put("status", "preload");
            else
                subscriptionPayload.put("status", "normal");
            break;

        case LifeStatus::LifeStatus_CLOSING:
            subscriptionPayload.put("event", "close");
            subscriptionPayload.put("reason", runningApp.getReason());
            break;

        case LifeStatus::LifeStatus_PAUSING:
            subscriptionPayload.put("event", "pause");
            break;

        case LifeStatus::LifeStatus_RUNNING:
            break;
        }

        subscriptionPayload.put("returnValue", true);
        subscriptionPayload.put("subscribed", true);
        Logger::logSubscriptionPost(getClassName(), __FUNCTION__, m_getAppLifeEvents, subscriptionPayload);
        m_getAppLifeEvents.post(subscriptionPayload.stringify().c_str());
    }

    void postGetAppLifeStatus(RunningApp& runningApp)
    {
        pbnjson::JValue subscriptionPayload = pbnjson::Object();
        if (subscriptionPayload.isNull()) {
            Logger::warning(getClassName(), __FUNCTION__, "make pbnjson failed");
            return;
        }

        subscriptionPayload.put("appId", runningApp.getAppId());
        subscriptionPayload.put("instanceId", runningApp.getInstanceId());
        subscriptionPayload.put("status", RunningApp::toString(runningApp.getLifeStatus()));
        subscriptionPayload.put("processId", runningApp.getPid());
        subscriptionPayload.put("type", AppDescription::toString(runningApp.getLaunchPoint()->getAppDesc()->getAppType()));

        pbnjson::JValue fg_info = pbnjson::JValue();
        switch(runningApp.getLifeStatus()) {
        case LifeStatus::LifeStatus_LAUNCHING:
        case LifeStatus::LifeStatus_RELAUNCHING:
            subscriptionPayload.put("reason", runningApp.getReason());
            break;

        case LifeStatus::LifeStatus_FOREGROUND:
            RunningAppList::getInstance().getForegroundInfoById(runningApp.getAppId(), fg_info);
            if (!fg_info.isNull() && fg_info.isObject()) {
                for (auto it : fg_info.children()) {
                    const std::string key = it.first.asString();
                    if ("windowType" == key ||
                        "windowGroup" == key ||
                        "windowGroupOwner" == key ||
                        "windowGroupOwnerId" == key) {
                        subscriptionPayload.put(key, fg_info[key]);
                    }
                }
            }
            break;

        case LifeStatus::LifeStatus_BACKGROUND:
            if (runningApp.getPreloadMode())
                subscriptionPayload.put("backgroundStatus", "preload");
            else
                subscriptionPayload.put("backgroundStatus", "normal");
            break;

        case LifeStatus::LifeStatus_STOP:
        case LifeStatus::LifeStatus_CLOSING:
            subscriptionPayload.put("reason", runningApp.getReason());
            break;
        }

        subscriptionPayload.put("returnValue", true);
        subscriptionPayload.put("subscribed", true);
        Logger::logSubscriptionPost(getClassName(), __FUNCTION__, m_getAppLifeEvents, subscriptionPayload);
        m_getAppLifeEvents.post(subscriptionPayload.stringify().c_str());
    }

    void postListLaunchPoints(LaunchPointPtr launchPoint)
    {
        if (launchPoint && !launchPoint->isVisible())
            return;

        pbnjson::JValue launchPoints;
        if (launchPoint == nullptr) {
            launchPoints = pbnjson::Array();
            LaunchPointList::getInstance().toJson(launchPoints);
        } else {
            launchPoint->toJson(launchPoints);
        }

        pbnjson::JValue subscriptionPayload = pbnjson::Object();
        subscriptionPayload.put("subscribed", true);
        subscriptionPayload.put("returnValue", true);
        subscriptionPayload.put("launchPoints", launchPoints);

        Logger::logSubscriptionPost(getClassName(), __FUNCTION__, m_listLaunchPointsPoint, subscriptionPayload);
        m_listLaunchPointsPoint.post(subscriptionPayload.stringify().c_str());
    }

    void postGetAppStatus(AppDescriptionPtr appDesc, AppStatusEvent event)
    {
        if (!appDesc) {
            return;
        }

        pbnjson::JValue subscriptionPayload = pbnjson::Object();

        switch (event) {
        case AppStatusEvent::AppStatusEvent_Installed:
        case AppStatusEvent::AppStatusEvent_UpdateCompleted:
            subscriptionPayload.put("status", "launchable");
            subscriptionPayload.put("exist", true);
            subscriptionPayload.put("launchable", true);
            break;

        case AppStatusEvent::AppStatusEvent_Uninstalled:
            subscriptionPayload.put("status", "notExist");
            subscriptionPayload.put("exist", false);
            subscriptionPayload.put("launchable", false);
            break;

        default:
            return;
        }

        subscriptionPayload.put("appId", appDesc->getAppId());
        subscriptionPayload.put("event", AppDescription::toString(event));
        subscriptionPayload.put("returnValue", true);

        std::string nKey = "getappstatus#" + appDesc->getAppId() + "#N";
        Logger::logSubscriptionPost(getClassName(), __FUNCTION__, nKey, subscriptionPayload);
        if (!LSSubscriptionReply(ApplicationManager::getInstance().get(), nKey.c_str(), subscriptionPayload.stringify().c_str(), NULL)) {
            Logger::warning(getClassName(), __FUNCTION__, "Failed to post subscription");
        }

        switch (event) {
        case AppStatusEvent::AppStatusEvent_Installed:
        case AppStatusEvent::AppStatusEvent_UpdateCompleted:
            subscriptionPayload.put("appInfo", appDesc->getJson());
            break;

        default:
            break;
        }

        std::string yKey = "getappstatus#" + appDesc->getAppId() + "#Y";
        Logger::logSubscriptionPost(getClassName(), __FUNCTION__, yKey, subscriptionPayload);
        if (!LSSubscriptionReply(ApplicationManager::getInstance().get(), yKey.c_str(), subscriptionPayload.stringify().c_str(), NULL)) {
            Logger::warning(getClassName(), __FUNCTION__, "Failed to post subscription");
        }
    }

    void postGetForegroundAppIfo(const std::string& appId)
    {
        pbnjson::JValue subscriptionPayload = pbnjson::Object();
        subscriptionPayload.put("returnValue", true);
        subscriptionPayload.put("subscribed", true);
        subscriptionPayload.put("appId", appId);
        subscriptionPayload.put("windowId", "");
        subscriptionPayload.put("processId", "");

        Logger::logSubscriptionPost(getClassName(), __FUNCTION__, SUBSKEY_FOREGROUND_INFO, subscriptionPayload);
        if (!LSSubscriptionReply(ApplicationManager::getInstance().get(),
                                 SUBSKEY_FOREGROUND_INFO,
                                 subscriptionPayload.stringify().c_str(),
                                 NULL)) {
            Logger::error(getClassName(), __FUNCTION__, SUBSKEY_FOREGROUND_INFO, subscriptionPayload.stringify());
            return;
        }
    }

    void postGetForegroundAppIfoExtra(const pbnjson::JValue& foreground_info)
    {
        pbnjson::JValue subscriptionPayload = pbnjson::Object();
        subscriptionPayload.put("returnValue", true);
        subscriptionPayload.put("subscribed", true);
        subscriptionPayload.put("foregroundAppInfo", foreground_info);

        Logger::logSubscriptionPost(getClassName(), __FUNCTION__, SUBSKEY_FOREGROUND_INFO_EX, subscriptionPayload);
        if (!LSSubscriptionReply(ApplicationManager::getInstance().get(),
                                 SUBSKEY_FOREGROUND_INFO_EX,
                                 subscriptionPayload.stringify().c_str(),
                                 NULL)) {
            Logger::error(getClassName(), __FUNCTION__, "LS2", "subscriptionreply", subscriptionPayload.stringify());
            return;
        }
    }

    void postListApps(const string& appId, const string& change, const string& changeReason)
    {
        JValue subscriptionPayload = pbnjson::Object();
        subscriptionPayload.put("change", change);
        subscriptionPayload.put("changeReason", changeReason);
        subscriptionPayload.put("returnValue", true);
        subscriptionPayload.put("subscribed", true);

        AppDescriptionPtr appDesc = nullptr;
        if (!appId.empty()) {
            appDesc = AppDescriptionList::getInstance().getById(appId);
            Logger::warning(getClassName(), __FUNCTION__, appId, "Cannot fine the appId");
            return;
        }

        LSSubscriptionIter *iter = NULL;
        if (!LSSubscriptionAcquire(ApplicationManager::getInstance().get(), METHOD_LIST_APPS, &iter, NULL))
            return;

        while (LSSubscriptionHasNext(iter)) {
            LSMessage* message = LSSubscriptionNext(iter);
            Message request(message);
            bool isDevmode = (strcmp(request.getKind(), "/dev/listApps") == 0);

            if (isDevmode && !SAMConf::getInstance().isDevmodeEnabled()) {
                continue;
            }

            pbnjson::JValue requestPayload = JDomParser::fromString(request.getPayload(), JValueUtil::getSchema("applicationManager.listApps"));
            if (requestPayload.isNull()) {
                continue;
            }

            JValue properties;
            if (JValueUtil::getValue(requestPayload, "properties", properties) && properties.isArray()) {
                properties.append("id");
            }

            if (appDesc != nullptr) {
                if (appDesc->isDevmodeApp() != isDevmode)
                    continue;
                pbnjson::JValue app = appDesc->getJson(properties);
                subscriptionPayload.put("app", app);
            } else {
                pbnjson::JValue apps = pbnjson::Array();
                AppDescriptionList::getInstance().toJson(apps, properties, isDevmode);
                subscriptionPayload.put("apps", apps);
            }
            request.respond(subscriptionPayload.stringify().c_str());
        }
        LSSubscriptionRelease(iter);
        iter = NULL;
    }

    void postRunning(bool devmode = false)
    {
        pbnjson::JValue subscriptionPayload = pbnjson::Object();
        std::string kind = devmode ? SUBSKEY_DEV_RUNNING : SUBSKEY_RUNNING;

        JValue running = pbnjson::Array();
        if (devmode) {
            RunningAppList::getInstance().toJson(running, true);
        } else {
            RunningAppList::getInstance().toJson(running);
        }

        subscriptionPayload.put("running", running);
        subscriptionPayload.put("returnValue", true);

        if (devmode) {
            Logger::logSubscriptionPost(getClassName(), __FUNCTION__, m_runningDev, subscriptionPayload);
            m_runningDev.post(subscriptionPayload.stringify().c_str());
        } else {
            Logger::logSubscriptionPost(getClassName(), __FUNCTION__, m_running, subscriptionPayload);
            m_running.post(subscriptionPayload.stringify().c_str());
        }
    }

private:
    static bool onAPICalled(LSHandle* sh, LSMessage* message, void* context);

    ApplicationManager();

    void registerApiHandler(const std::string& category, const std::string& method, LunaApiHandler handler)
    {
        std::string api = File::join(category, method);
        m_APIHandlers[api] = handler;
    }

    static LSMethod METHODS_ROOT[];
    static LSMethod METHODS_DEV[];

    std::map<std::string, LunaApiHandler> m_APIHandlers;

    LS::SubscriptionPoint m_listLaunchPointsPoint;
    LS::SubscriptionPoint m_listAppsPoint;
    LS::SubscriptionPoint m_listAppsCompactPoint;
    LS::SubscriptionPoint m_listDevAppsPoint;
    LS::SubscriptionPoint m_listDevAppsCompactPoint;
    LS::SubscriptionPoint m_getAppLifeEvents;
    LS::SubscriptionPoint m_getAppLifeStatus;
    LS::SubscriptionPoint m_running;
    LS::SubscriptionPoint m_runningDev;

    // TODO: Following should be deleted
    ApplicationManagerCompat m_compat1;
    ApplicationManagerCompat m_compat2;

    bool m_enableSubscription;

};

#endif
