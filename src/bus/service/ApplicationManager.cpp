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

#include "ApplicationManager.h"

#include <string>
#include <vector>

#include "base/LunaTaskList.h"
#include "base/LaunchPointList.h"
#include "base/AppDescriptionList.h"
#include "base/RunningAppList.h"
#include "bus/client/AppInstallService.h"
#include "bus/client/DB8.h"
#include "bus/client/LSM.h"
#include "conf/SAMConf.h"
#include "manager/PolicyManager.h"
#include "SchemaChecker.h"
#include "util/JValueUtil.h"
#include "util/Time.h"

const char* ApplicationManager::CATEGORY_ROOT = "/";
const char* ApplicationManager::CATEGORY_DEV = "/dev";

const char* ApplicationManager::METHOD_LAUNCH = "launch";
const char* ApplicationManager::METHOD_PAUSE = "pause";
const char* ApplicationManager::METHOD_CLOSE = "close";
const char* ApplicationManager::METHOD_CLOSE_BY_APPID = "closeByAppId";
const char* ApplicationManager::METHOD_RUNNING = "running";
const char* ApplicationManager::METHOD_GET_APP_LIFE_EVENTS ="getAppLifeEvents";
const char* ApplicationManager::METHOD_GET_APP_LIFE_STATUS = "getAppLifeStatus";
const char* ApplicationManager::METHOD_GET_FOREGROUND_APPINFO = "getForegroundAppInfo";
const char* ApplicationManager::METHOD_LOCK_APP = "lockApp";
const char* ApplicationManager::METHOD_REGISTER_APP = "registerApp";
const char* ApplicationManager::METHOD_REGISTER_NATIVE_APP = "registerNativeApp";

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
    { METHOD_CLOSE,                    ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_CLOSE_BY_APPID,           ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_RUNNING,                  ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_GET_APP_LIFE_EVENTS,      ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_GET_APP_LIFE_STATUS,      ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_GET_FOREGROUND_APPINFO,   ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_LOCK_APP,                 ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_REGISTER_APP,             ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
    { METHOD_REGISTER_NATIVE_APP,      ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },

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
    { METHOD_CLOSE,                    ApplicationManager::onAPICalled, LUNA_METHOD_FLAGS_NONE },
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

    lunaTask = make_shared<LunaTask>(request, requestPayload, message);
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
      m_enablePosting(false),
      m_compat1("com.webos.service.applicationmanager"),
      m_compat2("com.webos.service.applicationManager")
{
    setClassName("ApplicationManager");

    registerApiHandler(CATEGORY_ROOT, METHOD_LAUNCH, boost::bind(&ApplicationManager::launch, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_PAUSE, boost::bind(&ApplicationManager::pause, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_CLOSE, boost::bind(&ApplicationManager::close, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_CLOSE_BY_APPID, boost::bind(&ApplicationManager::close, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_RUNNING, boost::bind(&ApplicationManager::running, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_GET_APP_LIFE_EVENTS, boost::bind(&ApplicationManager::getAppLifeEvents, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_GET_APP_LIFE_STATUS, boost::bind(&ApplicationManager::getAppLifeStatus, this, _1));

    registerApiHandler(CATEGORY_ROOT, METHOD_GET_FOREGROUND_APPINFO, boost::bind(&ApplicationManager::getForegroundAppInfo, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_LOCK_APP, boost::bind(&ApplicationManager::lockApp, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_REGISTER_APP, boost::bind(&ApplicationManager::registerApp, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_REGISTER_NATIVE_APP, boost::bind(&ApplicationManager::registerNativeApp, this, _1));

    registerApiHandler(CATEGORY_ROOT, METHOD_LIST_APPS, boost::bind(&ApplicationManager::listApps, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_GET_APP_STATUS, boost::bind(&ApplicationManager::getAppStatus, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_GET_APP_INFO, boost::bind(&ApplicationManager::getAppInfo, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_GET_APP_BASE_PATH, boost::bind(&ApplicationManager::getAppBasePath, this, _1));

    registerApiHandler(CATEGORY_ROOT, METHOD_ADD_LAUNCHPOINT, boost::bind(&ApplicationManager::addLaunchPoint, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_UPDATE_LAUNCHPOINT, boost::bind(&ApplicationManager::updateLaunchPoint, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_REMOVE_LAUNCHPOINT, boost::bind(&ApplicationManager::removeLaunchPoint, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_MOVE_LAUNCHPOINT, boost::bind(&ApplicationManager::moveLaunchPoint, this, _1));
    registerApiHandler(CATEGORY_ROOT, METHOD_LIST_LAUNCHPOINTS, boost::bind(&ApplicationManager::listLaunchPoints, this, _1));

    registerApiHandler(CATEGORY_DEV, METHOD_CLOSE, boost::bind(&ApplicationManager::close, this, _1));
    registerApiHandler(CATEGORY_DEV, METHOD_CLOSE_BY_APPID, boost::bind(&ApplicationManager::close, this, _1));
    registerApiHandler(CATEGORY_DEV, METHOD_LIST_APPS, boost::bind(&ApplicationManager::listApps, this, _1));
    registerApiHandler(CATEGORY_DEV, METHOD_RUNNING, boost::bind(&ApplicationManager::running, this, _1));
    registerApiHandler(CATEGORY_DEV, METHOD_MANAGER_INFO, boost::bind(&ApplicationManager::managerInfo, this, _1));
}

ApplicationManager::~ApplicationManager()
{
}

bool ApplicationManager::attach(GMainLoop* gml)
{
    try {
        this->registerCategory(CATEGORY_ROOT, METHODS_ROOT, nullptr, nullptr);
        m_compat1.registerCategory(CATEGORY_ROOT, METHODS_ROOT, nullptr, nullptr);
        m_compat2.registerCategory(CATEGORY_ROOT, METHODS_ROOT, nullptr, nullptr);

        if (SAMConf::getInstance().isDevmodeEnabled()) {
            this->registerCategory(CATEGORY_DEV, METHODS_DEV, nullptr, nullptr);
            m_compat1.registerCategory(CATEGORY_DEV, METHODS_DEV, nullptr, nullptr);
            m_compat2.registerCategory(CATEGORY_DEV, METHODS_DEV, nullptr, nullptr);
        }

        m_getAppLifeEvents = new LS::SubscriptionPoint();               m_getAppLifeEvents->setServiceHandle(this);
        m_getAppLifeStatus = new LS::SubscriptionPoint();               m_getAppLifeStatus->setServiceHandle(this);
        m_getForgroundAppInfo = new LS::SubscriptionPoint();            m_getForgroundAppInfo->setServiceHandle(this);
        m_getForgroundAppInfoExtraInfo = new LS::SubscriptionPoint();   m_getForgroundAppInfoExtraInfo->setServiceHandle(this);
        m_listLaunchPointsPoint = new LS::SubscriptionPoint();          m_listLaunchPointsPoint->setServiceHandle(this);
        m_listAppsPoint = new LS::SubscriptionPoint();                  m_listAppsPoint->setServiceHandle(this);
        m_listAppsCompactPoint = new LS::SubscriptionPoint();           m_listAppsCompactPoint->setServiceHandle(this);
        m_listDevAppsPoint = new LS::SubscriptionPoint();               m_listDevAppsPoint->setServiceHandle(this);
        m_listDevAppsCompactPoint = new LS::SubscriptionPoint();        m_listDevAppsCompactPoint->setServiceHandle(this);
        m_running = new LS::SubscriptionPoint();                        m_running->setServiceHandle(this);
        m_runningDev = new LS::SubscriptionPoint();                     m_runningDev->setServiceHandle(this);

        this->attachToLoop(gml);
        m_compat1.attachToLoop(gml);
        m_compat2.attachToLoop(gml);
    } catch(exception& e) {
    }
    return true;
}

void ApplicationManager::detach()
{
    m_APIHandlers.clear();

    delete m_getAppLifeEvents;
    delete m_getAppLifeStatus;
    delete m_getForgroundAppInfo;
    delete m_getForgroundAppInfoExtraInfo;
    delete m_listLaunchPointsPoint;
    delete m_listAppsPoint;
    delete m_listAppsCompactPoint;
    delete m_listDevAppsPoint;
    delete m_listDevAppsCompactPoint;
    delete m_running;
    delete m_runningDev;

    Handle::detach();
    m_compat1.detach();
    m_compat2.detach();
}

void ApplicationManager::launch(LunaTaskPtr lunaTask)
{
    if (LaunchPointList::getInstance().getByLunaTask(lunaTask) == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Cannot find proper launchPoint");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    string reason = "";
    JValueUtil::getValue(lunaTask->getRequestPayload(), "params", "reason", reason);
    if (reason.empty()) {
        reason = "normal";
    }
    lunaTask->setReason(reason);

    RunningAppPtr runningApp = RunningAppList::getInstance().getByAppIdAndDisplayId(lunaTask->getAppId(), lunaTask->getDisplayAffinity());
    if (runningApp == nullptr)
        PolicyManager::getInstance().launch(lunaTask);
    else
        PolicyManager::getInstance().relaunch(lunaTask);
}

void ApplicationManager::pause(LunaTaskPtr lunaTask)
{
    RunningAppPtr runningApp = RunningAppList::getInstance().getByLunaTask(lunaTask);
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, lunaTask->getAppId() + " is not running");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    lunaTask->setErrCodeAndText(ErrCode_GENERAL, "This API is not implemented");
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
    return;
}

void ApplicationManager::close(LunaTaskPtr lunaTask)
{
    RunningAppPtr runningApp = RunningAppList::getInstance().getByLunaTask(lunaTask);
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, lunaTask->getAppId() + " is not running");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    if (strcmp(lunaTask->getRequest().getCategory(), "/dev") == 0 &&
        !runningApp->getLaunchPoint()->getAppDesc()->isDevmodeApp()) {
        Logger::warning(getClassName(), __FUNCTION__, lunaTask->getAppId(), "Only Dev app should be closed using /dev category_API");
        return;
    }

    bool preloadOnly = false;
    bool letAppHandle = false;
    string reason = "";

    JValueUtil::getValue(lunaTask->getRequestPayload(), "preloadOnly", preloadOnly);
    JValueUtil::getValue(lunaTask->getRequestPayload(), "reason", reason);
    JValueUtil::getValue(lunaTask->getRequestPayload(), "letAppHandle", letAppHandle);

    if (preloadOnly) {
        Logger::warning(getClassName(), __FUNCTION__, lunaTask->getAppId(), "app is being launched by user");
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
    bool isDevmode = (strcmp(lunaTask->getRequest().getCategory(), "/dev") == 0);
    bool subscribed = false;

    makeRunning(lunaTask->getResponsePayload(), isDevmode);
    lunaTask->getResponsePayload().put("returnValue", true);

    if (lunaTask->getRequest().isSubscription()) {
        if (isDevmode) {
            subscribed = m_runningDev->subscribe(lunaTask->getRequest());
        } else {
            subscribed = m_running->subscribe(lunaTask->getRequest());
        }
    }
    lunaTask->getResponsePayload().put("subscribed", subscribed);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::getAppLifeEvents(LunaTaskPtr lunaTask)
{
    if (!lunaTask->getRequest().isSubscription()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "subscription is required");
        lunaTask->getResponsePayload().put("subscribed", false);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    if (!m_getAppLifeEvents->subscribe(lunaTask->getRequest())) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Subscription failed");
        lunaTask->getResponsePayload().put("subscribed", false);
    } else {
        lunaTask->getResponsePayload().put("subscribed", true);
    }
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::getAppLifeStatus(LunaTaskPtr lunaTask)
{
    if (!lunaTask->getRequest().isSubscription()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "subscription is required");
        lunaTask->getResponsePayload().put("subscribed", false);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    if (!m_getAppLifeStatus->subscribe(lunaTask->getRequest())) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Subscription failed");
        lunaTask->getResponsePayload().put("subscribed", false);
    } else {
        lunaTask->getResponsePayload().put("subscribed", true);
    }
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::getForegroundAppInfo(LunaTaskPtr lunaTask)
{
    bool extraInfo = false;
    bool subscribed = false;

    JValueUtil::getValue(lunaTask->getRequestPayload(), "extraInfo", extraInfo);
    makeGetForegroundAppInfo(lunaTask->getResponsePayload(), extraInfo);

    if (lunaTask->getRequest().isSubscription()) {
        if (extraInfo) {
            subscribed = m_getForgroundAppInfoExtraInfo->subscribe(lunaTask->getRequest());
        } else {
            subscribed = m_getForgroundAppInfo->subscribe(lunaTask->getRequest());
        }
    }
    lunaTask->getResponsePayload().put("subscribed", subscribed);
    lunaTask->getResponsePayload().put("returnValue", true);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void ApplicationManager::lockApp(LunaTaskPtr lunaTask)
{
    string appId;
    bool lock;

    JValueUtil::getValue(lunaTask->getRequestPayload(), "id", appId);
    JValueUtil::getValue(lunaTask->getRequestPayload(), "lock", lock);

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getByAppId(appId);
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
    if (lunaTask->getRequest().getApplicationID() == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "The API should be called by application.");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    string appId = lunaTask->getRequest().getApplicationID();
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
    if (lunaTask->getRequest().getApplicationID() == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "The API should be called by application.");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    string appId = lunaTask->getRequest().getApplicationID();
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
        lunaTask->getResponsePayload().put("subscribed", LSSubscriptionAdd(this->get(), METHOD_LIST_APPS, lunaTask->getMessage(), nullptr));
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
        string subscriptionKey = "getappstatus#" + appId + "#" + (appInfo ? "Y" : "N");
        if (LSSubscriptionAdd(this->get(), subscriptionKey.c_str(), lunaTask->getMessage(), NULL)) {
            lunaTask->getResponsePayload().put("subscribed", true);
        } else {
            lunaTask->getResponsePayload().put("subscribed", false);
        }
    }

    // for first return (event: nothing)
    lunaTask->getResponsePayload().put("appId", appId);
    lunaTask->getResponsePayload().put("event", "nothing");

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getByAppId(appId);
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

    string appId = requestPayload["id"].asString();
    if (appId.empty()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid appId specified");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getByAppId(appId);
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

    string appId = requestPayload["appId"].asString();

    if (appId.empty()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid appId specified");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getByAppId(appId);
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

    if (lunaTask->getAppId().empty()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "missing required 'id' parameter");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getByAppId(lunaTask->getAppId());
    if (!appDesc) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid appId specified OR Unsupported Application Type: " + lunaTask->getAppId());
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    LaunchPointPtr launchPoint = LaunchPointList::getInstance().createBootmarkByAPI(appDesc, requestPayload);
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

    string launchPointId = "";
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
    LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByLaunchPointId(lunaTask->getLaunchPointId());
    if (launchPoint == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Cannot find launch point");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    if (!launchPoint->isRemovable()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "This launchPoint cannot be removable");
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

    string launchPointId = "";
    string errorText = "";
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
        ApplicationManager::getInstance().m_listLaunchPointsPoint->subscribe(lunaTask->getRequest());

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

void ApplicationManager::postGetAppLifeEvents(RunningApp& runningApp)
{
    if (!m_enablePosting) return;

    pbnjson::JValue info = pbnjson::JValue();
    pbnjson::JValue subscriptionPayload = pbnjson::Object();
    subscriptionPayload.put("instanceId", runningApp.getInstanceId());
    subscriptionPayload.put("launchPointId", runningApp.getLaunchPointId());
    subscriptionPayload.put("appId", runningApp.getAppId());
    subscriptionPayload.put("returnValue", true);
    subscriptionPayload.put("subscribed", true);

    switch (runningApp.getLifeStatus()) {
    case LifeStatus::LifeStatus_PRELOADED:
        subscriptionPayload.put("event", "preload");
        subscriptionPayload.put("preload", runningApp.getPreload());
        break;

    case LifeStatus::LifeStatus_SPLASHING:
        subscriptionPayload.put("event", "splash");
        subscriptionPayload.put("title", runningApp.getLaunchPoint()->getTitle());
        subscriptionPayload.put("splashBackground", runningApp.getLaunchPoint()->getAppDesc()->getSplashBackground());
        subscriptionPayload.put("showSplash", runningApp.isShowSplash());
        subscriptionPayload.put("showSpinner", runningApp.isShowSpinner());
        break;

    case LifeStatus::LifeStatus_LAUNCHING:
    case LifeStatus::LifeStatus_RELAUNCHING:
        subscriptionPayload.put("event", "launch");
        subscriptionPayload.put("reason", runningApp.getReason());
        break;

    case LifeStatus::LifeStatus_STOP:
        subscriptionPayload.put("event", "stop");
        subscriptionPayload.put("reason", runningApp.getReason());
        break;

    case LifeStatus::LifeStatus_CLOSING:
        subscriptionPayload.put("reason", runningApp.getReason());
        subscriptionPayload.put("event", "close");
        break;

    case LifeStatus::LifeStatus_FOREGROUND:
        subscriptionPayload.put("event", "foreground");
        subscriptionPayload.put("reason", runningApp.getReason());
        LSM::getInstance().getForegroundInfoById(runningApp.getAppId(), info);
        if (!info.isNull() && info.isObject()) {
            for (auto it : info.children()) {
                const string key = it.first.asString();
                if ("windowType" == key || "windowGroup" == key || "windowGroupOwner" == key || "windowGroupOwnerId" == key || "displayId" == key) {
                    subscriptionPayload.put(key, info[key]);
                }
            }
        }
        break;

    case LifeStatus::LifeStatus_BACKGROUND:
        subscriptionPayload.put("event", "background");
        if (runningApp.isKeepAlive()) {
            subscriptionPayload.put("status", "preload");
        } else {
            subscriptionPayload.put("status", "normal");
        }
        break;

    case LifeStatus::LifeStatus_PAUSED:
        subscriptionPayload.put("event", "pause");
        break;

    default:
        return;
    };

    Logger::logSubscriptionPost(getClassName(), __FUNCTION__, *m_getAppLifeEvents, subscriptionPayload);
    m_getAppLifeEvents->post(subscriptionPayload.stringify().c_str());
}

void ApplicationManager::postGetAppLifeStatus(RunningApp& runningApp)
{
    if (!m_enablePosting) return;

    pbnjson::JValue subscriptionPayload = pbnjson::Object();
    subscriptionPayload.put("returnValue", true);
    subscriptionPayload.put("subscribed", true);

    runningApp.toJson(subscriptionPayload, true);
    pbnjson::JValue foregroundInfo = pbnjson::JValue();
    switch(runningApp.getLifeStatus()) {
    case LifeStatus::LifeStatus_LAUNCHING:
    case LifeStatus::LifeStatus_RELAUNCHING:
    case LifeStatus::LifeStatus_CLOSING:
    case LifeStatus::LifeStatus_STOP:
        break;

    case LifeStatus::LifeStatus_FOREGROUND:
        LSM::getInstance().getForegroundInfoById(runningApp.getAppId(), foregroundInfo);
        if (!foregroundInfo.isNull() && foregroundInfo.isObject()) {
            for (auto it : foregroundInfo.children()) {
                const string key = it.first.asString();
                if ("windowType" == key ||
                    "windowGroup" == key ||
                    "windowGroupOwner" == key ||
                    "windowGroupOwnerId" == key ||
                    "displayId" == key) {
                    subscriptionPayload.put(key, foregroundInfo[key]);
                }
            }
        }
        break;

    case LifeStatus::LifeStatus_BACKGROUND:
        if (runningApp.isKeepAlive())
            subscriptionPayload.put("backgroundStatus", "preload");
        else
            subscriptionPayload.put("backgroundStatus", "normal");
        break;

    default:
        return;
    }

    Logger::logSubscriptionPost(getClassName(), __FUNCTION__, *m_getAppLifeStatus, subscriptionPayload);
    m_getAppLifeStatus->post(subscriptionPayload.stringify().c_str());
}

void ApplicationManager::postGetAppStatus(AppDescriptionPtr appDesc, AppStatusEvent event)
{
    if (!m_enablePosting) return;
    if (!appDesc) return;

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

    string nKey = "getappstatus#" + appDesc->getAppId() + "#N";
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

    string yKey = "getappstatus#" + appDesc->getAppId() + "#Y";
    Logger::logSubscriptionPost(getClassName(), __FUNCTION__, yKey, subscriptionPayload);
    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(), yKey.c_str(), subscriptionPayload.stringify().c_str(), NULL)) {
        Logger::warning(getClassName(), __FUNCTION__, "Failed to post subscription");
    }
}

void ApplicationManager::postGetForegroundAppInfo(bool extraInfoOnly)
{
    if (!m_enablePosting) return;

    pbnjson::JValue subscriptionPayload;
    if (!extraInfoOnly) {
        subscriptionPayload = pbnjson::Object();
        makeGetForegroundAppInfo(subscriptionPayload, false);
        subscriptionPayload.put("returnValue", true);
        subscriptionPayload.put("subscribed", true);

        Logger::logSubscriptionPost(getClassName(), __FUNCTION__, *m_getForgroundAppInfo, subscriptionPayload);
        m_getForgroundAppInfo->post(subscriptionPayload.stringify().c_str());
    }

    subscriptionPayload = pbnjson::Object();
    makeGetForegroundAppInfo(subscriptionPayload, true);
    subscriptionPayload.put("returnValue", true);
    subscriptionPayload.put("subscribed", true);

    Logger::logSubscriptionPost(getClassName(), __FUNCTION__, *m_getForgroundAppInfoExtraInfo, subscriptionPayload);
    m_getForgroundAppInfoExtraInfo->post(subscriptionPayload.stringify().c_str());
}

void ApplicationManager::postListApps(AppDescriptionPtr appDesc, const string& change, const string& changeReason)
{
    if (!m_enablePosting) return;

    JValue subscriptionPayload = pbnjson::Object();
    subscriptionPayload.put("returnValue", true);
    subscriptionPayload.put("subscribed", true);

    if (!change.empty())
        subscriptionPayload.put("change", change);
    if (!changeReason.empty())
        subscriptionPayload.put("changeReason", changeReason);

    Logger::info(getClassName(), __FUNCTION__, "SubscriptionPost", change);
    LSSubscriptionIter *iter = NULL;
    if (!LSSubscriptionAcquire(ApplicationManager::getInstance().get(), METHOD_LIST_APPS, &iter, NULL))
        return;
    while (LSSubscriptionHasNext(iter)) {
        LSMessage* message = LSSubscriptionNext(iter);
        Message request(message);
        bool isDevmode = (strcmp(request.getKind(), "/dev/listApps") == 0);

        if (isDevmode && !SAMConf::getInstance().isDevmodeEnabled()) {
            Logger::debug(getClassName(), __FUNCTION__, "Devmode is disabled");
            continue;
        }

        pbnjson::JValue requestPayload = JDomParser::fromString(request.getPayload(), JValueUtil::getSchema("applicationManager.listApps"));
        if (requestPayload.isNull()) {
            Logger::warning(getClassName(), __FUNCTION__, "Failed to parse requestPayload");
            continue;
        }

        JValue properties = pbnjson::Array();
        if (JValueUtil::getValue(requestPayload, "properties", properties) && properties.isArray()) {
            properties.append("id");
        }

        if (appDesc == nullptr) {
            pbnjson::JValue apps = pbnjson::Array();
            AppDescriptionList::getInstance().toJson(apps, properties, isDevmode);
            subscriptionPayload.put("apps", apps);
        } else {
            if (appDesc->isDevmodeApp() != isDevmode) {
                Logger::debug(getClassName(), __FUNCTION__, "Devmode != DevmodeApp");
                continue;
            }
            pbnjson::JValue app = appDesc->getJson(properties);
            subscriptionPayload.put("app", app);
        }
        Logger::debug(getClassName(), __FUNCTION__, request.getSenderServiceName());
        request.respond(subscriptionPayload.stringify().c_str());
    }
    LSSubscriptionRelease(iter);
    iter = NULL;
}

void ApplicationManager::postListLaunchPoints(LaunchPointPtr launchPoint, string change)
{
    if (!m_enablePosting) return;

    if (launchPoint != nullptr && !launchPoint->isVisible())
        return;

    pbnjson::JValue subscriptionPayload = pbnjson::Object();


    if (launchPoint) {
        launchPoint->toJson(subscriptionPayload);
        subscriptionPayload.put("launchPoint", subscriptionPayload.duplicate());
    } else {
        pbnjson::JValue launchPoints = pbnjson::Array();
        LaunchPointList::getInstance().toJson(launchPoints);
    }

    subscriptionPayload.put("subscribed", true);
    subscriptionPayload.put("returnValue", true);
    if (!change.empty())
        subscriptionPayload.put("change", change);

    Logger::logSubscriptionPost(getClassName(), __FUNCTION__, *m_listLaunchPointsPoint, subscriptionPayload);
    m_listLaunchPointsPoint->post(subscriptionPayload.stringify().c_str());
}

void ApplicationManager::postRunning(RunningAppPtr runningApp)
{
    static JValue prevSubscriptionPayloadAll;
    static JValue prevSubscriptionPayloadDev;

    if (!m_enablePosting) return;

    pbnjson::JValue subscriptionPayload;
    if (runningApp == nullptr || runningApp->getLaunchPoint()->getAppDesc()->isDevmodeApp()) {
        subscriptionPayload = pbnjson::Object();
        makeRunning(subscriptionPayload, true);
        subscriptionPayload.put("subscribed", true);
        subscriptionPayload.put("returnValue", true);

        if (subscriptionPayload != prevSubscriptionPayloadDev) {
            prevSubscriptionPayloadDev = subscriptionPayload.duplicate();
            Logger::logSubscriptionPost(getClassName(), __FUNCTION__, *m_runningDev, subscriptionPayload);
            m_runningDev->post(subscriptionPayload.stringify().c_str());
        }
    }
    subscriptionPayload = pbnjson::Object();
    makeRunning(subscriptionPayload, false);
    subscriptionPayload.put("subscribed", true);
    subscriptionPayload.put("returnValue", true);

    if (subscriptionPayload == prevSubscriptionPayloadAll) return;
    prevSubscriptionPayloadAll = subscriptionPayload.duplicate();
    Logger::logSubscriptionPost(getClassName(), __FUNCTION__, *m_running, subscriptionPayload);
    m_running->post(subscriptionPayload.stringify().c_str());
}

void ApplicationManager::makeGetForegroundAppInfo(JValue& payload, bool extraInfo)
{
    if (extraInfo) {
        payload.put("foregroundAppInfo", LSM::getInstance().getForegroundInfo());
    } else {
        string appId = LSM::getInstance().getFullWindowAppId();
        RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(appId);
        if (runningApp == nullptr) {
            payload.put("appId", "");
            payload.put("instanceId", "");
            payload.put("launchPointId", "");
        } else {
            payload.put("appId", runningApp->getAppId());
            payload.put("instanceId", runningApp->getInstanceId());
            payload.put("launchPointId", runningApp->getLaunchPointId());

            payload.put("windowId", runningApp->getWindowId());
            payload.put("processId", runningApp->getProcessId());
        }
    }
}

void ApplicationManager::makeRunning(JValue& payload, bool isDevmode)
{
    pbnjson::JValue running = pbnjson::Array();
    RunningAppList::getInstance().toJson(running, isDevmode);
    payload.put("running", running);
}