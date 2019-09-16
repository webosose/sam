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

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <memory>

#include <glib.h>
#include <boost/bind.hpp>
#include <bus/client/AppInstallService.h>
#include <bus/client/Bootd.h>
#include <bus/client/Configd.h>
#include <bus/client/LSM.h>
#include <bus/client/SettingService.h>
#include <bus/service/ApplicationManager.h>
#include <launchpoint/LaunchPointManager.h>
#include <lifecycle/LifecycleManager.h>
#include <MainDaemon.h>
#include <package/AppPackageManager.h>
#include <prerequisite/BootdPrerequisiteItem.h>
#include <prerequisite/ConfigdPrerequisiteItem.h>
#include <setting/Settings.h>

#define SUBSKEY_LIST_APPS             "listApps"
#define SUBSKEY_LIST_APPS_COMPACT     "listAppsCompact"
#define SUBSKEY_DEV_LIST_APPS         "listDevApps"
#define SUBSKEY_DEV_LIST_APPS_COMPACT "listDevAppsCompact"

MainDaemon::MainDaemon()
{
    m_mainLoop = g_main_loop_new(NULL, FALSE);

    // Load Settings (first!)
    SettingsImpl::getInstance().load(NULL);
    SettingsImpl::getInstance().loadSAMConf();

    // Load managers (lifecycle, package, launchpoint)
    AppPackageManager::getInstance().initialize();
    LifecycleManager::getInstance().initialize();
    LaunchPointManager::getInstance().initialize();
    SettingService::getInstance().initialize();   // load locale info

    // service attach
    ApplicationManager::getInstance().exportAPI();
    ApplicationManager::getInstance().attach(m_mainLoop);

    Configd::getInstance().initialize();
    Bootd::getInstance().initialize();
    LSM::getInstance().initialize();
    AppInstallService::getInstance().initialize();

    m_bootdPrerequisiteItem.EventItemStatusChanged.connect(boost::bind(&MainDaemon::onPrerequisiteItemStatusChanged, this, _1));
    m_configdPrerequisiteItem.EventItemStatusChanged.connect(boost::bind(&MainDaemon::onPrerequisiteItemStatusChanged, this, _1));

    m_bootdPrerequisiteItem.start();
    m_configdPrerequisiteItem.start();

    // to check first app launch finished
    LifecycleManager::getInstance().EventLaunchingFinished.connect(boost::bind(&MainDaemon::onLaunchingFinished, this, _1));
    AppPackageManager::getInstance().scanInitialApps();

    LaunchPointManager::getInstance().EventLaunchPointListChanged.connect(boost::bind(&MainDaemon::onLaunchPointsListChanged, this, _1));
    LaunchPointManager::getInstance().EventLaunchPointChanged.connect(boost::bind(&MainDaemon::onLaunchPointChanged, this, _1, _2));

    LifecycleManager::getInstance().EventForegroundAppChanged.connect(boost::bind(&MainDaemon::onForegroundAppChanged, this, _1));
    LifecycleManager::getInstance().EventForegroundExtraInfoChanged.connect(boost::bind(&MainDaemon::onExtraForegroundInfoChanged, this, _1));
    LifecycleManager::getInstance().EventLifecycle.connect(boost::bind(&MainDaemon::onLifeCycleEventGenarated, this, _1));

    AppPackageManager::getInstance().EventListAppsChanged.connect(boost::bind(&MainDaemon::onListAppsChanged, this, _1, _2, _3));
    AppPackageManager::getInstance().EventOneAppChanged.connect(boost::bind(&MainDaemon::onOneAppChanged, this, _1, _2, _3, _4));
    AppPackageManager::getInstance().EventAppStatusChanged.connect(boost::bind(&MainDaemon::onAppStatusChanged, this, _1, _2));
}

MainDaemon::~MainDaemon()
{
    ApplicationManager::getInstance().detach();
    //SingletonNS::destroyAll();

    if (m_mainLoop) {
        g_main_loop_unref(m_mainLoop);
    }
}

void MainDaemon::start()
{
    g_main_loop_run(m_mainLoop);
}

void MainDaemon::stop()
{
    if (m_mainLoop)
        g_main_loop_quit(m_mainLoop);
}

void MainDaemon::onPrerequisiteItemStatusChanged(PrerequisiteItem* item)
{
    if (m_configdPrerequisiteItem.getStatus() != PrerequisiteItemStatus::PASSED &&
        m_bootdPrerequisiteItem.getStatus() != PrerequisiteItemStatus::PASSED) {
        Logger::info(getClassName(), __FUNCTION__, "", "all_precondition_is_not_ready");
        return;
    }

    if (ApplicationManager::getInstance().isServiceReady()) {
        Logger::info(getClassName(), __FUNCTION__, "", "applicaitonmanager_is_not_ready");
        return;
    }

    Logger::info(getClassName(), __FUNCTION__, "", "all_precondition_ready");

    SettingsImpl::getInstance().onRestLoad();
    SettingService::getInstance().onRestInit();
    AppPackageManager::getInstance().startPostInit();

    ApplicationManager::getInstance().setServiceStatus(true);
    ApplicationManager::getInstance().onServiceReady();
}

void MainDaemon::onLaunchingFinished(LaunchAppItemPtr item)
{
    if (item->getErrorText().empty())
        LifecycleManager::getInstance().setLastLoadingApp(item->getAppId());
}

void MainDaemon::onLaunchPointsListChanged(const pbnjson::JValue& launchPoints)
{
    pbnjson::JValue subscriptionPayload = pbnjson::Object();
    subscriptionPayload.put("subscribed", true);
    subscriptionPayload.put("returnValue", true);
    subscriptionPayload.put("launchPoints", launchPoints);

    ApplicationManager::getInstance().postListLaunchPoints(subscriptionPayload);
}

void MainDaemon::onLaunchPointChanged(const std::string& change, const pbnjson::JValue& launchPoint)
{
    pbnjson::JValue subscriptionPayload = launchPoint.duplicate();
    subscriptionPayload.put("returnValue", true);
    subscriptionPayload.put("subscribed", true);

    ApplicationManager::getInstance().postListLaunchPoints(subscriptionPayload);
}

void MainDaemon::onForegroundAppChanged(const std::string& appId)
{

    pbnjson::JValue subscriptionPayload = pbnjson::Object();
    subscriptionPayload.put("returnValue", true);
    subscriptionPayload.put("subscribed", true);
    subscriptionPayload.put(Logger::LOG_KEY_APPID, appId);
    subscriptionPayload.put("windowId", "");
    subscriptionPayload.put("processId", "");

    Logger::info(getClassName(), __FUNCTION__, SUBSKEY_FOREGROUND_INFO, subscriptionPayload.stringify());

    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(),
                             SUBSKEY_FOREGROUND_INFO,
                             subscriptionPayload.stringify().c_str(),
                             NULL)) {
        Logger::error(getClassName(), __FUNCTION__, SUBSKEY_FOREGROUND_INFO, subscriptionPayload.stringify());
        return;
    }
}

void MainDaemon::onExtraForegroundInfoChanged(const pbnjson::JValue& foreground_info)
{
    pbnjson::JValue subscriptionPayload = pbnjson::Object();
    subscriptionPayload.put("returnValue", true);
    subscriptionPayload.put("subscribed", true);
    subscriptionPayload.put("foregroundAppInfo", foreground_info);

    Logger::info(getClassName(), __FUNCTION__, "LS2", SUBSKEY_FOREGROUND_INFO_EX, subscriptionPayload.stringify());
    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(),
                             SUBSKEY_FOREGROUND_INFO_EX,
                             subscriptionPayload.stringify().c_str(),
                             NULL)) {
        Logger::error(getClassName(), __FUNCTION__, "LS2", "subscriptionreply", subscriptionPayload.stringify());
        return;
    }
}

void MainDaemon::onLifeCycleEventGenarated(const pbnjson::JValue& event)
{
    pbnjson::JValue subscriptionPayload = event.duplicate();
    subscriptionPayload.put("returnValue", true);

    Logger::info(getClassName(), __FUNCTION__, "LS2", SUBSKEY_GET_APP_LIFE_EVENTS, subscriptionPayload.stringify());
    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(),
                             SUBSKEY_GET_APP_LIFE_EVENTS,
                             subscriptionPayload.stringify().c_str(),
                             NULL)) {
        Logger::error(getClassName(), __FUNCTION__, "LS2", "subscriptionreply", subscriptionPayload.stringify());
        return;
    }
}

void MainDaemon::onListAppsChanged(const pbnjson::JValue& apps, const std::vector<std::string>& changes, bool dev)
{
    std::string subs_key = dev ? SUBSKEY_DEV_LIST_APPS : SUBSKEY_LIST_APPS;
    std::string subs_key4compact = dev ? SUBSKEY_DEV_LIST_APPS_COMPACT : SUBSKEY_LIST_APPS_COMPACT;
    pbnjson::JValue subscriptionPayload = pbnjson::Object();
    subscriptionPayload.put("returnValue", true);
    subscriptionPayload.put("subscribed", true);
    subscriptionPayload.put("apps", apps);

    // reply for clients wanted full properties
    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(),
                             subs_key.c_str(),
                             subscriptionPayload.stringify().c_str(),
                             NULL)) {
        Logger::warning(getClassName(), __FUNCTION__, "LS2", "subscriptionreply", subscriptionPayload.stringify());
    }

    // reply for clients wanted partial properties
    LSSubscriptionIter *iter = NULL;
    if (!LSSubscriptionAcquire(ApplicationManager::getInstance().get(), subs_key4compact.c_str(), &iter, NULL))
        return;

    while (LSSubscriptionHasNext(iter)) {
        LSMessage* message = LSSubscriptionNext(iter);
        pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message), JValueUtil::getSchema("applicationManager.listApps"));
        if (responsePayload.isNull())
            continue;

        // not a clients wanted partial properties
        if (!responsePayload.hasKey("properties") || !responsePayload["properties"].isArray()) {
            continue;
        }

        pbnjson::JValue new_apps = pbnjson::Array();

        // id is required
        responsePayload["properties"].append("id");
        if (!AppPackage::getSelectedPropsFromApps(apps, responsePayload["properties"], new_apps)) {
            Logger::warning(getClassName(), __FUNCTION__, "Failed to get selected props");
            continue;
        }

        subscriptionPayload.put("apps", new_apps);

        if (!LSMessageRespond(message, subscriptionPayload.stringify().c_str(), NULL)) {
            Logger::error(getClassName(), __FUNCTION__, "Failed to call LSMessageRespond");
        }
    }
    LSSubscriptionRelease(iter);
    iter = NULL;
}

void MainDaemon::onOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev)
{
    std::string subs_key = dev ? SUBSKEY_DEV_LIST_APPS : SUBSKEY_LIST_APPS;
    std::string subs_key4compact = dev ? SUBSKEY_DEV_LIST_APPS_COMPACT : SUBSKEY_LIST_APPS_COMPACT;
    pbnjson::JValue subscriptionPayload = pbnjson::Object();
    subscriptionPayload.put("returnValue", true);
    subscriptionPayload.put("subscribed", true);
    subscriptionPayload.put("change", change);
    subscriptionPayload.put("changeReason", reason);
    subscriptionPayload.put("app", app);

    // reply for clients wanted full properties
    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(), subs_key.c_str(), subscriptionPayload.stringify().c_str(), NULL)) {
        Logger::error(getClassName(), __FUNCTION__, "LS2", "subscriptionreply", subscriptionPayload.stringify());
    }

    // reply for clients wanted partial properties
    LSSubscriptionIter *iter = NULL;
    if (!LSSubscriptionAcquire(ApplicationManager::getInstance().get(), subs_key4compact.c_str(), &iter, NULL))
        return;

    while (LSSubscriptionHasNext(iter)) {
        LSMessage* message = LSSubscriptionNext(iter);
        pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message), JValueUtil::getSchema("applicationManager.listApps"));

        if (responsePayload.isNull())
            continue;

        // not a clients wanted partial properties
        if (!responsePayload.hasKey("properties") || !responsePayload["properties"].isArray()) {
            continue;
        }

        pbnjson::JValue new_props = pbnjson::Object();
        // id is required
        responsePayload["properties"].append("id");
        if (!AppPackage::getSelectedPropsFromAppInfo(app, responsePayload["properties"], new_props)) {
            Logger::warning(getClassName(), __FUNCTION__, "Failed to get selected props");
            continue;
        }

        subscriptionPayload.put("app", new_props);

        if (!LSMessageRespond(message, subscriptionPayload.stringify().c_str(), NULL)) {
            Logger::warning(getClassName(), __FUNCTION__, "Failed to call LSMessageRespond");
        }
    }

    LSSubscriptionRelease(iter);
    iter = NULL;
}

void MainDaemon::onAppStatusChanged(AppStatusChangeEvent event, AppPackagePtr app_desc)
{
    if (!app_desc) {
        // leave error
        return;
    }

    pbnjson::JValue subscriptionPayload = pbnjson::Object();
    std::string str_event = AppPackageManager::getInstance().toString(event);

    switch (event) {
    case AppStatusChangeEvent::AppStatusChangeEvent_Installed:
    case AppStatusChangeEvent::STORAGE_ATTACHED:
    case AppStatusChangeEvent::UPDATE_COMPLETED:
        subscriptionPayload.put("status", "launchable");
        subscriptionPayload.put("exist", true);
        subscriptionPayload.put("launchable", true);
        break;

    case AppStatusChangeEvent::STORAGE_DETACHED:
    case AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled:
        subscriptionPayload.put("status", "notExist");
        subscriptionPayload.put("exist", false);
        subscriptionPayload.put("launchable", false);
        break;

    default:
        return;
    }

    subscriptionPayload.put(Logger::LOG_KEY_APPID, app_desc->getAppId());
    subscriptionPayload.put("event", str_event);
    subscriptionPayload.put("returnValue", true);

    std::string subs_key = "getappstatus#" + app_desc->getAppId() + "#N";
    std::string subs_key_w_appinfo = "getappstatus#" + app_desc->getAppId() + "#Y";
    std::string str_payload = subscriptionPayload.stringify();

    switch (event) {
    case AppStatusChangeEvent::AppStatusChangeEvent_Installed:
    case AppStatusChangeEvent::STORAGE_ATTACHED:
    case AppStatusChangeEvent::UPDATE_COMPLETED:
        subscriptionPayload.put("appInfo", app_desc->toJValue());
        break;

    default:
        break;
    }

    std::string str_payload_w_appinfo = subscriptionPayload.stringify();

    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(), subs_key.c_str(), str_payload.c_str(), NULL)) {
        Logger::warning(getClassName(), __FUNCTION__, "LS2", subs_key);
    }

    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(), subs_key_w_appinfo.c_str(), str_payload_w_appinfo.c_str(), NULL)) {
        Logger::warning(getClassName(), __FUNCTION__, "LS2", subs_key_w_appinfo);
    }
}
