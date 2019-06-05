// Copyright (c) 2017-2018 LG Electronics, Inc.
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

#include <bus/appmgr_service.h>
#include <bus/lunaservice_api.h>
#include <bus/package_luna_adapter.h>
#include <lifecycle/lifecycle_manager.h>
#include <util/logging.h>

#define SUBSKEY_LIST_APPS             "listApps"
#define SUBSKEY_LIST_APPS_COMPACT     "listAppsCompact"
#define SUBSKEY_DEV_LIST_APPS         "listDevApps"
#define SUBSKEY_DEV_LIST_APPS_COMPACT "listDevAppsCompact"

PackageLunaAdapter::PackageLunaAdapter()
{
}

PackageLunaAdapter::~PackageLunaAdapter()
{
}

void PackageLunaAdapter::PackageLunaAdapter::init()
{
    initLunaApiHandler();

    PackageManager::instance().appScanner().signalAppScanFinished.connect(boost::bind(&PackageLunaAdapter::onScanFinished, this, _1, _2));

    PackageManager::instance().signalListAppsChanged.connect(boost::bind(&PackageLunaAdapter::onListAppsChanged, this, _1, _2, _3));

    PackageManager::instance().signalOneAppChanged.connect(boost::bind(&PackageLunaAdapter::onOneAppChanged, this, _1, _2, _3, _4));

    PackageManager::instance().signalAppStatusChanged.connect(boost::bind(&PackageLunaAdapter::onAppStatusChanged, this, _1, _2));

    AppMgrService::instance().signalOnServiceReady.connect(std::bind(&PackageLunaAdapter::onReady, this));
}

void PackageLunaAdapter::PackageLunaAdapter::initLunaApiHandler()
{
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_LIST_APPS, "applicationManager.listApps", boost::bind(&PackageLunaAdapter::requestController, this, _1));
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_GET_APP_STATUS, "applicationManager.getAppStatus", boost::bind(&PackageLunaAdapter::requestController, this, _1));
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_GET_APP_INFO, "applicationManager.getAppInfo", boost::bind(&PackageLunaAdapter::requestController, this, _1));
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_GET_APP_BASE_PATH, "applicationManager.getAppBasePath", boost::bind(&PackageLunaAdapter::requestController, this, _1));

    // dev category
    AppMgrService::instance().registerApiHandler(API_CATEGORY_DEV, API_LIST_APPS, "applicationManager.listApps", boost::bind(&PackageLunaAdapter::requestController, this, _1));
}

void PackageLunaAdapter::requestController(LunaTaskPtr task)
{
    if (API_GET_APP_INFO == task->method()) {
        if (!AppMgrService::instance().isServiceReady()) {
            LOG_INFO(MSGID_API_REQUEST, 4,
                     PMLOGKS("category", task->category().c_str()),
                     PMLOGKS("method", task->method().c_str()),
                     PMLOGKS("status", "pending"),
                     PMLOGKS("caller", task->caller().c_str()),
                     "received message, but will handle later");
            m_pendingTasksOnReady.push_back(task);
            return;
        }
    } else {
        if (!AppMgrService::instance().isServiceReady() || PackageManager::instance().appScanner().isRunning()) {
            LOG_INFO(MSGID_API_REQUEST, 4,
                     PMLOGKS("category", task->category().c_str()),
                     PMLOGKS("method", task->method().c_str()),
                     PMLOGKS("status", "pending"),
                     PMLOGKS("caller", task->caller().c_str()),
                     "received message, but will handle later");
            m_pendingTasksOnScanner.push_back(task);
            return;
        }
    }

    handleRequest(task);
}

void PackageLunaAdapter::onReady()
{
    auto it = m_pendingTasksOnReady.begin();
    while (it != m_pendingTasksOnReady.end()) {
        handleRequest(*it);
        it = m_pendingTasksOnReady.erase(it);
    }
}

void PackageLunaAdapter::onScanFinished(ScanMode mode, const AppDescMaps& scannced_apps)
{
    auto it = m_pendingTasksOnScanner.begin();
    while (it != m_pendingTasksOnScanner.end()) {
        handleRequest(*it);
        it = m_pendingTasksOnScanner.erase(it);
    }
}

void PackageLunaAdapter::handleRequest(LunaTaskPtr task)
{
    if (API_CATEGORY_GENERAL == task->category()) {
        if (API_LIST_APPS == task->method())
            listApps(task);
        else if (API_GET_APP_STATUS == task->method())
            getAppStatus(task);
        else if (API_GET_APP_INFO == task->method())
            getAppInfo(task);
        else if (API_GET_APP_BASE_PATH == task->method())
            getAppBasePath(task);
    } else if (API_CATEGORY_DEV == task->category()) {
        if (API_LIST_APPS == task->method())
            listAppsForDev(task);
    }
}

void PackageLunaAdapter::listApps(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();
    const std::map<std::string, AppDescPtr>& apps = PackageManager::instance().allApps();

    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue apps_info = pbnjson::Array();

    for (auto it : apps)
        apps_info.append(it.second->toJValue());

    bool is_full_list_client = true;
    if (jmsg.hasKey("properties") && jmsg["properties"].isArray()) {
        is_full_list_client = false;
        pbnjson::JValue apps_selected_info = pbnjson::Array();
        jmsg["properties"].append("id"); // id is required
        (void) AppDescription::getSelectedPropsFromApps(apps_info, jmsg["properties"], apps_selected_info);
        payload.put("apps", apps_selected_info);
    } else {
        payload.put("apps", apps_info);
    }

    if (LSMessageIsSubscription(task->lsmsg())) {
        payload.put("subscribed", LSSubscriptionAdd(task->lshandle(), (is_full_list_client ? SUBSKEY_LIST_APPS : SUBSKEY_LIST_APPS_COMPACT), task->lsmsg(), NULL));
    } else {
        payload.put("subscribed", false);
    }

    task->ReplyResult(payload);
}

void PackageLunaAdapter::listAppsForDev(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();
    const std::map<std::string, AppDescPtr>& apps = PackageManager::instance().allApps();

    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue apps_info = pbnjson::Array();

    for (auto it : apps) {
        if (AppTypeByDir::AppTypeByDir_Dev == it.second->getTypeByDir()) {
            apps_info.append(it.second->toJValue());
        }
    }

    payload.put("returnValue", true);

    bool is_full_list_client = true;
    if (jmsg.hasKey("properties") && jmsg["properties"].isArray()) {
        is_full_list_client = false;
        pbnjson::JValue apps_selected_info = pbnjson::Array();
        jmsg["properties"].append("id"); // id is required
        (void) AppDescription::getSelectedPropsFromApps(apps_info, jmsg["properties"], apps_selected_info);
        payload.put("apps", apps_selected_info);
    } else {
        payload.put("apps", apps_info);
    }

    if (LSMessageIsSubscription(task->lsmsg())) {
        payload.put("subscribed", LSSubscriptionAdd(task->lshandle(), (is_full_list_client ? SUBSKEY_DEV_LIST_APPS : SUBSKEY_DEV_LIST_APPS_COMPACT), task->lsmsg(), NULL));
    } else {
        payload.put("subscribed", false);
    }

    task->ReplyResult(payload);
}

void PackageLunaAdapter::getAppStatus(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    pbnjson::JValue payload = pbnjson::Object();
    std::string app_id = jmsg["appId"].asString();
    bool require_app_info = (jmsg.hasKey("appInfo") && jmsg["appInfo"].isBoolean() && jmsg["appInfo"].asBool()) ? true : false;

    if (app_id.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified");
        return;
    }

    if (LSMessageIsSubscription(task->lsmsg())) {
        std::string subs_key = "getappstatus#" + app_id + "#" + (require_app_info ? "Y" : "N");
        if (LSSubscriptionAdd(task->lshandle(), subs_key.c_str(), task->lsmsg(), NULL)) {
            payload.put("subscribed", true);
        } else {
            LOG_WARNING(MSGID_LUNA_SUBSCRIPTION, 1, PMLOGKS("method", "getAppStatus"), "trace (%s:%d)", __FUNCTION__, __LINE__);
            payload.put("subscribed", false);
        }
    }

    // for first return (event: nothing)
    payload.put("appId", app_id);
    payload.put("event", "nothing");

    AppDescPtr app_desc = PackageManager::instance().getAppById(app_id);
    if (!app_desc) {
        payload.put("status", "notExist");
        payload.put("exist", false);
        payload.put("launchable", false);
    } else {
        payload.put("exist", true);

        if (app_desc->canExecute()) {
            payload.put("status", "launchable");
            payload.put("launchable", true);
        } else {
            payload.put("status", "updating");
            payload.put("launchable", false);
        }

        if (require_app_info) {
            payload.put("appInfo", app_desc->toJValue());
        }
    }

    task->ReplyResult(payload);
}

void PackageLunaAdapter::getAppInfo(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue app_info = pbnjson::Object();
    std::string app_id = jmsg["id"].asString();

    if (app_id.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified");
        return;
    }

    AppDescPtr app_desc = PackageManager::instance().getAppById(app_id);
    if (!app_desc) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified OR Unsupported Application Type: " + app_id);
        return;
    }

    if (jmsg.hasKey("properties") && jmsg["properties"].isArray()) {
        const pbnjson::JValue& origin_app = app_desc->toJValue();
        if (!AppDescription::getSelectedPropsFromAppInfo(origin_app, jmsg["properties"], app_info)) {
            task->replyResultWithError(API_ERR_CODE_GENERAL, "Fail to get selected properties from AppInfo: " + app_id);
            return;
        }
    } else {
        app_info = app_desc->toJValue();
    }

    payload.put("appInfo", app_info);
    payload.put("appId", app_id);
    task->ReplyResult(payload);
}

void PackageLunaAdapter::getAppBasePath(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    pbnjson::JValue payload = pbnjson::Object();
    std::string app_id = jmsg["appId"].asString();

    if (app_id.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified");
        return;
    }
    if (task->caller() != app_id) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Not allowed. Allow only for the info of calling app itself.");
        return;
    }

    AppDescPtr app_desc = PackageManager::instance().getAppById(app_id);
    if (!app_desc) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified: " + app_id);
        return;
    }

    payload.put("appId", app_id);
    payload.put("basePath", app_desc->entryPoint());

    task->ReplyResult(payload);
}

void PackageLunaAdapter::onListAppsChanged(const pbnjson::JValue& apps, const std::vector<std::string>& changes, bool dev)
{

    std::string subs_key = dev ? SUBSKEY_DEV_LIST_APPS : SUBSKEY_LIST_APPS;
    std::string subs_key4compact = dev ? SUBSKEY_DEV_LIST_APPS_COMPACT : SUBSKEY_LIST_APPS_COMPACT;
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);
    payload.put("subscribed", true);
    payload.put("apps", apps);

    // reply for clients wanted full properties
    if (!LSSubscriptionReply(AppMgrService::instance().serviceHandle(),
                             subs_key.c_str(),
                             payload.stringify().c_str(),
                             NULL)) {
        LOG_WARNING(MSGID_LSCALL_ERR, 1, PMLOGKS("type", "subscription_reply"), "%s: %d", __FUNCTION__, __LINE__);
    }

    // reply for clients wanted partial properties
    LSSubscriptionIter *iter = NULL;
    if (!LSSubscriptionAcquire(AppMgrService::instance().serviceHandle(), subs_key4compact.c_str(), &iter, NULL))
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
        if (!AppDescription::getSelectedPropsFromApps(apps, jmsg["properties"], new_apps)) {
            LOG_WARNING(MSGID_FAIL_GET_SELECTED_PROPS, 1, PMLOGKS("where", __FUNCTION__), "");
            continue;
        }

        payload.put("apps", new_apps);

        if (!LSMessageRespond(message, payload.stringify().c_str(), NULL)) {
            LOG_ERROR(MSGID_LSCALL_ERR, 1, PMLOGKS("type", "respond"), "%s: %d", __FUNCTION__, __LINE__);
        }
    }
    LSSubscriptionRelease(iter);
    iter = NULL;
}

void PackageLunaAdapter::onOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev)
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
    if (!LSSubscriptionReply(AppMgrService::instance().serviceHandle(), subs_key.c_str(), payload.stringify().c_str(), NULL)) {
        LOG_WARNING(MSGID_LSCALL_ERR, 1, PMLOGKS("type", "subscription_reply"), "%s: %d", __FUNCTION__, __LINE__);
    }

    // reply for clients wanted partial properties
    LSSubscriptionIter *iter = NULL;
    if (!LSSubscriptionAcquire(AppMgrService::instance().serviceHandle(), subs_key4compact.c_str(), &iter, NULL))
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
        if (!AppDescription::getSelectedPropsFromAppInfo(app, jmsg["properties"], new_props)) {
            LOG_WARNING(MSGID_FAIL_GET_SELECTED_PROPS, 1, PMLOGKS("where", __FUNCTION__), "");
            continue;
        }

        payload.put("app", new_props);

        if (!LSMessageRespond(message, payload.stringify().c_str(), NULL)) {
            LOG_ERROR(MSGID_LSCALL_ERR, 1, PMLOGKS("type", "respond"), "%s: %d", __FUNCTION__, __LINE__);
        }
    }

    LSSubscriptionRelease(iter);
    iter = NULL;
}

void PackageLunaAdapter::onAppStatusChanged(AppStatusChangeEvent event, AppDescPtr app_desc)
{

    if (!app_desc) {
        // leave error
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    std::string str_event = PackageManager::instance().toString(event);

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
        // leave error
        return;
    }

    payload.put("appId", app_desc->id());
    payload.put("event", str_event);
    payload.put("returnValue", true);

    std::string subs_key = "getappstatus#" + app_desc->id() + "#N";
    std::string subs_key_w_appinfo = "getappstatus#" + app_desc->id() + "#Y";
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

    if (!LSSubscriptionReply(AppMgrService::instance().serviceHandle(), subs_key.c_str(), str_payload.c_str(), NULL)) {

        LOG_WARNING(MSGID_SUBSCRIPTION_REPLY_ERR, 1,
                    PMLOGKS("key", subs_key.c_str()), "trace(%s:%d)", __FUNCTION__, __LINE__);
    }

    if (!LSSubscriptionReply(AppMgrService::instance().serviceHandle(), subs_key_w_appinfo.c_str(), str_payload_w_appinfo.c_str(), NULL)) {
        LOG_WARNING(MSGID_SUBSCRIPTION_REPLY_ERR, 1,
                    PMLOGKS("key", subs_key_w_appinfo.c_str()), "trace(%s:%d)", __FUNCTION__, __LINE__);
    }
}
