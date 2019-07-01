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

#include <bus/service/ApplicationManager.h>
#include "launchpoint/LaunchPointManager.h"
#include "lifecycle/LifecycleManager.h"
#include <pbnjson.hpp>
#include <setting/Settings.h>
#include <util/Logging.h>
#include <util/LSUtils.h>
#include <string>
#include <vector>
#include "SchemaChecker.h"
#include "util/JValueUtil.h"

const char* ApplicationManager::API_CATEGORY_ROOT = "/";
const char* ApplicationManager::API_CATEGORY_DEV = "/dev";
const char* ApplicationManager::API_LAUNCH = "launch";
const char* ApplicationManager::API_PAUSE = "pause";
const char* ApplicationManager::API_CLOSE_BY_APPID = "closeByAppId";
const char* ApplicationManager::API_RUNNING = "running";
const char* ApplicationManager::API_GET_APP_LIFE_EVENTS ="getAppLifeEvents";
const char* ApplicationManager::API_GET_APP_LIFE_STATUS = "getAppLifeStatus";
const char* ApplicationManager::API_GET_FOREGROUND_APPINFO = "getForegroundAppInfo";
const char* ApplicationManager::API_LOCK_APP = "lockApp";
const char* ApplicationManager::API_REGISTER_APP = "registerApp";
const char* ApplicationManager::API_LIST_APPS = "listApps";
const char* ApplicationManager::API_GET_APP_STATUS = "getAppStatus";
const char* ApplicationManager::API_GET_APP_INFO = "getAppInfo";
const char* ApplicationManager::API_GET_APP_BASE_PATH = "getAppBasePath";
const char* ApplicationManager::API_ADD_LAUNCHPOINT = "addLaunchPoint";
const char* ApplicationManager::API_UPDATE_LAUNCHPOINT = "updateLaunchPoint";
const char* ApplicationManager::API_REMOVE_LAUNCHPOINT = "removeLaunchPoint";
const char* ApplicationManager::API_MOVE_LAUNCHPOINT = "moveLaunchPoint";
const char* ApplicationManager::API_LIST_LAUNCHPOINTS = "listLaunchPoints";

LSMethod ApplicationManager::ROOT_METHOD[] = {
    { API_LAUNCH,                   ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_PAUSE,                    ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_CLOSE_BY_APPID,           ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_RUNNING,                  ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_GET_APP_LIFE_EVENTS,      ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_GET_APP_LIFE_STATUS,      ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_GET_FOREGROUND_APPINFO,   ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_LOCK_APP,                 ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_REGISTER_APP,             ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },

    // core: package
    { API_LIST_APPS,                ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_GET_APP_STATUS,           ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_GET_APP_INFO,             ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_GET_APP_BASE_PATH,        ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },

    // core: launchpoint
    { API_ADD_LAUNCHPOINT,          ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_UPDATE_LAUNCHPOINT,       ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_REMOVE_LAUNCHPOINT,       ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_MOVE_LAUNCHPOINT,         ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_LIST_LAUNCHPOINTS,        ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { 0, 0, LUNA_METHOD_FLAGS_NONE }
};
LSMethod ApplicationManager::ROOT_METHOD_DEV[] = {
    { API_CLOSE_BY_APPID,           ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_LIST_APPS,                ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { API_RUNNING,                  ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { 0, 0, LUNA_METHOD_FLAGS_NONE }
};
std::map<std::string, LunaApiHandler> ApplicationManager::s_APIHandlerMap;

bool ApplicationManager::onAPICalled(LSHandle* lshandle, LSMessage* lsmsg, void* ctx)
{
    LS::Message request(lsmsg);
    LunaApiHandler handler;
    LunaTaskPtr task = nullptr;
    string errorText = "";
    int errorCode = 0;

    if (!SchemaChecker::getInstance().checkRequest(request)) {
        errorCode = API_ERR_CODE_INVALID_PAYLOAD;
        errorText = "invalid parameters";
        goto Done;
    }

    task = std::make_shared<LunaTask>(lshandle, request, lsmsg);
    if (!task) {
        errorCode = API_ERR_CODE_GENERAL;
        errorText = "memory alloc fail";
        goto Done;
    }

    if (ApplicationManager::getInstance().s_APIHandlerMap.find(request.getKind()) != ApplicationManager::getInstance().s_APIHandlerMap.end())
        handler = ApplicationManager::getInstance().s_APIHandlerMap[request.getKind()];

    if (!handler) {
        errorCode = API_ERR_CODE_DEPRECATED;
        errorText = "deprecated method";
        goto Done;
    }

    if (!LaunchPointManager::getInstance().ready() ||
        !ApplicationManager::getInstance().isServiceReady() ||
        AppPackageManager::getInstance().appScanner().isRunning()) {
        ApplicationManager::getInstance().m_pendingTasks.push_back(task);
        task->writeInfoLog("pending");
        goto Done;
    }
    task->writeInfoLog("received_request");
    handler(task);

Done:
    if (!errorText.empty()) {
        if (task) {
            task->writeErrorLog(errorText);
        }

        pbnjson::JValue responsePayload = pbnjson::Object();
        responsePayload.put("returnValue", false);
        responsePayload.put("errorText", errorText);
        responsePayload.put("errorCode", errorCode);
        request.respond(responsePayload.stringify().c_str());
    }
    return true;
}

ApplicationManager::ApplicationManager()
    : LS::Handle(LS::registerService("com.webos.applicationManager")),
      m_compat1("com.webos.service.applicationmanager"),
      m_compat2("com.webos.service.applicationManager"),
      m_serviceReady(false)
{
    this->registerCategory(API_CATEGORY_ROOT, ROOT_METHOD, nullptr, nullptr);
    m_compat1.registerCategory(API_CATEGORY_ROOT, ROOT_METHOD, nullptr, nullptr);
    m_compat2.registerCategory(API_CATEGORY_ROOT, ROOT_METHOD, nullptr, nullptr);

    m_listLaunchPointsPoint.setServiceHandle(this);
    m_listAppsPoint.setServiceHandle(this);
    m_listAppsCompactPoint.setServiceHandle(this);
    m_listDevAppsPoint.setServiceHandle(this);
    m_listDevAppsCompactPoint.setServiceHandle(this);
}

ApplicationManager::~ApplicationManager()
{
}

void ApplicationManager::exportAPI()
{
}

void ApplicationManager::exportDevAPI()
{
    if (!SettingsImpl::getInstance().m_isDevMode) {
        return;
    }

    this->registerCategory(API_CATEGORY_DEV, ROOT_METHOD_DEV, nullptr, nullptr);
    m_compat1.registerCategory(API_CATEGORY_DEV, ROOT_METHOD_DEV, nullptr, nullptr);
    m_compat2.registerCategory(API_CATEGORY_DEV, ROOT_METHOD_DEV, nullptr, nullptr);
}

bool ApplicationManager::attach(GMainLoop* gml)
{
    attachToLoop(gml);
    m_compat1.attachToLoop(gml);
    m_compat2.attachToLoop(gml);
    return true;
}

void ApplicationManager::detach()
{
    s_APIHandlerMap.clear();

    detach();
    m_compat1.detach();
    m_compat2.detach();
}

void ApplicationManager::onServiceReady()
{
    exportDevAPI();
    EventServiceReady();
}

void ApplicationManager::registerApiHandler(const std::string& category, const std::string& method, LunaApiHandler handler)
{
    std::string api = category + method;
    s_APIHandlerMap[api] = handler;
}

void ApplicationManager::addLaunchPoint(LunaTaskPtr task)
{
    bool result = false;
    std::string appId;
    std::string launchPointId;
    std::string errorText;
    LaunchPointPtr launchPointPtr = NULL;

    launchPointPtr = LaunchPointManager::getInstance().addLP(LPMAction::API_ADD, task->getRequestPayload(), errorText);

    if (launchPointPtr != NULL) {
        result = true;
        appId = launchPointPtr->Id();
        launchPointId = launchPointPtr->getLunchPointId();
    }

    LOG_NORMAL(NLID_LAUNCH_POINT_ADDED, 4,
               PMLOGKS("caller", task->caller().c_str()),
               PMLOGKS("status", result?"done":"fail"),
               PMLOGKS(LOG_KEY_APPID, appId.c_str()),
               PMLOGKS("lp_id", launchPointId.c_str()), "");

    if (!result) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, errorText);
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("launchPointId", launchPointId);

    task->replyResult(payload);
}

void ApplicationManager::updateLaunchPoint(LunaTaskPtr task)
{
    bool result = false;
    std::string appId;
    std::string launchPointId;
    std::string errorText;
    LaunchPointPtr launchPointPtr = NULL;

    launchPointPtr = LaunchPointManager::getInstance().updateLP(LPMAction::API_UPDATE, task->getRequestPayload(), errorText);

    if (launchPointPtr != NULL) {
        result = true;
        appId = launchPointPtr->Id();
        launchPointId = launchPointPtr->getLunchPointId();
    }

    LOG_INFO(MSGID_LAUNCH_POINT_UPDATED, 4,
             PMLOGKS("caller", task->caller().c_str()),
             PMLOGKS("status", (result ? "done" : "failed")),
             PMLOGKS(LOG_KEY_APPID, appId.c_str()),
             PMLOGKS("lp_id", launchPointId.c_str()), "");

    if (!result)
        task->setError(API_ERR_CODE_GENERAL, errorText);

    task->reply();
}

void ApplicationManager::removeLaunchPoint(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getRequestPayload();

    std::string lpid = jmsg["launchPointId"].asString();
    std::string errorText;

    LaunchPointManager::getInstance().removeLP(task->getRequestPayload(), errorText);

    LOG_INFO(MSGID_LAUNCH_POINT_REMOVED, 3,
             PMLOGKS("caller", task->caller().c_str()),
             PMLOGKS("status", (errorText.empty() ? "done" : "failed")),
             PMLOGKS("lp_id", lpid.c_str()), "");

    if (!errorText.empty())
        task->setError(API_ERR_CODE_GENERAL, errorText);

    task->reply();
}

void ApplicationManager::moveLaunchPoint(LunaTaskPtr task)
{
    bool result = false;
    std::string appId;
    std::string launchPointId;
    std::string errorText;
    LaunchPointPtr launchPointPtr = NULL;

    launchPointPtr = LaunchPointManager::getInstance().moveLP(LPMAction::API_MOVE, task->getRequestPayload(), errorText);

    if (launchPointPtr != NULL) {
        result = true;
        appId = launchPointPtr->Id();
        launchPointId = launchPointPtr->getLunchPointId();
    }

    LOG_INFO(MSGID_LAUNCH_POINT_MOVED, 4,
             PMLOGKS("caller", task->caller().c_str()),
             PMLOGKS("status", (result?"done":"failed")),
             PMLOGKS(LOG_KEY_APPID, appId.c_str()),
             PMLOGKS("lp_id", launchPointId.c_str()), "");

    if (!errorText.empty())
        task->setError(API_ERR_CODE_GENERAL, errorText);

    task->reply();
}

void ApplicationManager::listLaunchPoints(LunaTaskPtr task)
{
    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue launchPoints = pbnjson::Array();
    bool subscribed = false;

    LaunchPointManager::getInstance().convertLPsToJson(launchPoints);


    if (task->getMessage().isSubscription())
        ApplicationManager::getInstance().m_listLaunchPointsPoint.subscribe(task->getMessage());

    payload.put("subscribed", subscribed);
    payload.put("launchPoints", launchPoints);

    LOG_INFO(MSGID_LAUNCH_POINT_REQUEST, 2,
             PMLOGKS("STATUS", "done"),
             PMLOGKS("CALLER", task->caller().c_str()),
             "reply listLaunchPoint");

    task->replyResult(payload);
}

void ApplicationManager::launch(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getRequestPayload();

    std::string appId;
    if (!JValueUtil::getValue(jmsg, "id", appId)) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "App ID is not specified");
        return;
    }

    std::string display;
    if (!JValueUtil::getValue(jmsg, "display", display)) {
        display = ""; // TODO default displayId
    }

    std::string mode;
    if (!JValueUtil::getValue(jmsg, "params", "reason", mode)) {
        mode = "normal";
    }
    LOG_NORMAL(NLID_APP_LAUNCH_BEGIN, 3,
               PMLOGKS(LOG_KEY_APPID, appId.c_str()),
               PMLOGKS("caller_id", task->caller().c_str()),
               PMLOGKS("mode", mode.c_str()), "");

    LifecycleTaskPtr lifeCycleTask = std::make_shared<LifecycleTask>(task);
    lifeCycleTask->setAppId(appId);
    lifeCycleTask->setDisplay(display);

    LifecycleManager::getInstance().launch(lifeCycleTask);
}

void ApplicationManager::pause(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getRequestPayload();

    std::string appId = jmsg["id"].asString();
    if (appId.length() == 0) {
        pbnjson::JValue payload = pbnjson::Object();
        task->replyResultWithError(API_ERR_CODE_GENERAL, "App ID is not specified");
        return;
    }

    LifecycleTaskPtr lifecycleTask = std::make_shared<LifecycleTask>(task);
    lifecycleTask->setAppId(appId);
    LifecycleManager::getInstance().pause(lifecycleTask);
}

void ApplicationManager::closeByAppId(LunaTaskPtr task)
{
    LifecycleTaskPtr lifecycleTask = std::make_shared<LifecycleTask>(task);
    LifecycleManager::getInstance().close(lifecycleTask);
}

void ApplicationManager::closeByAppIdForDev(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getRequestPayload();
    std::string appId = jmsg["id"].asString();
    AppPackagePtr appPackagePtr = AppPackageManager::getInstance().getAppById(appId);

    if (AppTypeByDir::AppTypeByDir_Dev != appPackagePtr->getTypeByDir()) {
        LOG_WARNING(MSGID_APPCLOSE_ERR, 1,
                    PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                    "only dev apps should be closed in devmode");
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Only Dev app should be closed using /dev category_API");
        return;
    }

    LifecycleTaskPtr lifecycleTask = std::make_shared<LifecycleTask>(task);
    LifecycleManager::getInstance().close(lifecycleTask);
}

void ApplicationManager::closeAllApps(LunaTaskPtr task)
{
    LifecycleTaskPtr lifecycleTask = std::make_shared<LifecycleTask>(task);
    LifecycleManager::getInstance().closeAll(lifecycleTask);
    task->reply();
}

void ApplicationManager::running(LunaTaskPtr task)
{
    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue runningList = pbnjson::Array();
    RunningInfoManager::getInstance().getRunningList(runningList);

    payload.put("returnValue", true);
    payload.put("running", runningList);
    if (LSMessageIsSubscription(task->lsmsg())) {
        payload.put("subscribed", LSSubscriptionAdd(task->getLSHandle(), SUBSKEY_RUNNING, task->lsmsg(), NULL));
    }

    task->replyResult(payload);
}

void ApplicationManager::runningForDev(LunaTaskPtr task)
{
    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue runningList = pbnjson::Array();
    RunningInfoManager::getInstance().getRunningList(runningList, true);

    payload.put("returnValue", true);
    payload.put("running", runningList);
    if (LSMessageIsSubscription(task->lsmsg())) {
        payload.put("subscribed", LSSubscriptionAdd(task->getLSHandle(), SUBSKEY_DEV_RUNNING, task->lsmsg(), NULL));
    }

    task->replyResult(payload);
}

void ApplicationManager::getAppLifeEvents(LunaTaskPtr task)
{
    pbnjson::JValue payload = pbnjson::Object();

    if (LSMessageIsSubscription(task->lsmsg())) {
        if (!LSSubscriptionAdd(task->getLSHandle(), SUBSKEY_GET_APP_LIFE_EVENTS, task->lsmsg(), NULL)) {
            task->setError(API_ERR_CODE_GENERAL, "Subscription failed");
            payload.put("subscribed", false);
        } else {
            payload.put("subscribed", true);
        }
    } else {
        task->setError(API_ERR_CODE_GENERAL, "subscription is required");
        payload.put("subscribed", false);
    }

    task->replyResult(payload);
}

void ApplicationManager::getAppLifeStatus(LunaTaskPtr task)
{
    pbnjson::JValue payload = pbnjson::Object();

    if (LSMessageIsSubscription(task->lsmsg())) {
        if (!LSSubscriptionAdd(task->getLSHandle(), SUBSKEY_GET_APP_LIFE_STATUS, task->lsmsg(), NULL)) {
            task->setError(API_ERR_CODE_GENERAL, "Subscription failed");
            payload.put("subscribed", false);
        } else {
            payload.put("subscribed", true);
        }
    } else {
        task->setError(API_ERR_CODE_GENERAL, "subscription is required");
        payload.put("subscribed", false);
    }

    task->replyResult(payload);
}

void ApplicationManager::getForegroundAppInfo(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getRequestPayload();

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);

    if (jmsg["extraInfo"].asBool() == true) {
        payload.put("foregroundAppInfo", RunningInfoManager::getInstance().getForegroundInfo());
        if (LSMessageIsSubscription(task->lsmsg())) {
            payload.put("subscribed", LSSubscriptionAdd(task->getLSHandle(), SUBSKEY_FOREGROUND_INFO_EX, task->lsmsg(), NULL));
        }
    } else {
        payload.put(LOG_KEY_APPID, RunningInfoManager::getInstance().getForegroundAppId());
        payload.put("windowId", "");
        payload.put("processId", "");
        if (LSMessageIsSubscription(task->lsmsg())) {
            payload.put("subscribed", LSSubscriptionAdd(task->getLSHandle(), SUBSKEY_FOREGROUND_INFO, task->lsmsg(), NULL));
        }
    }

    task->replyResult(payload);
}

void ApplicationManager::lockApp(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getRequestPayload();

    std::string appId = jmsg["id"].asString();
    bool lock = jmsg["lock"].asBool();
    std::string errorText;

    // TODO: move this property into app info manager
    AppPackageManager::getInstance().lockAppForUpdate(appId, lock, errorText);

    if (!errorText.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, errorText);
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("id", appId);
    payload.put("locked", lock);

    task->replyResult(payload);
}

void ApplicationManager::registerApp(LunaTaskPtr task)
{
    if (0 == task->caller().length()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "empty_app_id"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        task->replyResultWithError(API_ERR_CODE_GENERAL, "cannot find caller id");
        return;
    }

    std::string error_text;
    LifecycleManager::getInstance().registerApp(task->caller(), task->lsmsg(), error_text);

    if (!error_text.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, error_text);
        return;
    }
}

void ApplicationManager::registerNativeApp(LunaTaskPtr task)
{
    if (0 == task->caller().length()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "empty_app_id"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        task->replyResultWithError(API_ERR_CODE_GENERAL, "cannot find caller id");
        return;
    }

    std::string errorText;
    LifecycleManager::getInstance().connectNativeApp(task->caller(), task->lsmsg(), errorText);
    if (!errorText.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, errorText);
        return;
    }
}

void ApplicationManager::notifyAlertClosed(LunaTaskPtr task)
{
    LifecycleManager::getInstance().handleBridgedLaunchRequest(task->getRequestPayload());
    task->reply();
}

void ApplicationManager::listApps(LunaTaskPtr task)
{
    const pbnjson::JValue& requestPayload = task->getRequestPayload();
    const std::map<std::string, AppPackagePtr>& allApps = AppPackageManager::getInstance().allApps();

    pbnjson::JValue responsePayload = pbnjson::Object();
    pbnjson::JValue apps = pbnjson::Array();

    for (auto it : allApps)
        apps.append(it.second->toJValue());

    bool is_full_list_client = true;
    if (requestPayload.hasKey("properties") && requestPayload["properties"].isArray()) {
        is_full_list_client = false;
        pbnjson::JValue apps_selected_info = pbnjson::Array();
        requestPayload["properties"].append("id"); // id is required
        (void) AppPackage::getSelectedPropsFromApps(apps, requestPayload["properties"], apps_selected_info);
        responsePayload.put("apps", apps_selected_info);
    } else {
        responsePayload.put("apps", apps);
    }

    if (task->getMessage().isSubscription()) {
        if (is_full_list_client) {
            responsePayload.put("subscribed", ApplicationManager::getInstance().m_listAppsPoint.subscribe(task->getMessage()));
        } else {
            responsePayload.put("subscribed", ApplicationManager::getInstance().m_listAppsCompactPoint.subscribe(task->getMessage()));
        }
    } else {
        responsePayload.put("subscribed", false);
    }

    task->replyResult(responsePayload);
}

void ApplicationManager::listAppsForDev(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getRequestPayload();
    const std::map<std::string, AppPackagePtr>& apps = AppPackageManager::getInstance().allApps();

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
        (void) AppPackage::getSelectedPropsFromApps(apps_info, jmsg["properties"], apps_selected_info);
        payload.put("apps", apps_selected_info);
    } else {
        payload.put("apps", apps_info);
    }

    if (task->getMessage().isSubscription()) {
        if (is_full_list_client) {
            payload.put("subscribed", ApplicationManager::getInstance().m_listDevAppsPoint.subscribe(task->getMessage()));
        } else {
            payload.put("subscribed", ApplicationManager::getInstance().m_listDevAppsCompactPoint.subscribe(task->getMessage()));
        }
    } else {
        payload.put("subscribed", false);
    }

    task->replyResult(payload);
}

void ApplicationManager::getAppStatus(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getRequestPayload();

    pbnjson::JValue payload = pbnjson::Object();
    std::string appId = jmsg[LOG_KEY_APPID].asString();
    bool requiredAppInfo = (jmsg.hasKey("appInfo") && jmsg["appInfo"].isBoolean() && jmsg["appInfo"].asBool()) ? true : false;

    if (appId.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified");
        return;
    }

    if (task->getMessage().isSubscription()) {
        std::string subs_key = "getappstatus#" + appId + "#" + (requiredAppInfo ? "Y" : "N");
        if (LSSubscriptionAdd(task->getLSHandle(), subs_key.c_str(), task->lsmsg(), NULL)) {
            payload.put("subscribed", true);
        } else {
            LOG_WARNING(MSGID_LUNA_SUBSCRIPTION, 2,
                        PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                        PMLOGKFV(LOG_KEY_LINE, "%d", __LINE__), "");
            payload.put("subscribed", false);
        }
    }

    // for first return (event: nothing)
    payload.put(LOG_KEY_APPID, appId);
    payload.put("event", "nothing");

    AppPackagePtr appDescPtr = AppPackageManager::getInstance().getAppById(appId);
    if (!appDescPtr) {
        payload.put("status", "notExist");
        payload.put("exist", false);
        payload.put("launchable", false);
    } else {
        payload.put("status", "launchable");
        payload.put("exist", true);
        payload.put("launchable", true);

        if (requiredAppInfo) {
            payload.put("appInfo", appDescPtr->toJValue());
        }
    }

    task->replyResult(payload);
}

void ApplicationManager::getAppInfo(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getRequestPayload();

    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue app_info = pbnjson::Object();
    std::string app_id = jmsg["id"].asString();

    if (app_id.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified");
        return;
    }

    AppPackagePtr app_desc = AppPackageManager::getInstance().getAppById(app_id);
    if (!app_desc) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified OR Unsupported Application Type: " + app_id);
        return;
    }

    if (jmsg.hasKey("properties") && jmsg["properties"].isArray()) {
        const pbnjson::JValue& origin_app = app_desc->toJValue();
        if (!AppPackage::getSelectedPropsFromAppInfo(origin_app, jmsg["properties"], app_info)) {
            task->replyResultWithError(API_ERR_CODE_GENERAL, "Fail to get selected properties from AppInfo: " + app_id);
            return;
        }
    } else {
        app_info = app_desc->toJValue();
    }

    payload.put("appInfo", app_info);
    payload.put(LOG_KEY_APPID, app_id);
    task->replyResult(payload);
}

void ApplicationManager::getAppBasePath(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getRequestPayload();

    pbnjson::JValue payload = pbnjson::Object();
    std::string app_id = jmsg[LOG_KEY_APPID].asString();

    if (app_id.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified");
        return;
    }
    if (task->caller() != app_id) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Not allowed. Allow only for the info of calling app itself.");
        return;
    }

    AppPackagePtr app_desc = AppPackageManager::getInstance().getAppById(app_id);
    if (!app_desc) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified: " + app_id);
        return;
    }

    payload.put(LOG_KEY_APPID, app_id);
    payload.put("basePath", app_desc->getMain());

    task->replyResult(payload);
}
