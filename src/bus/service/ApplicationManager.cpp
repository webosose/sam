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
#include "base/LunaTaskList.h"
#include "base/LaunchPointList.h"
#include "base/AppDescriptionList.h"
#include "base/RunningAppList.h"
#include "bus/client/AppInstallService.h"
#include "bus/client/DB8.h"
#include <manager/LifecycleManager.h>
#include <manager/PolicyManager.h>
#include <pbnjson.hpp>
#include <setting/Settings.h>
#include <string>
#include <vector>
#include "SchemaChecker.h"
#include "util/JValueUtil.h"
#include "util/Time.h"

const char* ApplicationManager::CATEGORY_ROOT = "/";
const char* ApplicationManager::CATEGORY_DEV = "/dev";

const char* ApplicationManager::METHOD_LAUNCH = "launch";
const char* ApplicationManager::METHOD_PAUSE = "pause";
const char* ApplicationManager::METHOD_CLOSE_BY_APPID = "closeByAppId";
const char* ApplicationManager::METHOD_RUNNING = "running";
const char* ApplicationManager::METHOD_GET_APP_LIFE_EVENTS ="getAppLifeEvents";
const char* ApplicationManager::METHOD_GET_APP_LIFE_STATUS = "getAppLifeStatus";
const char* ApplicationManager::METHOD_GET_FOREGROUND_APPINFO = "getForegroundAppInfo";
const char* ApplicationManager::METHOD_LOCK_APP = "lockApp";
const char* ApplicationManager::METHOD_REGISTER_APP = "registerApp";

const char* ApplicationManager::METHOD_LIST_APPS = "listApps";
const char* ApplicationManager::METHOD_GET_APP_STATUS = "getAppStatus";
const char* ApplicationManager::METHOD_GET_APP_INFO = "getAppInfo";
const char* ApplicationManager::METHOD_GET_APP_BASE_PATH = "getAppBasePath";

const char* ApplicationManager::METHOD_ADD_LAUNCHPOINT = "addLaunchPoint";
const char* ApplicationManager::METHOD_UPDATE_LAUNCHPOINT = "updateLaunchPoint";
const char* ApplicationManager::METHOD_REMOVE_LAUNCHPOINT = "removeLaunchPoint";
const char* ApplicationManager::METHOD_MOVE_LAUNCHPOINT = "moveLaunchPoint";
const char* ApplicationManager::METHOD_LIST_LAUNCHPOINTS = "listLaunchPoints";

const char* ApplicationManager::METHOD_MANAGER_INFO = "managerInfo";

LSMethod ApplicationManager::METHODS_ROOT[] = {
    { METHOD_LAUNCH,                   ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_PAUSE,                    ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_CLOSE_BY_APPID,           ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_RUNNING,                  ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_GET_APP_LIFE_EVENTS,      ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_GET_APP_LIFE_STATUS,      ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_GET_FOREGROUND_APPINFO,   ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_LOCK_APP,                 ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_REGISTER_APP,             ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },

    // core: package
    { METHOD_LIST_APPS,                ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_GET_APP_STATUS,           ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_GET_APP_INFO,             ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_GET_APP_BASE_PATH,        ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },

    // core: launchpoint
    { METHOD_ADD_LAUNCHPOINT,          ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_UPDATE_LAUNCHPOINT,       ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_REMOVE_LAUNCHPOINT,       ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_MOVE_LAUNCHPOINT,         ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_LIST_LAUNCHPOINTS,        ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { 0,                               0,                               LUNA_METHOD_FLAGS_NONE }
};

LSMethod ApplicationManager::METHODS_DEV[] = {
    { METHOD_CLOSE_BY_APPID,           ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_LIST_APPS,                ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_RUNNING,                  ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_MANAGER_INFO,             ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { 0,                               0,                               LUNA_METHOD_FLAGS_NONE }
};

bool ApplicationManager::onAPICalled(LSHandle* sh, LSMessage* message, void* ctx)
{
    Message request(message);
    JValue requestPayload = SchemaChecker::getInstance().getRequestPayloadWithSchema(request);
    LunaApiHandler handler;
    LunaTaskPtr task = nullptr;
    string errorText = "";
    int errorCode = 0;

    Logger::logAPIRequest(getInstance().getClassName(), __FUNCTION__, request, requestPayload);
    if (requestPayload.isNull()) {
        errorCode = Logger::ERRCODE_INVALID_PAYLOAD;
        errorText = "invalid parameters";
        goto Done;
    }

    task = std::make_shared<LunaTask>(sh, request, requestPayload, message);
    if (!task) {
        errorCode = Logger::ERRCODE_GENERAL;
        errorText = "memory alloc fail";
        goto Done;
    }

    if (getInstance().m_APIHandlers.find(request.getKind()) != getInstance().m_APIHandlers.end())
        handler = getInstance().m_APIHandlers[request.getKind()];

    if (!handler) {
        errorCode = Logger::ERRCODE_DEPRECATED;
        errorText = "deprecated method";
        goto Done;
    }

    LunaTaskList::getInstance().add(task);
    handler(task);

Done:
    if (!errorText.empty()) {
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
      m_enableSubscription(false)
{
    setClassName("ApplicationManager");

    registerApiHandler(CATEGORY_ROOT, METHOD_LAUNCH, boost::bind(&ApplicationManager::launch, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_PAUSE, boost::bind(&ApplicationManager::pause, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_CLOSE_BY_APPID, boost::bind(&ApplicationManager::closeByAppId, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_RUNNING, boost::bind(&ApplicationManager::running, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_GET_APP_LIFE_EVENTS, boost::bind(&ApplicationManager::getAppLifeEvents, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_GET_APP_LIFE_STATUS, boost::bind(&ApplicationManager::getAppLifeStatus, this, _1));

    registerApiHandler(CATEGORY_ROOT, METHOD_GET_FOREGROUND_APPINFO, boost::bind(&ApplicationManager::getForegroundAppInfo, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_LOCK_APP, boost::bind(&ApplicationManager::lockApp, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_REGISTER_APP, boost::bind(&ApplicationManager::registerApp, this, _1));

    registerApiHandler(CATEGORY_ROOT, METHOD_LIST_APPS, boost::bind(&ApplicationManager::listApps, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_GET_APP_STATUS, boost::bind(&ApplicationManager::getAppStatus, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_GET_APP_INFO, boost::bind(&ApplicationManager::getAppInfo, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_GET_APP_BASE_PATH, boost::bind(&ApplicationManager::getAppBasePath, this, _1));

    registerApiHandler(CATEGORY_ROOT, METHOD_ADD_LAUNCHPOINT, boost::bind(&ApplicationManager::addLaunchPoint, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_UPDATE_LAUNCHPOINT, boost::bind(&ApplicationManager::updateLaunchPoint, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_REMOVE_LAUNCHPOINT, boost::bind(&ApplicationManager::removeLaunchPoint, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_MOVE_LAUNCHPOINT, boost::bind(&ApplicationManager::moveLaunchPoint, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_LIST_LAUNCHPOINTS, boost::bind(&ApplicationManager::listLaunchPoints, this, _1));

    registerApiHandler(CATEGORY_DEV, METHOD_CLOSE_BY_APPID, boost::bind(&ApplicationManager::closeByAppId, this, _1));
    registerApiHandler(CATEGORY_DEV, METHOD_LIST_APPS, boost::bind(&ApplicationManager::listApps, this, _1));
    registerApiHandler(CATEGORY_DEV, METHOD_RUNNING, boost::bind(&ApplicationManager::runningForDev, this, _1));
    registerApiHandler(CATEGORY_DEV, METHOD_MANAGER_INFO, boost::bind(&ApplicationManager::managerInfo, this, _1));
}

ApplicationManager::~ApplicationManager()
{
}

bool ApplicationManager::attach(GMainLoop* gml)
{
    this->registerCategory(CATEGORY_ROOT, METHODS_ROOT, nullptr, nullptr);
    m_compat1.registerCategory(CATEGORY_ROOT, METHODS_ROOT, nullptr, nullptr);
    m_compat2.registerCategory(CATEGORY_ROOT, METHODS_ROOT, nullptr, nullptr);

    if (SettingsImpl::getInstance().isDevmodeEnabled()) {
        this->registerCategory(CATEGORY_DEV, METHODS_DEV, nullptr, nullptr);
        m_compat1.registerCategory(CATEGORY_DEV, METHODS_DEV, nullptr, nullptr);
        m_compat2.registerCategory(CATEGORY_DEV, METHODS_DEV, nullptr, nullptr);
    }

    m_listLaunchPointsPoint.setServiceHandle(this);
    m_listAppsPoint.setServiceHandle(this);
    m_listAppsCompactPoint.setServiceHandle(this);
    m_listDevAppsPoint.setServiceHandle(this);
    m_listDevAppsCompactPoint.setServiceHandle(this);
    m_getAppLifeEvents.setServiceHandle(this);
    m_getAppLifeStatus.setServiceHandle(this);

    this->attachToLoop(gml);
    m_compat1.attachToLoop(gml);
    m_compat2.attachToLoop(gml);
    return true;
}

void ApplicationManager::detach()
{
    m_APIHandlers.clear();

    this->detach();
    m_compat1.detach();
    m_compat2.detach();
}

void ApplicationManager::launch(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    string appId = lunaTask->getAppId();
    string reason = "";
    bool keepAlive = false;
    bool spinner = true;
    bool noSplash = false;
    pbnjson::JValue params;

    if (appId.empty()) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "App ID is not specified");
        return;
    }
    if (LunaTaskList::getInstance().getByKindAndId(lunaTask->getRequest().getKind(), appId) != nullptr) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, appId + " is already launching");
        return;
    }

    JValueUtil::getValue(requestPayload, "keepAlive", keepAlive);
    JValueUtil::getValue(requestPayload, "noSplash", noSplash);
    JValueUtil::getValue(requestPayload, "spinner", spinner);
    JValueUtil::getValue(requestPayload, "params", params);
    JValueUtil::getValue(requestPayload, "params", "reason", reason);

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (appDesc == nullptr) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "App ID is not exist");
        return;
    }
    if (!appDesc->isSpinnerOnLaunch()) {
        spinner = false;
    }
    if (!appDesc->isNoSplashOnLaunch()) {
        noSplash = false;
    }
    if (SettingsImpl::getInstance().isKeepAliveApp(lunaTask->getAppId())) {
        keepAlive = true;
    }
    if (reason.empty()) {
        reason = "normal";
    }

    lunaTask->setKeepAlive(keepAlive);
    lunaTask->setNoSplash(noSplash);
    lunaTask->setSpinner(spinner);
    lunaTask->setReason(reason);

    Logger::info(getClassName(), __FUNCTION__, appId,
                 Logger::format("keepAlive(%s) noSplash(%s) spinner(%s) reason(%s)",
                 Logger::toString(keepAlive), Logger::toString(noSplash), Logger::toString(spinner), reason.c_str()));
    PolicyManager::getInstance().launch(lunaTask);
}

void ApplicationManager::pause(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    string appId = "";
    if (!JValueUtil::getValue(requestPayload, "id", appId)) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "App ID is not specified");
        return;
    }
    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId);
    if (runningApp == nullptr) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, appId + " is not running");
        return;
    }

    lunaTask->reply(Logger::ERRCODE_GENERAL, "This API is not implemented");
    return;
}

void ApplicationManager::closeByAppId(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    string appId = "";
    if (!JValueUtil::getValue(requestPayload, "id", appId)) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "App ID is not specified");
        return;
    }
    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId);
    if (runningApp == nullptr) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, appId + " is not running");
        return;
    }
    if (LunaTaskList::getInstance().getByKindAndId(lunaTask->getRequest().getKind(), appId) != nullptr) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, appId + " is already closing");
        return;
    }
    if (strcmp(lunaTask->getRequest().getCategory(), "/dev") == 0 &&
        runningApp->getLaunchPoint()->getAppDesc()->isDevmodeApp()) {
        Logger::warning(getClassName(), __FUNCTION__, appId, "Only Dev app should be closed using /dev category_API");
        return;
    }

    std::string callerId = lunaTask->getCaller();
    bool preloadOnly = false;
    bool letAppHandle = false;
    std::string reason = "";

    JValueUtil::getValue(requestPayload, "preloadOnly", preloadOnly);
    JValueUtil::getValue(requestPayload, "reason", reason);
    JValueUtil::getValue(requestPayload, "letAppHandle", letAppHandle);

    if (preloadOnly) {
        Logger::warning(getClassName(), __FUNCTION__, appId, "app is being launched by user");
        return;
    }

    if (reason.empty()) {
        reason = lunaTask->getCaller();
    }
    runningApp->setReason(reason);
    runningApp->close(lunaTask);
}

void ApplicationManager::running(LunaTaskPtr lunaTask)
{
    pbnjson::JValue responsePayload = pbnjson::Object();
    pbnjson::JValue runningList = pbnjson::Array();

    RunningAppList::getInstance().toJson(runningList);
    responsePayload.put("returnValue", true);
    responsePayload.put("running", runningList);
    if (LSMessageIsSubscription(lunaTask->getMessage())) {
        responsePayload.put("subscribed", LSSubscriptionAdd(lunaTask->getHandle(), SUBSKEY_RUNNING, lunaTask->getMessage(), NULL));
    }

    lunaTask->reply(responsePayload);
}

void ApplicationManager::runningForDev(LunaTaskPtr lunaTask)
{
    pbnjson::JValue responsePayload = pbnjson::Object();
    pbnjson::JValue runningList = pbnjson::Array();

    RunningAppList::getInstance().toJson(runningList, true);
    responsePayload.put("returnValue", true);
    responsePayload.put("running", runningList);
    if (LSMessageIsSubscription(lunaTask->getMessage())) {
        responsePayload.put("subscribed", LSSubscriptionAdd(lunaTask->getHandle(), SUBSKEY_DEV_RUNNING, lunaTask->getMessage(), NULL));
    }

    lunaTask->reply(responsePayload);
}

void ApplicationManager::getAppLifeEvents(LunaTaskPtr lunaTask)
{
    pbnjson::JValue responsePayload = pbnjson::Object();

    if (LSMessageIsSubscription(lunaTask->getMessage())) {
        if (!LSSubscriptionAdd(lunaTask->getHandle(), SUBSKEY_GET_APP_LIFE_EVENTS, lunaTask->getMessage(), NULL)) {
            lunaTask->setErrorCodeAndText(Logger::ERRCODE_GENERAL, "Subscription failed");
            responsePayload.put("subscribed", false);
        } else {
            responsePayload.put("subscribed", true);
        }
    } else {
        lunaTask->setErrorCodeAndText(Logger::ERRCODE_GENERAL, "subscription is required");
        responsePayload.put("subscribed", false);
    }

    lunaTask->reply(responsePayload);
}

void ApplicationManager::getAppLifeStatus(LunaTaskPtr lunaTask)
{
    pbnjson::JValue responsePayload = pbnjson::Object();

    if (LSMessageIsSubscription(lunaTask->getMessage())) {
        if (!LSSubscriptionAdd(lunaTask->getHandle(), SUBSKEY_GET_APP_LIFE_STATUS, lunaTask->getMessage(), NULL)) {
            lunaTask->setErrorCodeAndText(Logger::ERRCODE_GENERAL, "Subscription failed");
            responsePayload.put("subscribed", false);
        } else {
            responsePayload.put("subscribed", true);
        }
    } else {
        lunaTask->setErrorCodeAndText(Logger::ERRCODE_GENERAL, "subscription is required");
        responsePayload.put("subscribed", false);
    }

    lunaTask->reply(responsePayload);
}

void ApplicationManager::getForegroundAppInfo(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    pbnjson::JValue responsePayload = pbnjson::Object();
    responsePayload.put("returnValue", true);

    if (requestPayload["extraInfo"].asBool() == true) {
        responsePayload.put("foregroundAppInfo", RunningAppList::getInstance().getForegroundInfo());
        if (LSMessageIsSubscription(lunaTask->getMessage())) {
            responsePayload.put("subscribed", LSSubscriptionAdd(lunaTask->getHandle(), SUBSKEY_FOREGROUND_INFO_EX, lunaTask->getMessage(), NULL));
        }
    } else {
        responsePayload.put("appId", RunningAppList::getInstance().getForegroundAppId());
        responsePayload.put("windowId", "");
        responsePayload.put("processId", "");
        if (LSMessageIsSubscription(lunaTask->getMessage())) {
            responsePayload.put("subscribed", LSSubscriptionAdd(lunaTask->getHandle(), SUBSKEY_FOREGROUND_INFO, lunaTask->getMessage(), NULL));
        }
    }

    lunaTask->reply(responsePayload);
}

void ApplicationManager::lockApp(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    std::string appId;
    bool lock;
    std::string errorText;

    JValueUtil::getValue(requestPayload, "id", appId);
    JValueUtil::getValue(requestPayload, "lock", lock);

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (!appDesc) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, appId + " was not found OR Unsupported Application Type");
        return;
    }

    Logger::info(getClassName(), __FUNCTION__, appId, Logger::format("lock(%s)", Logger::toString(lock)));
    appDesc->lock();

    pbnjson::JValue responsePayload = pbnjson::Object();
    responsePayload.put("id", appId);
    responsePayload.put("locked", lock);
    lunaTask->reply(responsePayload);
    return;
}

void ApplicationManager::registerApp(LunaTaskPtr lunaTask)
{
    string appId = lunaTask->getRequest().getApplicationID();
    if (appId.empty()) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "cannot find caller id");
        return;
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId);
    if (runningApp == nullptr) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, appId + " is not running");
        return;
    }

    if (runningApp->getInterfaceVersion() != 2) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "trying to register via unmatched method with nativeLifeCycleInterface");
        return;
    }

    runningApp->registerApp(lunaTask);
    lunaTask->reply();
}

void ApplicationManager::registerNativeApp(LunaTaskPtr lunaTask)
{
    string appId = lunaTask->getRequest().getApplicationID();
    if (appId.empty()) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "cannot find caller id");
        return;
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId);
    if (runningApp == nullptr) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, appId + " is not running");
        return;
    }

    if (runningApp->getInterfaceVersion() != 1) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "trying to register via unmatched method with nativeLifeCycleInterface");
        return;
    }

    runningApp->registerApp(lunaTask);
    lunaTask->reply();
}

void ApplicationManager::listApps(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    pbnjson::JValue responsePayload = pbnjson::Object();
    pbnjson::JValue apps = pbnjson::Array();
    pbnjson::JValue properties = pbnjson::Array();

    if (JValueUtil::getValue(requestPayload, "properties", properties) && properties.arraySize() > 0) {
        properties.append("id");
    }

    bool isDevmode = (strcmp(lunaTask->getRequest().getKind(), "/dev/listApps") == 0);
    AppDescriptionList::getInstance().toJson(apps, properties, isDevmode);
    responsePayload.put("apps", apps);

    if (lunaTask->getRequest().isSubscription()) {
        responsePayload.put("subscribed", LSSubscriptionAdd(lunaTask->getHandle(), METHOD_LIST_APPS, lunaTask->getMessage(), nullptr));
    } else {
        responsePayload.put("subscribed", false);
    }
    lunaTask->reply(responsePayload);
}

void ApplicationManager::getAppStatus(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    pbnjson::JValue responsePayload = pbnjson::Object();
    std::string appId = requestPayload["appId"].asString();
    bool requiredAppInfo = (requestPayload.hasKey("appInfo") && requestPayload["appInfo"].isBoolean() && requestPayload["appInfo"].asBool()) ? true : false;

    if (appId.empty()) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "Invalid appId specified");
        return;
    }

    if (lunaTask->getRequest().isSubscription()) {
        std::string subs_key = "getappstatus#" + appId + "#" + (requiredAppInfo ? "Y" : "N");
        if (LSSubscriptionAdd(lunaTask->getHandle(), subs_key.c_str(), lunaTask->getMessage(), NULL)) {
            responsePayload.put("subscribed", true);
        } else {
            Logger::warning(getClassName(), __FUNCTION__, "lscall", "failed");
            responsePayload.put("subscribed", false);
        }
    }

    // for first return (event: nothing)
    responsePayload.put("appId", appId);
    responsePayload.put("event", "nothing");

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (!appDesc) {
        responsePayload.put("status", "notExist");
        responsePayload.put("exist", false);
        responsePayload.put("launchable", false);
    } else {
        responsePayload.put("status", "launchable");
        responsePayload.put("exist", true);
        responsePayload.put("launchable", true);

        if (requiredAppInfo) {
            responsePayload.put("appInfo", appDesc->getJson());
        }
    }

    lunaTask->reply(responsePayload);
}

void ApplicationManager::getAppInfo(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    pbnjson::JValue responsePayload = pbnjson::Object();
    std::string appId = requestPayload["id"].asString();
    if (appId.empty()) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "Invalid appId specified");
        return;
    }

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (!appDesc) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "Invalid appId specified OR Unsupported Application Type: " + appId);
        return;
    }

    pbnjson::JValue appInfo;
    pbnjson::JValue properties;
    if (JValueUtil::getValue(requestPayload, "properties", properties) && properties.isArray()) {
        appInfo = appDesc->getJson(properties);
    } else {
        appInfo = appDesc->getJson();
    }

    responsePayload.put("appInfo", appInfo);
    responsePayload.put("appId", appId);
    lunaTask->reply(responsePayload);
}

void ApplicationManager::getAppBasePath(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    pbnjson::JValue responsePayload = pbnjson::Object();
    std::string appId = requestPayload["appId"].asString();

    if (appId.empty()) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "Invalid appId specified");
        return;
    }
    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (!appDesc) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "Invalid appId specified: " + appId);
        return;
    }
    if (lunaTask->getCaller() != appId) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "Not allowed. Allow only for the info of calling app itself.");
        return;
    }

    responsePayload.put("appId", appId);
    responsePayload.put("basePath", appDesc->getAbsMain());

    lunaTask->reply(responsePayload);
}

void ApplicationManager::addLaunchPoint(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    std::string appId;
    if (!JValueUtil::getValue(requestPayload, "id", appId)) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "missing required 'id' parameter");
        return;
    }
    if (appId.empty()) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "Invalid appId specified");
        return;
    }

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (!appDesc) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "Invalid appId specified OR Unsupported Application Type: " + appId);
        return;
    }

    LaunchPointPtr launchPoint = LaunchPointList::getInstance().createBootmark(appDesc, requestPayload);
    if (!launchPoint) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "Cannot create bookmark");
        return;
    }

    LaunchPointList::getInstance().add(launchPoint);
    lunaTask->getResponsePayload().put("launchPointId", launchPoint->getLaunchPointId());
    lunaTask->reply();
}

void ApplicationManager::updateLaunchPoint(LunaTaskPtr lunaTask)
{
    pbnjson::JValue requestPayload = lunaTask->getRequestPayload().duplicate();

    std::string launchPointId = "";
    if (!JValueUtil::getValue(requestPayload, "launchPointId", launchPointId)) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "launchPointId is empty");
        return;
    }
    LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByLaunchPointId(launchPointId);
    if (launchPoint == nullptr) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "cannot find launch point");
        return;
    }

    requestPayload.remove("launchPointId");
    launchPoint->updateDatabase(requestPayload);
    launchPoint->syncDatabase();
    lunaTask->reply();
}

void ApplicationManager::removeLaunchPoint(LunaTaskPtr lunaTask)
{
    pbnjson::JValue requestPayload = lunaTask->getRequestPayload().duplicate();

    std::string launchPointId = "";
    if (!JValueUtil::getValue(requestPayload, "launchPointId", launchPointId)) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "launchPointId is empty");
        return;
    }
    LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByLaunchPointId(launchPointId);
    if (launchPoint == nullptr) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "cannot find launch point");
        return;
    }
    if (!launchPoint->isRemovable()) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "this launch point cannot be removable");
        return;
    }

    switch(launchPoint->getType()) {
    case LaunchPointType::LaunchPoint_DEFAULT:
        if (AppLocation::AppLocation_System_ReadOnly != launchPoint->getAppDesc()->getAppLocation()) {
            Call call = AppInstallService::getInstance().remove(launchPoint->getAppDesc()->getAppId());
            Logger::info(getClassName(), __FUNCTION__, launchPoint->getAppDesc()->getAppId(), "requested_to_appinstalld");
        }

        if (!launchPoint->getAppDesc()->isVisible()) {
            AppDescriptionList::getInstance().remove(launchPoint->getAppDesc());
            // removeApp(appId, false, AppStatusEvent::AppStatusEvent_Uninstalled);
        } else {
            // Call call = SettingService::getInstance().checkParentalLock(onCheckParentalLock, appId);
        }
        break;

    case LaunchPointType::LaunchPoint_BOOKMARK:
        ApplicationManager::getInstance().postListLaunchPoints(launchPoint);
        DB8::getInstance().deleteLaunchPoint(launchPoint->getLaunchPointId());
        //LifecycleManager::getInstance().closeByAppId(launchPoint->getAppDesc()->getAppId(), "", "", errorText, false, true);
        LaunchPointList::getInstance().remove(launchPoint);
        break;

    default:
        lunaTask->reply(Logger::ERRCODE_GENERAL, "Invalid launch point type");
        return;

    }

    lunaTask->reply();
}

void ApplicationManager::moveLaunchPoint(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    std::string launchPointId = "";
    std::string errorText = "";
    int position = -1;

    JValueUtil::getValue(requestPayload, "launchPointId", launchPointId);
    JValueUtil::getValue(requestPayload, "position", position);
    if (launchPointId.empty()) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "launchPointId is empty");
        return;
    }
    if (position < 0) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "position number should be over 0");
        return;
    }
    LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByLaunchPointId(launchPointId);
    if (launchPoint == nullptr) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "cannot find launch point");
        return;
    }
    if (!launchPoint->isVisible()) {
        lunaTask->reply(Logger::ERRCODE_GENERAL, "this launch point is not visible");
         return;
    }

    // TODO position needs to be set

    lunaTask->reply();
}

void ApplicationManager::listLaunchPoints(LunaTaskPtr lunaTask)
{
    pbnjson::JValue responsePayload = pbnjson::Object();
    pbnjson::JValue launchPoints = pbnjson::Array();
    bool subscribed = false;

    LaunchPointList::getInstance().toJson(launchPoints);

    if (lunaTask->getRequest().isSubscription())
        ApplicationManager::getInstance().m_listLaunchPointsPoint.subscribe(lunaTask->getRequest());

    responsePayload.put("subscribed", subscribed);
    responsePayload.put("launchPoints", launchPoints);

    lunaTask->reply(responsePayload);
}

void ApplicationManager::managerInfo(LunaTaskPtr lunaTask)
{
    pbnjson::JValue responsePayload = pbnjson::Object();
    responsePayload.put("returnValue", true);

//    pbnjson::JValue apps = pbnjson::Array();
//    pbnjson::JValue properties = pbnjson::Array();
//    AppDescriptionList::getInstance().toJson(apps, properties);
//    responsePayload.put("apps", apps);
//
//    pbnjson::JValue launchPoints = pbnjson::Array();
//    LaunchPointList::getInstance().toJson(launchPoints);
//    responsePayload.put("launchPoints", launchPoints);

    pbnjson::JValue running = pbnjson::Array();
    RunningAppList::getInstance().toJson(running);
    responsePayload.put("running", running);

    pbnjson::JValue lunaTasks = pbnjson::Array();
    LunaTaskList::getInstance().toJson(lunaTasks);
    responsePayload.put("lunaTasks", lunaTasks);

    lunaTask->reply(responsePayload);
}
