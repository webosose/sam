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
#include <conf/SAMConf.h>
#include "base/LunaTaskList.h"
#include "base/LaunchPointList.h"
#include "base/AppDescriptionList.h"
#include "base/RunningAppList.h"
#include "bus/client/AppInstallService.h"
#include "bus/client/DB8.h"
#include <manager/PolicyManager.h>
#include <pbnjson.hpp>
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
    LunaTaskPtr lunaTask = nullptr;
    string errorText = "";
    int errorCode = 0;

    Logger::logAPIRequest(getInstance().getClassName(), __FUNCTION__, request, requestPayload);
    if (requestPayload.isNull()) {
        errorCode = ErrCode_INVALID_PAYLOAD;
        errorText = "invalid parameters";
        goto Done;
    }

    lunaTask = std::make_shared<LunaTask>(sh, request, requestPayload, message);
    if (!lunaTask) {
        errorCode = ErrCode_GENERAL;
        errorText = "memory alloc fail";
        goto Done;
    }

    if (getInstance().m_APIHandlers.find(request.getKind()) != getInstance().m_APIHandlers.end())
        handler = getInstance().m_APIHandlers[request.getKind()];

    if (!handler) {
        errorCode = ErrCode_DEPRECATED;
        errorText = "deprecated method";
        goto Done;
    }

    LunaTaskList::getInstance().add(lunaTask);
    handler(lunaTask);

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

    if (SAMConf::getInstance().isDevmodeEnabled()) {
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
    string appId = lunaTask->getAppId();
    string reason = "";
    bool keepAlive = false;
    bool spinner = true;
    bool noSplash = false;
    pbnjson::JValue params;

    if (appId.empty()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "App ID is not specified");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    JValueUtil::getValue(lunaTask->getRequestPayload(), "params", params);
    JValueUtil::getValue(lunaTask->getRequestPayload(), "params", "reason", reason);

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (appDesc == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "App ID is not exist");
        return;
    }
    if (!appDesc->isSpinnerOnLaunch()) {
        spinner = false;
    }
    if (!appDesc->isNoSplashOnLaunch()) {
        noSplash = false;
    }
    if (SAMConf::getInstance().isKeepAliveApp(lunaTask->getAppId())) {
        keepAlive = true;
    }
    if (reason.empty()) {
        reason = "normal";
    }

    lunaTask->setReason(reason);

    Logger::info(getClassName(), __FUNCTION__, appId,
                 Logger::format("keepAlive(%s) noSplash(%s) spinner(%s) reason(%s)",
                 Logger::toString(keepAlive), Logger::toString(noSplash), Logger::toString(spinner), reason.c_str()));
    PolicyManager::getInstance().launch(lunaTask);
}

void ApplicationManager::pause(LunaTaskPtr lunaTask)
{
    string appId = "";
    if (!JValueUtil::getValue(lunaTask->getRequestPayload(), "id", appId)) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "App ID is not specified");
        return;
    }
    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId);
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, appId + " is not running");
        return;
    }

    lunaTask->setErrCodeAndText(ErrCode_GENERAL, "This API is not implemented");
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
    return;
}

void ApplicationManager::closeByAppId(LunaTaskPtr lunaTask)
{
    string appId = "";
    if (!JValueUtil::getValue(lunaTask->getRequestPayload(), "id", appId)) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "App ID is not specified");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId);
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, appId + " is not running");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    if (LunaTaskList::getInstance().getByKindAndId(lunaTask->getRequest().getKind(), appId) != nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, appId + " is already closing");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
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

    JValueUtil::getValue(lunaTask->getRequestPayload(), "preloadOnly", preloadOnly);
    JValueUtil::getValue(lunaTask->getRequestPayload(), "reason", reason);
    JValueUtil::getValue(lunaTask->getRequestPayload(), "letAppHandle", letAppHandle);

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
    pbnjson::JValue running = pbnjson::Array();

    RunningAppList::getInstance().toJson(running);
    lunaTask->getResponsePayload().put("returnValue", true);
    lunaTask->getResponsePayload().put("running", running);
    if (LSMessageIsSubscription(lunaTask->getMessage())) {
        lunaTask->getResponsePayload().put("subscribed", LSSubscriptionAdd(lunaTask->getHandle(), SUBSKEY_RUNNING, lunaTask->getMessage(), NULL));
    }
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::runningForDev(LunaTaskPtr lunaTask)
{
    pbnjson::JValue running = pbnjson::Array();

    RunningAppList::getInstance().toJson(running, true);
    lunaTask->getResponsePayload().put("returnValue", true);
    lunaTask->getResponsePayload().put("running", running);
    if (LSMessageIsSubscription(lunaTask->getMessage())) {
        lunaTask->getResponsePayload().put("subscribed", LSSubscriptionAdd(lunaTask->getHandle(), SUBSKEY_DEV_RUNNING, lunaTask->getMessage(), NULL));
    }

    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::getAppLifeEvents(LunaTaskPtr lunaTask)
{
    if (LSMessageIsSubscription(lunaTask->getMessage())) {
        if (!LSSubscriptionAdd(lunaTask->getHandle(), SUBSKEY_GET_APP_LIFE_EVENTS, lunaTask->getMessage(), NULL)) {
            lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Subscription failed");
            lunaTask->getResponsePayload().put("subscribed", false);
        } else {
            lunaTask->getResponsePayload().put("subscribed", true);
        }
    } else {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "subscription is required");
        lunaTask->getResponsePayload().put("subscribed", false);
    }

    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::getAppLifeStatus(LunaTaskPtr lunaTask)
{
    if (LSMessageIsSubscription(lunaTask->getMessage())) {
        if (!LSSubscriptionAdd(lunaTask->getHandle(), SUBSKEY_GET_APP_LIFE_STATUS, lunaTask->getMessage(), NULL)) {
            lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Subscription failed");
            lunaTask->getResponsePayload().put("subscribed", false);
        } else {
            lunaTask->getResponsePayload().put("subscribed", true);
        }
    } else {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "subscription is required");
        lunaTask->getResponsePayload().put("subscribed", false);
    }

    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::getForegroundAppInfo(LunaTaskPtr lunaTask)
{
    lunaTask->getResponsePayload().put("returnValue", true);

    if (lunaTask->getRequestPayload()["extraInfo"].asBool() == true) {
        lunaTask->getResponsePayload().put("foregroundAppInfo", RunningAppList::getInstance().getForegroundInfo());
        if (LSMessageIsSubscription(lunaTask->getMessage())) {
            lunaTask->getResponsePayload().put("subscribed", LSSubscriptionAdd(lunaTask->getHandle(), SUBSKEY_FOREGROUND_INFO_EX, lunaTask->getMessage(), NULL));
        }
    } else {
        lunaTask->getResponsePayload().put("appId", RunningAppList::getInstance().getForegroundAppId());
        lunaTask->getResponsePayload().put("windowId", "");
        lunaTask->getResponsePayload().put("processId", "");
        if (LSMessageIsSubscription(lunaTask->getMessage())) {
            lunaTask->getResponsePayload().put("subscribed", LSSubscriptionAdd(lunaTask->getHandle(), SUBSKEY_FOREGROUND_INFO, lunaTask->getMessage(), NULL));
        }
    }

    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::lockApp(LunaTaskPtr lunaTask)
{
    std::string appId;
    bool lock;
    std::string errorText;

    JValueUtil::getValue(lunaTask->getRequestPayload(), "id", appId);
    JValueUtil::getValue(lunaTask->getRequestPayload(), "lock", lock);

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (!appDesc) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, appId + " was not found OR Unsupported Application Type");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    Logger::info(getClassName(), __FUNCTION__, appId, Logger::format("lock(%s)", Logger::toString(lock)));
    appDesc->lock();

    lunaTask->getResponsePayload().put("id", appId);
    lunaTask->getResponsePayload().put("locked", lock);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
    return;
}

void ApplicationManager::registerApp(LunaTaskPtr lunaTask)
{
    string appId = lunaTask->getRequest().getApplicationID();
    if (appId.empty()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "cannot find caller id");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId);
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, appId + " is not running");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    if (runningApp->getInterfaceVersion() != 2) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "trying to register via unmatched method with nativeLifeCycleInterface");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    runningApp->registerApp(lunaTask);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::registerNativeApp(LunaTaskPtr lunaTask)
{
    string appId = lunaTask->getRequest().getApplicationID();
    if (appId.empty()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "cannot find caller id");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId);
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, appId + " is not running");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    if (runningApp->getInterfaceVersion() != 1) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "trying to register via unmatched method with nativeLifeCycleInterface");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    runningApp->registerApp(lunaTask);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::listApps(LunaTaskPtr lunaTask)
{
    pbnjson::JValue apps = pbnjson::Array();
    pbnjson::JValue properties = pbnjson::Array();

    if (JValueUtil::getValue(lunaTask->getRequestPayload(), "properties", properties) && properties.arraySize() > 0) {
        properties.append("id");
    }

    bool isDevmode = (strcmp(lunaTask->getRequest().getKind(), "/dev/listApps") == 0);
    AppDescriptionList::getInstance().toJson(apps, properties, isDevmode);
    lunaTask->getResponsePayload().put("apps", apps);

    if (lunaTask->getRequest().isSubscription()) {
        lunaTask->getResponsePayload().put("subscribed", LSSubscriptionAdd(lunaTask->getHandle(), METHOD_LIST_APPS, lunaTask->getMessage(), nullptr));
    } else {
        lunaTask->getResponsePayload().put("subscribed", false);
    }
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::getAppStatus(LunaTaskPtr lunaTask)
{
    string appId = lunaTask->getAppId();
    bool appInfo = false;

    JValueUtil::getValue(lunaTask->getRequestPayload(), "appInfo", appInfo);

    if (appId.empty()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid appId specified");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    if (lunaTask->getRequest().isSubscription()) {
        std::string subscriptionKey = "getappstatus#" + appId + "#" + (appInfo ? "Y" : "N");
        if (LSSubscriptionAdd(lunaTask->getHandle(), subscriptionKey.c_str(), lunaTask->getMessage(), NULL)) {
            lunaTask->getResponsePayload().put("subscribed", true);
        } else {
            lunaTask->getResponsePayload().put("subscribed", false);
        }
    }

    // for first return (event: nothing)
    lunaTask->getResponsePayload().put("appId", appId);
    lunaTask->getResponsePayload().put("event", "nothing");

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (!appDesc) {
        lunaTask->getResponsePayload().put("status", "notExist");
        lunaTask->getResponsePayload().put("exist", false);
        lunaTask->getResponsePayload().put("launchable", false);
    } else {
        lunaTask->getResponsePayload().put("status", "launchable");
        lunaTask->getResponsePayload().put("exist", true);
        lunaTask->getResponsePayload().put("launchable", true);

        if (appInfo) {
            lunaTask->getResponsePayload().put("appInfo", appDesc->getJson());
        }
    }

    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::getAppInfo(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    std::string appId = requestPayload["id"].asString();
    if (appId.empty()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid appId specified");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (!appDesc) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid appId specified OR Unsupported Application Type: " + appId);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    pbnjson::JValue appInfo;
    pbnjson::JValue properties;
    if (JValueUtil::getValue(requestPayload, "properties", properties) && properties.isArray()) {
        appInfo = appDesc->getJson(properties);
    } else {
        appInfo = appDesc->getJson();
    }

    lunaTask->getResponsePayload().put("appInfo", appInfo);
    lunaTask->getResponsePayload().put("appId", appId);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::getAppBasePath(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    std::string appId = requestPayload["appId"].asString();

    if (appId.empty()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid appId specified");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (!appDesc) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid appId specified: " + appId);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    if (lunaTask->getCaller() != appId) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Not allowed. Allow only for the info of calling app itself.");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    lunaTask->getResponsePayload().put("appId", appId);
    lunaTask->getResponsePayload().put("basePath", appDesc->getAbsMain());

    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::addLaunchPoint(LunaTaskPtr lunaTask)
{
    const pbnjson::JValue& requestPayload = lunaTask->getRequestPayload();

    std::string appId;
    if (!JValueUtil::getValue(requestPayload, "id", appId)) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "missing required 'id' parameter");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    if (appId.empty()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid appId specified");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(appId);
    if (!appDesc) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid appId specified OR Unsupported Application Type: " + appId);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    LaunchPointPtr launchPoint = LaunchPointList::getInstance().createBootmark(appDesc, requestPayload);
    if (!launchPoint) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Cannot create bookmark");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    LaunchPointList::getInstance().add(launchPoint);
    lunaTask->getResponsePayload().put("launchPointId", launchPoint->getLaunchPointId());
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::updateLaunchPoint(LunaTaskPtr lunaTask)
{
    pbnjson::JValue requestPayload = lunaTask->getRequestPayload().duplicate();

    std::string launchPointId = "";
    if (!JValueUtil::getValue(requestPayload, "launchPointId", launchPointId)) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "launchPointId is empty");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByLaunchPointId(launchPointId);
    if (launchPoint == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "cannot find launch point");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    requestPayload.remove("launchPointId");
    launchPoint->updateDatabase(requestPayload);
    launchPoint->syncDatabase();
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::removeLaunchPoint(LunaTaskPtr lunaTask)
{
    pbnjson::JValue requestPayload = lunaTask->getRequestPayload().duplicate();

    std::string launchPointId = "";
    if (!JValueUtil::getValue(requestPayload, "launchPointId", launchPointId)) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "launchPointId is empty");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByLaunchPointId(launchPointId);
    if (launchPoint == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "cannot find launch point");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    if (!launchPoint->isRemovable()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "this launch point cannot be removable");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
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
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid launch point type");
        return;

    }

    LunaTaskList::getInstance().removeAfterReply(lunaTask);
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
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "launchPointId is empty");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    if (position < 0) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "position number should be over 0");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByLaunchPointId(launchPointId);
    if (launchPoint == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "cannot find launch point");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    if (!launchPoint->isVisible()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "this launch point is not visible");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    // TODO position needs to be set
}

void ApplicationManager::listLaunchPoints(LunaTaskPtr lunaTask)
{
    pbnjson::JValue launchPoints = pbnjson::Array();
    bool subscribed = false;

    LaunchPointList::getInstance().toJson(launchPoints);

    if (lunaTask->getRequest().isSubscription())
        ApplicationManager::getInstance().m_listLaunchPointsPoint.subscribe(lunaTask->getRequest());

    lunaTask->getResponsePayload().put("subscribed", subscribed);
    lunaTask->getResponsePayload().put("launchPoints", launchPoints);

    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::managerInfo(LunaTaskPtr lunaTask)
{
    lunaTask->getResponsePayload().put("returnValue", true);

//    pbnjson::JValue apps = pbnjson::Array();
//    pbnjson::JValue properties = pbnjson::Array();
//    AppDescriptionList::getInstance().toJson(apps, properties);
//    lunaTask->getResponsePayload().put("apps", apps);
//
//    pbnjson::JValue launchPoints = pbnjson::Array();
//    LaunchPointList::getInstance().toJson(launchPoints);
//    lunaTask->getResponsePayload().put("launchPoints", launchPoints);

    pbnjson::JValue running = pbnjson::Array();
    RunningAppList::getInstance().toJson(running);
    lunaTask->getResponsePayload().put("running", running);

    pbnjson::JValue lunaTasks = pbnjson::Array();
    LunaTaskList::getInstance().toJson(lunaTasks);
    lunaTask->getResponsePayload().put("lunaTasks", lunaTasks);

    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}
