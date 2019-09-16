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

bool ApplicationManager::onAPICalled(LSHandle* sh, LSMessage* message, void* ctx)
{
    LS::Message request(message);
    LunaApiHandler handler;
    LunaTaskPtr task = nullptr;
    string errorText = "";
    int errorCode = 0;

    if (!SchemaChecker::getInstance().checkRequest(request)) {
        errorCode = Logger::API_ERR_CODE_INVALID_PAYLOAD;
        errorText = "invalid parameters";
        goto Done;
    }

    task = std::make_shared<LunaTask>(sh, request, message);
    if (!task) {
        errorCode = Logger::API_ERR_CODE_GENERAL;
        errorText = "memory alloc fail";
        goto Done;
    }

    if (ApplicationManager::getInstance().s_APIHandlerMap.find(request.getKind()) != ApplicationManager::getInstance().s_APIHandlerMap.end())
        handler = ApplicationManager::getInstance().s_APIHandlerMap[request.getKind()];

    if (!handler) {
        errorCode = Logger::API_ERR_CODE_DEPRECATED;
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

    Logger::info(getClassName(), __FUNCTION__, appId, launchPointPtr ? "success" : "failed");
    if (!result) {
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, errorText);
        return;
    }

    pbnjson::JValue responsePayload = pbnjson::Object();
    responsePayload.put("launchPointId", launchPointId);

    task->replyResult(responsePayload);
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

    Logger::info(getClassName(), __FUNCTION__, appId, launchPointPtr ? "success" : "failed");

    if (!result)
        task->setError(Logger::API_ERR_CODE_GENERAL, errorText);

    task->reply();
}

void ApplicationManager::removeLaunchPoint(LunaTaskPtr task)
{
    const pbnjson::JValue& responsePayload = task->getRequestPayload();

    std::string launchPointId = responsePayload["launchPointId"].asString();
    std::string errorText;

    LaunchPointManager::getInstance().removeLP(task->getRequestPayload(), errorText);

    Logger::info(getClassName(), __FUNCTION__, launchPointId, errorText);

    if (!errorText.empty())
        task->setError(Logger::API_ERR_CODE_GENERAL, errorText);

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

    Logger::info(getClassName(), __FUNCTION__, appId, launchPointPtr ? "success" : "failed");
    if (!errorText.empty())
        task->setError(Logger::API_ERR_CODE_GENERAL, errorText);

    task->reply();
}

void ApplicationManager::listLaunchPoints(LunaTaskPtr task)
{
    pbnjson::JValue responsePayload = pbnjson::Object();
    pbnjson::JValue launchPoints = pbnjson::Array();
    bool subscribed = false;

    LaunchPointManager::getInstance().convertLPsToJson(launchPoints);


    if (task->getMessage().isSubscription())
        ApplicationManager::getInstance().m_listLaunchPointsPoint.subscribe(task->getMessage());

    responsePayload.put("subscribed", subscribed);
    responsePayload.put("launchPoints", launchPoints);

    Logger::info(getClassName(), __FUNCTION__, task->caller(), "reply listLaunchPoint");
    task->replyResult(responsePayload);
}

void ApplicationManager::launch(LunaTaskPtr task)
{
    const pbnjson::JValue& responsePayload = task->getRequestPayload();

    std::string appId;
    if (!JValueUtil::getValue(responsePayload, "id", appId)) {
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, "App ID is not specified");
        return;
    }

    std::string display;
    if (!JValueUtil::getValue(responsePayload, "display", display)) {
        display = ""; // TODO default displayId
    }

    std::string mode;
    if (!JValueUtil::getValue(responsePayload, "params", "reason", mode)) {
        mode = "normal";
    }
    Logger::info(getClassName(), __FUNCTION__, appId, mode);
    LifecycleTaskPtr lifeCycleTask = std::make_shared<LifecycleTask>(task);
    lifeCycleTask->setAppId(appId);
    lifeCycleTask->setDisplay(display);

    LifecycleManager::getInstance().launch(lifeCycleTask);
}

void ApplicationManager::pause(LunaTaskPtr task)
{
    const pbnjson::JValue& responsePayload = task->getRequestPayload();

    std::string appId = responsePayload["id"].asString();
    if (appId.length() == 0) {
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, "App ID is not specified");
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
    const pbnjson::JValue& responsePayload = task->getRequestPayload();
    std::string appId = responsePayload["id"].asString();
    AppPackagePtr appPackagePtr = AppPackageManager::getInstance().getAppById(appId);

    if (AppTypeByDir::AppTypeByDir_Dev != appPackagePtr->getTypeByDir()) {
        string errorText = "Only Dev app should be closed using /dev category_API";
        Logger::warning(getClassName(), __FUNCTION__, appId, errorText);
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, errorText);
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
    pbnjson::JValue responsePayload = pbnjson::Object();
    pbnjson::JValue runningList = pbnjson::Array();
    RunningInfoManager::getInstance().getRunningList(runningList);

    responsePayload.put("returnValue", true);
    responsePayload.put("running", runningList);
    if (LSMessageIsSubscription(task->getRequest())) {
        responsePayload.put("subscribed", LSSubscriptionAdd(task->getLSHandle(), SUBSKEY_RUNNING, task->getRequest(), NULL));
    }

    task->replyResult(responsePayload);
}

void ApplicationManager::runningForDev(LunaTaskPtr task)
{
    pbnjson::JValue responsePayload = pbnjson::Object();
    pbnjson::JValue runningList = pbnjson::Array();
    RunningInfoManager::getInstance().getRunningList(runningList, true);

    responsePayload.put("returnValue", true);
    responsePayload.put("running", runningList);
    if (LSMessageIsSubscription(task->getRequest())) {
        responsePayload.put("subscribed", LSSubscriptionAdd(task->getLSHandle(), SUBSKEY_DEV_RUNNING, task->getRequest(), NULL));
    }

    task->replyResult(responsePayload);
}

void ApplicationManager::getAppLifeEvents(LunaTaskPtr task)
{
    pbnjson::JValue responsePayload = pbnjson::Object();

    if (LSMessageIsSubscription(task->getRequest())) {
        if (!LSSubscriptionAdd(task->getLSHandle(), SUBSKEY_GET_APP_LIFE_EVENTS, task->getRequest(), NULL)) {
            task->setError(Logger::API_ERR_CODE_GENERAL, "Subscription failed");
            responsePayload.put("subscribed", false);
        } else {
            responsePayload.put("subscribed", true);
        }
    } else {
        task->setError(Logger::API_ERR_CODE_GENERAL, "subscription is required");
        responsePayload.put("subscribed", false);
    }

    task->replyResult(responsePayload);
}

void ApplicationManager::getAppLifeStatus(LunaTaskPtr task)
{
    pbnjson::JValue responsePayload = pbnjson::Object();

    if (LSMessageIsSubscription(task->getRequest())) {
        if (!LSSubscriptionAdd(task->getLSHandle(), SUBSKEY_GET_APP_LIFE_STATUS, task->getRequest(), NULL)) {
            task->setError(Logger::API_ERR_CODE_GENERAL, "Subscription failed");
            responsePayload.put("subscribed", false);
        } else {
            responsePayload.put("subscribed", true);
        }
    } else {
        task->setError(Logger::API_ERR_CODE_GENERAL, "subscription is required");
        responsePayload.put("subscribed", false);
    }

    task->replyResult(responsePayload);
}

void ApplicationManager::getForegroundAppInfo(LunaTaskPtr task)
{
    const pbnjson::JValue& requestPayload = task->getRequestPayload();

    pbnjson::JValue responsePayload = pbnjson::Object();
    responsePayload.put("returnValue", true);

    if (requestPayload["extraInfo"].asBool() == true) {
        responsePayload.put("foregroundAppInfo", RunningInfoManager::getInstance().getForegroundInfo());
        if (LSMessageIsSubscription(task->getRequest())) {
            responsePayload.put("subscribed", LSSubscriptionAdd(task->getLSHandle(), SUBSKEY_FOREGROUND_INFO_EX, task->getRequest(), NULL));
        }
    } else {
        responsePayload.put(Logger::LOG_KEY_APPID, RunningInfoManager::getInstance().getForegroundAppId());
        responsePayload.put("windowId", "");
        responsePayload.put("processId", "");
        if (LSMessageIsSubscription(task->getRequest())) {
            responsePayload.put("subscribed", LSSubscriptionAdd(task->getLSHandle(), SUBSKEY_FOREGROUND_INFO, task->getRequest(), NULL));
        }
    }

    task->replyResult(responsePayload);
}

void ApplicationManager::lockApp(LunaTaskPtr task)
{
    const pbnjson::JValue& requestPayload = task->getRequestPayload();

    std::string appId = requestPayload["id"].asString();
    bool lock = requestPayload["lock"].asBool();
    std::string errorText;

    // TODO: move this property into app info manager
    AppPackageManager::getInstance().lockAppForUpdate(appId, lock, errorText);

    if (!errorText.empty()) {
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, errorText);
        return;
    }

    pbnjson::JValue responsePayload = pbnjson::Object();
    responsePayload.put("id", appId);
    responsePayload.put("locked", lock);

    task->replyResult(responsePayload);
}

void ApplicationManager::registerApp(LunaTaskPtr task)
{
    if (0 == task->caller().length()) {
        string errorText = "cannot find caller id";
        Logger::error(getClassName(), __FUNCTION__, "empty app id", errorText);
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, errorText);
        return;
    }

    std::string errorText;
    LifecycleManager::getInstance().registerApp(task->caller(), task->getRequest(), errorText);

    if (!errorText.empty()) {
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, errorText);
        return;
    }
}

void ApplicationManager::registerNativeApp(LunaTaskPtr task)
{
    if (0 == task->caller().length()) {
        string errorText = "cannot find caller id";
        Logger::error(getClassName(), __FUNCTION__, "empty app id", errorText);
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, errorText);
        return;
    }

    std::string errorText;
    LifecycleManager::getInstance().connectNativeApp(task->caller(), task->getRequest(), errorText);
    if (!errorText.empty()) {
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, errorText);
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
    const pbnjson::JValue& requestPayload = task->getRequestPayload();
    const std::map<std::string, AppPackagePtr>& apps = AppPackageManager::getInstance().allApps();

    pbnjson::JValue responsePayload = pbnjson::Object();
    pbnjson::JValue appsInfo = pbnjson::Array();

    for (auto it : apps) {
        if (AppTypeByDir::AppTypeByDir_Dev == it.second->getTypeByDir()) {
            appsInfo.append(it.second->toJValue());
        }
    }

    responsePayload.put("returnValue", true);

    bool is_full_list_client = true;
    if (requestPayload.hasKey("properties") && requestPayload["properties"].isArray()) {
        is_full_list_client = false;
        pbnjson::JValue apps_selected_info = pbnjson::Array();
        requestPayload["properties"].append("id"); // id is required
        (void) AppPackage::getSelectedPropsFromApps(appsInfo, requestPayload["properties"], apps_selected_info);
        responsePayload.put("apps", apps_selected_info);
    } else {
        responsePayload.put("apps", appsInfo);
    }

    if (task->getMessage().isSubscription()) {
        if (is_full_list_client) {
            responsePayload.put("subscribed", ApplicationManager::getInstance().m_listDevAppsPoint.subscribe(task->getMessage()));
        } else {
            responsePayload.put("subscribed", ApplicationManager::getInstance().m_listDevAppsCompactPoint.subscribe(task->getMessage()));
        }
    } else {
        responsePayload.put("subscribed", false);
    }

    task->replyResult(responsePayload);
}

void ApplicationManager::getAppStatus(LunaTaskPtr task)
{
    const pbnjson::JValue& requestPayload = task->getRequestPayload();

    pbnjson::JValue responsePayload = pbnjson::Object();
    std::string appId = requestPayload[Logger::LOG_KEY_APPID].asString();
    bool requiredAppInfo = (requestPayload.hasKey("appInfo") && requestPayload["appInfo"].isBoolean() && requestPayload["appInfo"].asBool()) ? true : false;

    if (appId.empty()) {
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, "Invalid appId specified");
        return;
    }

    if (task->getMessage().isSubscription()) {
        std::string subs_key = "getappstatus#" + appId + "#" + (requiredAppInfo ? "Y" : "N");
        if (LSSubscriptionAdd(task->getLSHandle(), subs_key.c_str(), task->getRequest(), NULL)) {
            responsePayload.put("subscribed", true);
        } else {
            Logger::warning(getClassName(), __FUNCTION__, "lscall", "failed");
            responsePayload.put("subscribed", false);
        }
    }

    // for first return (event: nothing)
    responsePayload.put(Logger::LOG_KEY_APPID, appId);
    responsePayload.put("event", "nothing");

    AppPackagePtr appDescPtr = AppPackageManager::getInstance().getAppById(appId);
    if (!appDescPtr) {
        responsePayload.put("status", "notExist");
        responsePayload.put("exist", false);
        responsePayload.put("launchable", false);
    } else {
        responsePayload.put("status", "launchable");
        responsePayload.put("exist", true);
        responsePayload.put("launchable", true);

        if (requiredAppInfo) {
            responsePayload.put("appInfo", appDescPtr->toJValue());
        }
    }

    task->replyResult(responsePayload);
}

void ApplicationManager::getAppInfo(LunaTaskPtr task)
{
    const pbnjson::JValue& requestPayload = task->getRequestPayload();

    pbnjson::JValue responsePayload = pbnjson::Object();
    pbnjson::JValue appInfo = pbnjson::Object();
    std::string appId = requestPayload["id"].asString();

    if (appId.empty()) {
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, "Invalid appId specified");
        return;
    }

    AppPackagePtr app_desc = AppPackageManager::getInstance().getAppById(appId);
    if (!app_desc) {
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, "Invalid appId specified OR Unsupported Application Type: " + appId);
        return;
    }

    if (requestPayload.hasKey("properties") && requestPayload["properties"].isArray()) {
        const pbnjson::JValue& origin_app = app_desc->toJValue();
        if (!AppPackage::getSelectedPropsFromAppInfo(origin_app, requestPayload["properties"], appInfo)) {
            task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, "Fail to get selected properties from AppInfo: " + appId);
            return;
        }
    } else {
        appInfo = app_desc->toJValue();
    }

    responsePayload.put("appInfo", appInfo);
    responsePayload.put(Logger::LOG_KEY_APPID, appId);
    task->replyResult(responsePayload);
}

void ApplicationManager::getAppBasePath(LunaTaskPtr task)
{
    const pbnjson::JValue& requestPayload = task->getRequestPayload();

    pbnjson::JValue responsePayload = pbnjson::Object();
    std::string appId = requestPayload[Logger::LOG_KEY_APPID].asString();

    if (appId.empty()) {
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, "Invalid appId specified");
        return;
    }
    if (task->caller() != appId) {
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, "Not allowed. Allow only for the info of calling app itself.");
        return;
    }

    AppPackagePtr app_desc = AppPackageManager::getInstance().getAppById(appId);
    if (!app_desc) {
        task->replyResultWithError(Logger::API_ERR_CODE_GENERAL, "Invalid appId specified: " + appId);
        return;
    }

    responsePayload.put(Logger::LOG_KEY_APPID, appId);
    responsePayload.put("basePath", app_desc->getMain());

    task->replyResult(responsePayload);
}
