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

#include <boost/function.hpp>
#include <boost/signals2.hpp>
#include <bus/service/compat/ApplicationManagerCompat.h>
#include <luna-service2/lunaservice.hpp>
#include "../LunaTask.h"
#include <map>
#include <memory>
#include <pbnjson.hpp>
#include <util/JUtil.h>
#include <util/Singleton.h>
#include <string>

#define SUBSKEY_RUNNING              "running"
#define SUBSKEY_DEV_RUNNING          "dev_running"
#define SUBSKEY_FOREGROUND_INFO      "foregroundAppInfo"
#define SUBSKEY_FOREGROUND_INFO_EX   "foregroundAppInfoEx"
#define SUBSKEY_GET_APP_LIFE_EVENTS  "getAppLifeEvents"
#define SUBSKEY_GET_APP_LIFE_STATUS  "getAppLifeStatus"
#define SUBSKEY_ON_LAUNCH            "onLaunch"

using namespace LS;

typedef boost::function<void(LunaTaskPtr)> LunaApiHandler;

class ApplicationManager : public LS::Handle,
                           public Singleton<ApplicationManager> {
friend class Singleton<ApplicationManager> ;
public:
    ApplicationManager();
    virtual ~ApplicationManager();

    virtual void exportAPI();
    virtual void exportDevAPI();

    virtual bool attach(GMainLoop* gml);
    virtual void detach();

    void registerApiHandler(const std::string& category, const std::string& method, LunaApiHandler handler);

    void onServiceReady();
    void setServiceStatus(bool status)
    {
        m_serviceReady = status;
    }
    bool isServiceReady() const
    {
        return m_serviceReady;
    }

    void postListLaunchPoints(JValue& subscriptionPayload)
    {
        LOG_INFO(MSGID_LAUNCH_POINT_REPLY_SUBSCRIBER, 1,
                 PMLOGKS("status", "reply_lp_list_to_subscribers"), "");
        if (!m_listLaunchPointsPoint.post(subscriptionPayload.stringify().c_str())) {
            LOG_ERROR(MSGID_LSCALL_ERR, 2,
                      PMLOGKS(LOG_KEY_TYPE, "ls_subscription_reply"),
                      PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        }
    }

    void postListApps(JValue& subscriptionPayload)
    {
        m_listAppsPoint.post(subscriptionPayload.stringify().c_str());
    }

    void postListAppsCompact(JValue& subscriptionPayload)
    {
        m_listAppsCompactPoint.post(subscriptionPayload.stringify().c_str());
    }

    void postListDevApps(JValue& subscriptionPayload)
    {
        m_listDevAppsPoint.post(subscriptionPayload.stringify().c_str());
    }

    void postListDevAppsCompact(JValue& subscriptionPayload)
    {
        m_listDevAppsCompactPoint.post(subscriptionPayload.stringify().c_str());
    }

    void addLaunchPoint(LunaTaskPtr task);
    void updateLaunchPoint(LunaTaskPtr task);
    void removeLaunchPoint(LunaTaskPtr task);
    void moveLaunchPoint(LunaTaskPtr task);
    void listLaunchPoints(LunaTaskPtr task);
    void launch(LunaTaskPtr task);
    void pause(LunaTaskPtr task);
    void closeByAppId(LunaTaskPtr task);
    void closeByAppIdForDev(LunaTaskPtr task);
    void closeAllApps(LunaTaskPtr task);
    void running(LunaTaskPtr task);
    void runningForDev(LunaTaskPtr task);
    void getAppLifeEvents(LunaTaskPtr task);
    void getAppLifeStatus(LunaTaskPtr task);
    void getForegroundAppInfo(LunaTaskPtr task);
    void lockApp(LunaTaskPtr task);
    void registerApp(LunaTaskPtr task);
    void registerNativeApp(LunaTaskPtr task);
    void notifyAlertClosed(LunaTaskPtr task);
    void listApps(LunaTaskPtr task);
    void listAppsForDev(LunaTaskPtr task);
    void getAppStatus(LunaTaskPtr task);
    void getAppInfo(LunaTaskPtr task);
    void getAppBasePath(LunaTaskPtr task);

    static const char* API_CATEGORY_ROOT;
    static const char* API_CATEGORY_DEV;

    static const char* API_LAUNCH;
    static const char* API_PAUSE;
    static const char* API_CLOSE_BY_APPID;
    static const char* API_RUNNING;
    static const char* API_GET_APP_LIFE_EVENTS;
    static const char* API_GET_APP_LIFE_STATUS;
    static const char* API_GET_FOREGROUND_APPINFO;
    static const char* API_LOCK_APP;
    static const char* API_REGISTER_APP;
    static const char* API_LIST_APPS;
    static const char* API_GET_APP_STATUS;
    static const char* API_GET_APP_INFO;
    static const char* API_GET_APP_BASE_PATH;
    static const char* API_ADD_LAUNCHPOINT;
    static const char* API_UPDATE_LAUNCHPOINT;
    static const char* API_REMOVE_LAUNCHPOINT;
    static const char* API_MOVE_LAUNCHPOINT;
    static const char* API_LIST_LAUNCHPOINTS;

    boost::signals2::signal<void()> EventServiceReady;

private:
    static bool onAPICalled(LSHandle* lshandle, LSMessage* lsmsg, void* ctx);

    static LSMethod ROOT_METHOD[];
    static LSMethod ROOT_METHOD_DEV[];
    static std::map<std::string, LunaApiHandler> s_APIHandlerMap;

    LS::SubscriptionPoint m_listLaunchPointsPoint;
    LS::SubscriptionPoint m_listAppsPoint;
    LS::SubscriptionPoint m_listAppsCompactPoint;
    LS::SubscriptionPoint m_listDevAppsPoint;
    LS::SubscriptionPoint m_listDevAppsCompactPoint;

    std::vector<LunaTaskPtr> m_pendingTasks;

    // TODO: Following should be deleted
    ApplicationManagerCompat m_compat1;
    ApplicationManagerCompat m_compat2;

    bool m_serviceReady;
};

#endif
