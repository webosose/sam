// Copyright (c) 2017-2020 LG Electronics, Inc.
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
#include <pbnjson.hpp>
#include <luna-service2/lunaservice.hpp>

#include "base/AppDescriptionList.h"
#include "base/LunaTask.h"
#include "base/LaunchPoint.h"
#include "base/LaunchPointList.h"
#include "base/RunningApp.h"
#include "base/RunningAppList.h"
#include "bus/service/compat/ApplicationManagerCompat.h"
#include "conf/SAMConf.h"
#include "interface/IClassName.h"
#include "interface/ISingleton.h"
#include "util/Logger.h"
#include "util/File.h"

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
    static const char* METHOD_CLOSE;
    static const char* METHOD_CLOSE_BY_APPID;
    static const char* METHOD_RUNNING;
    static const char* METHOD_GET_APP_LIFE_EVENTS;
    static const char* METHOD_GET_APP_LIFE_STATUS;
    static const char* METHOD_GET_FOREGROUND_APPINFO;
    static const char* METHOD_LOCK_APP;
    static const char* METHOD_REGISTER_APP;
    static const char* METHOD_REGISTER_NATIVE_APP;

    static const char* METHOD_LIST_APPS;
    static const char* METHOD_GET_APP_STATUS;
    static const char* METHOD_GET_APP_INFO;
    static const char* METHOD_GET_APP_BASE_PATH;

    static const char* METHOD_ADD_LAUNCHPOINT;
    static const char* METHOD_UPDATE_LAUNCHPOINT;
    static const char* METHOD_REMOVE_LAUNCHPOINT;
    static const char* METHOD_LIST_LAUNCHPOINTS;

    static const char* METHOD_MANAGER_INFO;

    virtual ~ApplicationManager();

    virtual bool attach(GMainLoop* gml);
    virtual void detach();

    // APIs
    void launch(LunaTaskPtr lunaTask);
    void pause(LunaTaskPtr lunaTask);
    void close(LunaTaskPtr lunaTask);
    void running(LunaTaskPtr lunaTask);
    void getAppLifeEvents(LunaTaskPtr lunaTask);
    void getAppLifeStatus(LunaTaskPtr lunaTask);
    void getForegroundAppInfo(LunaTaskPtr lunaTask);
    void lockApp(LunaTaskPtr lunaTask);
    void registerApp(LunaTaskPtr lunaTask);

    void listApps(LunaTaskPtr lunaTask);
    void getAppStatus(LunaTaskPtr lunaTask);
    void getAppInfo(LunaTaskPtr lunaTask);
    void getAppBasePath(LunaTaskPtr lunaTask);

    void addLaunchPoint(LunaTaskPtr lunaTask);
    void updateLaunchPoint(LunaTaskPtr lunaTask);
    void removeLaunchPoint(LunaTaskPtr lunaTask);
    void listLaunchPoints(LunaTaskPtr lunaTask);

    void managerInfo(LunaTaskPtr lunaTask);

    // Post
    void postGetAppLifeEvents(RunningApp& runningApp);
    void postGetAppLifeStatus(RunningApp& runningApp);
    void postGetAppStatus(AppDescriptionPtr appDesc, AppStatusEvent event);
    void postGetForegroundAppInfo(bool extraInfoOnly);
    void postListApps(AppDescriptionPtr appDesc, const string& change, const string& changeReason);
    void postListLaunchPoints(LaunchPointPtr launchPoint, string change);
    void postRunning(RunningAppPtr runningApp);

    // make
    void makeGetForegroundAppInfo(JValue& payload, bool extraInfo);
    void makeRunning(JValue& payload, bool isDevmode);

    void enablePosting()
    {
        if (m_enableSubscription)
            return;

        m_enableSubscription = true;
        postGetForegroundAppInfo(false);
        postRunning(nullptr);
        postListApps(nullptr, "", "");
        postListLaunchPoints(nullptr, "");
    }

    void disablePosting()
    {
        m_enableSubscription = false;
    }

private:
    static bool onAPICalled(LSHandle* sh, LSMessage* message, void* context);

    ApplicationManager();

    void registerApiHandler(const string& category, const string& method, LunaApiHandler handler)
    {
        string api = File::join(category, method);
        m_APIHandlers[api] = handler;
    }

    static LSMethod METHODS_ROOT[];
    static LSMethod METHODS_DEV[];

    map<string, LunaApiHandler> m_APIHandlers;

    LS::SubscriptionPoint* m_getAppLifeEvents;
    LS::SubscriptionPoint* m_getAppLifeStatus;
    LS::SubscriptionPoint* m_getForgroundAppInfo;
    LS::SubscriptionPoint* m_getForgroundAppInfoExtraInfo;
    LS::SubscriptionPoint* m_listLaunchPointsPoint;
    LS::SubscriptionPoint* m_listAppsPoint;
    LS::SubscriptionPoint* m_listAppsCompactPoint;
    LS::SubscriptionPoint* m_listDevAppsPoint;
    LS::SubscriptionPoint* m_listDevAppsCompactPoint;
    LS::SubscriptionPoint* m_running;
    LS::SubscriptionPoint* m_runningDev;

    bool m_enableSubscription;

    // TODO: Following should be deleted
    ApplicationManagerCompat m_compat1;
    ApplicationManagerCompat m_compat2;


};

#endif
