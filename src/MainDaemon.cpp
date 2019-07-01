// Copyright (c) 2012-2018 LG Electronics, Inc.
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
#include <util/Logging.h>

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

    m_bootdPrerequisiteItem.setListener(this);
    m_configdPrerequisiteItem.setListener(this);
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
    SingletonNS::destroyAll();

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
        LOG_INFO(MSGID_SAM_LOADING_SEQ, 1,
                 PMLOGKS("status", "all_precondition_is_not_ready"), "");
        return;
    }

    if (ApplicationManager::getInstance().isServiceReady()) {
        LOG_INFO(MSGID_SAM_LOADING_SEQ, 1,
                 PMLOGKS("status", "applicaitonmanager_is_not_ready"), "");
        return;
    }

    LOG_INFO(MSGID_SAM_LOADING_SEQ, 1,
             PMLOGKS("status", "all_precondition_ready"), "");

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
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("subscribed", true);
    payload.put("returnValue", true);
    payload.put("launchPoints", launchPoints);

    ApplicationManager::getInstance().postListLaunchPoints(payload);
}

void MainDaemon::onLaunchPointChanged(const std::string& change, const pbnjson::JValue& launchPoint)
{
    pbnjson::JValue payload = launchPoint.duplicate();
    payload.put("returnValue", true);
    payload.put("subscribed", true);

    ApplicationManager::getInstance().postListLaunchPoints(payload);
}

void MainDaemon::onForegroundAppChanged(const std::string& appId)
{

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);
    payload.put("subscribed", true);
    payload.put(LOG_KEY_APPID, appId);
    payload.put("windowId", "");
    payload.put("processId", "");

    LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2,
             PMLOGKS("skey", SUBSKEY_FOREGROUND_INFO),
             PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()), "");

    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(),
    SUBSKEY_FOREGROUND_INFO, payload.stringify().c_str(), NULL)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS(LOG_KEY_TYPE, "subscriptionreply"),
                  PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }
}

void MainDaemon::onExtraForegroundInfoChanged(const pbnjson::JValue& foreground_info)
{
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);
    payload.put("subscribed", true);
    payload.put("foregroundAppInfo", foreground_info);

    LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2,
             PMLOGKS("skey", SUBSKEY_FOREGROUND_INFO_EX),
             PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()), "");
    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(),
                             SUBSKEY_FOREGROUND_INFO_EX,
                             payload.stringify().c_str(),
                             NULL)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS(LOG_KEY_TYPE, "subscriptionreply"),
                  PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }
}

void MainDaemon::onLifeCycleEventGenarated(const pbnjson::JValue& event)
{
    pbnjson::JValue payload = event.duplicate();
    payload.put("returnValue", true);

    LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2,
             PMLOGKS("skey", SUBSKEY_GET_APP_LIFE_EVENTS),
             PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()), "");
    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(),
                             SUBSKEY_GET_APP_LIFE_EVENTS,
                             payload.stringify().c_str(),
                             NULL)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS(LOG_KEY_TYPE, "subscriptionreply"),
                  PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }
}

void MainDaemon::onListAppsChanged(const pbnjson::JValue& apps, const std::vector<std::string>& changes, bool dev)
{
    std::string subs_key = dev ? SUBSKEY_DEV_LIST_APPS : SUBSKEY_LIST_APPS;
    std::string subs_key4compact = dev ? SUBSKEY_DEV_LIST_APPS_COMPACT : SUBSKEY_LIST_APPS_COMPACT;
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);
    payload.put("subscribed", true);
    payload.put("apps", apps);

    // reply for clients wanted full properties
    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(),
                             subs_key.c_str(),
                             payload.stringify().c_str(),
                             NULL)) {
        LOG_WARNING(MSGID_LSCALL_ERR, 3,
                    PMLOGKS(LOG_KEY_TYPE, "subscription_reply"),
                    PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                    PMLOGKFV(LOG_KEY_LINE, "%d", __LINE__), "");
    }

    // reply for clients wanted partial properties
    LSSubscriptionIter *iter = NULL;
    if (!LSSubscriptionAcquire(ApplicationManager::getInstance().get(), subs_key4compact.c_str(), &iter, NULL))
        return;

    while (LSSubscriptionHasNext(iter)) {
        LSMessage* message = LSSubscriptionNext(iter);
        pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(message), std::string("applicationManager.listApps"));
        if (jmsg.isNull())
            continue;

        // not a clients wanted partial properties
        if (!jmsg.hasKey("properties") || !jmsg["properties"].isArray()) {
            continue;
        }

        pbnjson::JValue new_apps = pbnjson::Array();

        // id is required
        jmsg["properties"].append("id");
        if (!AppPackage::getSelectedPropsFromApps(apps, jmsg["properties"], new_apps)) {
            LOG_WARNING(MSGID_FAIL_GET_SELECTED_PROPS, 1,
                        PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
            continue;
        }

        payload.put("apps", new_apps);

        if (!LSMessageRespond(message, payload.stringify().c_str(), NULL)) {
            LOG_ERROR(MSGID_LSCALL_ERR, 3,
                      PMLOGKS(LOG_KEY_TYPE, "respond"),
                      PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                      PMLOGKFV(LOG_KEY_LINE, "%d", __LINE__), "");
        }
    }
    LSSubscriptionRelease(iter);
    iter = NULL;
}

void MainDaemon::onOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev)
{
    std::string subs_key = dev ? SUBSKEY_DEV_LIST_APPS : SUBSKEY_LIST_APPS;
    std::string subs_key4compact = dev ? SUBSKEY_DEV_LIST_APPS_COMPACT : SUBSKEY_LIST_APPS_COMPACT;
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);
    payload.put("subscribed", true);
    payload.put("change", change);
    payload.put("changeReason", reason);
    payload.put("app", app);

    // reply for clients wanted full properties
    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(), subs_key.c_str(), payload.stringify().c_str(), NULL)) {
        LOG_WARNING(MSGID_LSCALL_ERR, 3,
                    PMLOGKS(LOG_KEY_TYPE, "subscription_reply"),
                    PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                    PMLOGKFV(LOG_KEY_LINE, "%d", __LINE__), "");
    }

    // reply for clients wanted partial properties
    LSSubscriptionIter *iter = NULL;
    if (!LSSubscriptionAcquire(ApplicationManager::getInstance().get(), subs_key4compact.c_str(), &iter, NULL))
        return;

    while (LSSubscriptionHasNext(iter)) {
        LSMessage* message = LSSubscriptionNext(iter);
        pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(message), std::string("applicationManager.listApps"));

        if (jmsg.isNull())
            continue;

        // not a clients wanted partial properties
        if (!jmsg.hasKey("properties") || !jmsg["properties"].isArray()) {
            continue;
        }

        pbnjson::JValue new_props = pbnjson::Object();
        // id is required
        jmsg["properties"].append("id");
        if (!AppPackage::getSelectedPropsFromAppInfo(app, jmsg["properties"], new_props)) {
            LOG_WARNING(MSGID_FAIL_GET_SELECTED_PROPS, 1,
                        PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
            continue;
        }

        payload.put("app", new_props);

        if (!LSMessageRespond(message, payload.stringify().c_str(), NULL)) {
            LOG_ERROR(MSGID_LSCALL_ERR, 3,
                      PMLOGKS(LOG_KEY_TYPE, "respond"),
                      PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                      PMLOGKFV(LOG_KEY_LINE, "%d", __LINE__), "");
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

    pbnjson::JValue payload = pbnjson::Object();
    std::string str_event = AppPackageManager::getInstance().toString(event);

    switch (event) {
    case AppStatusChangeEvent::AppStatusChangeEvent_Installed:
    case AppStatusChangeEvent::STORAGE_ATTACHED:
    case AppStatusChangeEvent::UPDATE_COMPLETED:
        payload.put("status", "launchable");
        payload.put("exist", true);
        payload.put("launchable", true);
        break;

    case AppStatusChangeEvent::STORAGE_DETACHED:
    case AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled:
        payload.put("status", "notExist");
        payload.put("exist", false);
        payload.put("launchable", false);
        break;

    default:
        return;
    }

    payload.put(LOG_KEY_APPID, app_desc->getAppId());
    payload.put("event", str_event);
    payload.put("returnValue", true);

    std::string subs_key = "getappstatus#" + app_desc->getAppId() + "#N";
    std::string subs_key_w_appinfo = "getappstatus#" + app_desc->getAppId() + "#Y";
    std::string str_payload = payload.stringify();

    switch (event) {
    case AppStatusChangeEvent::AppStatusChangeEvent_Installed:
    case AppStatusChangeEvent::STORAGE_ATTACHED:
    case AppStatusChangeEvent::UPDATE_COMPLETED:
        payload.put("appInfo", app_desc->toJValue());
        break;

    default:
        break;
    }

    std::string str_payload_w_appinfo = payload.stringify();

    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(), subs_key.c_str(), str_payload.c_str(), NULL)) {

        LOG_WARNING(MSGID_SUBSCRIPTION_REPLY_ERR, 3,
                    PMLOGKS("key", subs_key.c_str()),
                    PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                    PMLOGKFV(LOG_KEY_LINE, "%d", __LINE__), "");
    }

    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(), subs_key_w_appinfo.c_str(), str_payload_w_appinfo.c_str(), NULL)) {
        LOG_WARNING(MSGID_SUBSCRIPTION_REPLY_ERR, 3,
                    PMLOGKS("key", subs_key_w_appinfo.c_str()),
                    PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                    PMLOGKFV(LOG_KEY_LINE, "%d", __LINE__), "");
    }
}
