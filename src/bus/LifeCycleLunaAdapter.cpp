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

#include <boost/bind.hpp>
#include <bus/AppMgrService.h>
#include <bus/AppMgrService.h>
#include <bus/LifeCycleLunaAdapter.h>
#include <bus/LunaserviceAPI.h>
#include <lifecycle/LifeCycleManager.h>
#include <package/PackageManager.h>
#include <util/Logging.h>

#define SUBSKEY_RUNNING              "running"
#define SUBSKEY_DEV_RUNNING          "dev_running"
#define SUBSKEY_FOREGROUND_INFO      "foregroundAppInfo"
#define SUBSKEY_FOREGROUND_INFO_EX   "foregroundAppInfoEx"
#define SUBSKEY_GET_APP_LIFE_EVENTS  "getAppLifeEvents"
#define SUBSKEY_GET_APP_LIFE_STATUS  "getAppLifeStatus"
#define SUBSKEY_ON_LAUNCH            "onLaunch"

LifeCycleLunaAdapter::LifeCycleLunaAdapter()
{
}

LifeCycleLunaAdapter::~LifeCycleLunaAdapter()
{
}

void LifeCycleLunaAdapter::init()
{
    initLunaApiHandler();

    AppMgrService::instance().signalOnServiceReady.connect(std::bind(&LifeCycleLunaAdapter::onReady, this));

    PackageManager::instance().appScanner().signalAppScanFinished.connect(boost::bind(&LifeCycleLunaAdapter::onScanFinished, this, _1, _2));

    LifecycleManager::instance().signal_foreground_app_changed.connect(boost::bind(&LifeCycleLunaAdapter::onForegroundAppChanged, this, _1));

    LifecycleManager::instance().signal_foreground_extra_info_changed.connect(boost::bind(&LifeCycleLunaAdapter::onExtraForegroundInfoChanged, this, _1));

    LifecycleManager::instance().signal_lifecycle_event.connect(boost::bind(&LifeCycleLunaAdapter::onLifeCycleEventGenarated, this, _1));
}

void LifeCycleLunaAdapter::initLunaApiHandler()
{
    // general category (/)
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_LAUNCH, "applicationManager.launch", boost::bind(&LifeCycleLunaAdapter::requestController, this, _1));
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_PAUSE, "", boost::bind(&LifeCycleLunaAdapter::pause, this, _1));
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_CLOSE_BY_APPID, "applicationManager.closeByAppId", boost::bind(&LifeCycleLunaAdapter::closeByAppId, this, _1));
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_RUNNING, "applicationManager.running", boost::bind(&LifeCycleLunaAdapter::running, this, _1));
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_GET_APP_LIFE_EVENTS, "", boost::bind(&LifeCycleLunaAdapter::getAppLifeEvents, this, _1));
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_GET_APP_LIFE_STATUS, "", boost::bind(&LifeCycleLunaAdapter::getAppLifeStatus, this, _1));
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_GET_FOREGROUND_APPINFO, "applicationManager.getForegroundAppInfo",
            boost::bind(&LifeCycleLunaAdapter::getForegroundAppInfo, this, _1));
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_LOCK_APP, "applicationManager.lockApp", boost::bind(&LifeCycleLunaAdapter::lockApp, this, _1));
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_REGISTER_APP, "", boost::bind(&LifeCycleLunaAdapter::registerApp, this, _1));

    // dev category
    AppMgrService::instance().registerApiHandler(API_CATEGORY_DEV, API_CLOSE_BY_APPID, "applicationManager.closeByAppId", boost::bind(&LifeCycleLunaAdapter::closeByAppIdForDev, this, _1));
    AppMgrService::instance().registerApiHandler(API_CATEGORY_DEV, API_RUNNING, "applicationManager.running", boost::bind(&LifeCycleLunaAdapter::runningForDev, this, _1));
}

void LifeCycleLunaAdapter::requestController(LunaTaskPtr task)
{
    if (API_LAUNCH == task->method()) {
        std::string app_id = task->jmsg()["id"].asString();
        AppDescPtr app_desc = PackageManager::instance().getAppById(app_id);
        if (app_desc == NULL && !AppMgrService::instance().isServiceReady()) {
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
            m_pendingTasksOnScanner.push_back(task);
            return;
        }
    }

    handleRequest(task);
}

void LifeCycleLunaAdapter::onReady()
{
    auto it = m_pendingTasksOnReady.begin();
    while (it != m_pendingTasksOnReady.end()) {
        handleRequest(*it);
        it = m_pendingTasksOnReady.erase(it);
    }
}

void LifeCycleLunaAdapter::onScanFinished(ScanMode mode, const AppDescMaps& scannced_apps)
{
    auto it = m_pendingTasksOnScanner.begin();
    while (it != m_pendingTasksOnScanner.end()) {
        handleRequest(*it);
        it = m_pendingTasksOnScanner.erase(it);
    }
}

void LifeCycleLunaAdapter::handleRequest(LunaTaskPtr task)
{
    if (API_CATEGORY_GENERAL == task->category()) {
        if (API_LAUNCH == task->method())
            launch(task);
    }
}

void LifeCycleLunaAdapter::launch(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string id = jmsg["id"].asString();
    if (id.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "App ID is not specified");
        return;
    }

    std::string launch_mode = "normal";
    if (jmsg.hasKey("params") && jmsg["params"].hasKey("reason"))
        launch_mode = jmsg["params"]["reason"].asString();

    LOG_NORMAL(NLID_APP_LAUNCH_BEGIN, 3,
               PMLOGKS("app_id", id.c_str()),
               PMLOGKS("caller_id", task->caller().c_str()),
               PMLOGKS("mode", launch_mode.c_str()), "");

    LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(task);
    lifecycle_task->setAppId(id);
    LifecycleManager::instance().Launch(lifecycle_task);
}

void LifeCycleLunaAdapter::pause(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string id = jmsg["id"].asString();
    if (id.length() == 0) {
        pbnjson::JValue payload = pbnjson::Object();
        task->replyResultWithError(API_ERR_CODE_GENERAL, "App ID is not specified");
        return;
    }

    LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(task);
    lifecycle_task->setAppId(id);
    LifecycleManager::instance().Pause(lifecycle_task);
}

void LifeCycleLunaAdapter::closeByAppId(LunaTaskPtr task)
{
    LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(task);
    LifecycleManager::instance().Close(lifecycle_task);
}

void LifeCycleLunaAdapter::closeByAppIdForDev(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();
    std::string appId = jmsg["id"].asString();
    AppDescPtr appDescPtr = PackageManager::instance().getAppById(appId);

    if (AppTypeByDir::AppTypeByDir_Dev != appDescPtr->getTypeByDir()) {
        LOG_WARNING(MSGID_APPCLOSE_ERR, 1,
                    PMLOGKS("app_id", appId.c_str()),
                    "only dev apps should be closed in devmode");
        task->replyResultWithError(API_ERR_CODE_GENERAL, "Only Dev app should be closed using /dev category_API");
        return;
    }

    LifeCycleTaskPtr lifecycleTask = std::make_shared<LifeCycleTask>(task);
    LifecycleManager::instance().Close(lifecycleTask);
}

void LifeCycleLunaAdapter::closeAllApps(LunaTaskPtr task)
{
    LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(task);
    LifecycleManager::instance().CloseAll(lifecycle_task);
    task->replyResult();
}

void LifeCycleLunaAdapter::running(LunaTaskPtr task)
{
    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue running_list = pbnjson::Array();
    AppInfoManager::instance().getRunningList(running_list);

    payload.put("returnValue", true);
    payload.put("running", running_list);
    if (LSMessageIsSubscription(task->lsmsg())) {
        payload.put("subscribed", LSSubscriptionAdd(task->lshandle(), SUBSKEY_RUNNING, task->lsmsg(), NULL));
    }

    task->ReplyResult(payload);
}

void LifeCycleLunaAdapter::runningForDev(LunaTaskPtr task)
{
    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue running_list = pbnjson::Array();
    AppInfoManager::instance().getRunningList(running_list, true);

    payload.put("returnValue", true);
    payload.put("running", running_list);
    if (LSMessageIsSubscription(task->lsmsg())) {
        payload.put("subscribed", LSSubscriptionAdd(task->lshandle(), SUBSKEY_DEV_RUNNING, task->lsmsg(), NULL));
    }

    task->ReplyResult(payload);
}

void LifeCycleLunaAdapter::changeRunningAppId(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    pbnjson::JValue response = pbnjson::Object();
    std::string caller_id = task->caller();
    std::string target_id = jmsg["id"].asString();
    ErrorInfo err_info;

    if (caller_id != "com.webos.app.inputcommon") {
        task->replyResultWithError(PERMISSION_DENIED, "only 'com.webos.app.inputcommon' can call this API");
        return;
    }

    if (NULL == PackageManager::instance().getAppById(target_id)) {
        task->replyResultWithError(APP_ERR_INVALID_APPID, "Cannot find target appId.");
        return;
    }

    if (!LifecycleManager::instance().changeRunningAppId(caller_id, target_id, err_info)) {
        task->replyResultWithError(err_info.errorCode, err_info.errorText);
        return;
    }

    task->replyResult();
}

void LifeCycleLunaAdapter::getAppLifeEvents(LunaTaskPtr task)
{
    pbnjson::JValue payload = pbnjson::Object();

    if (LSMessageIsSubscription(task->lsmsg())) {
        if (!LSSubscriptionAdd(task->lshandle(), SUBSKEY_GET_APP_LIFE_EVENTS, task->lsmsg(), NULL)) {
            task->setError(API_ERR_CODE_GENERAL, "Subscription failed");
            payload.put("subscribed", false);
        } else {
            payload.put("subscribed", true);
        }
    } else {
        task->setError(API_ERR_CODE_GENERAL, "subscription is required");
        payload.put("subscribed", false);
    }

    task->ReplyResult(payload);
}

void LifeCycleLunaAdapter::getAppLifeStatus(LunaTaskPtr task)
{
    pbnjson::JValue payload = pbnjson::Object();

    if (LSMessageIsSubscription(task->lsmsg())) {
        if (!LSSubscriptionAdd(task->lshandle(), SUBSKEY_GET_APP_LIFE_STATUS, task->lsmsg(), NULL)) {
            task->setError(API_ERR_CODE_GENERAL, "Subscription failed");
            payload.put("subscribed", false);
        } else {
            payload.put("subscribed", true);
        }
    } else {
        task->setError(API_ERR_CODE_GENERAL, "subscription is required");
        payload.put("subscribed", false);
    }

    task->ReplyResult(payload);
}

void LifeCycleLunaAdapter::getForegroundAppInfo(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);

    if (jmsg["extraInfo"].asBool() == true) {
        payload.put("foregroundAppInfo", AppInfoManager::instance().getJsonForegroundInfo());
        if (LSMessageIsSubscription(task->lsmsg())) {
            payload.put("subscribed", LSSubscriptionAdd(task->lshandle(), SUBSKEY_FOREGROUND_INFO_EX, task->lsmsg(), NULL));
        }
    } else {
        payload.put("appId", AppInfoManager::instance().getCurrentForegroundAppId());
        payload.put("windowId", "");
        payload.put("processId", "");
        if (LSMessageIsSubscription(task->lsmsg())) {
            payload.put("subscribed", LSSubscriptionAdd(task->lshandle(), SUBSKEY_FOREGROUND_INFO, task->lsmsg(), NULL));
        }
    }

    task->ReplyResult(payload);
}

void LifeCycleLunaAdapter::lockApp(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string app_id = jmsg["id"].asString();
    bool lock = jmsg["lock"].asBool();
    std::string error_text;

    // TODO: move this property into app info manager
    (void) PackageManager::instance().lockAppForUpdate(app_id, lock, error_text);

    if (!error_text.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, error_text);
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("id", app_id);
    payload.put("locked", lock);

    task->ReplyResult(payload);
}

void LifeCycleLunaAdapter::registerApp(LunaTaskPtr task)
{
    if (0 == task->caller().length()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "empty_app_id"),
                  PMLOGKS("where", "register_app"), "");
        task->replyResultWithError(API_ERR_CODE_GENERAL, "cannot find caller id");
        return;
    }

    std::string error_text;
    LifecycleManager::instance().registerApp(task->caller(), task->lsmsg(), error_text);

    if (!error_text.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, error_text);
        return;
    }
}

void LifeCycleLunaAdapter::registerNativeApp(LunaTaskPtr task)
{
    if (0 == task->caller().length()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "empty_app_id"),
                  PMLOGKS("where", "register_native_app"), "");
        task->replyResultWithError(API_ERR_CODE_GENERAL, "cannot find caller id");
        return;
    }

    std::string error_text;
    LifecycleManager::instance().connectNativeApp(task->caller(), task->lsmsg(), error_text);

    if (!error_text.empty()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, error_text);
        return;
    }
}

void LifeCycleLunaAdapter::notifyAlertClosed(LunaTaskPtr task)
{
    LifecycleManager::instance().handleBridgedLaunchRequest(task->jmsg());
    task->replyResult();
}

// subscription adapter to reply
void LifeCycleLunaAdapter::onForegroundAppChanged(const std::string& app_id)
{

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);
    payload.put("subscribed", true);
    payload.put("appId", app_id);
    payload.put("windowId", "");
    payload.put("processId", "");

    LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2,
             PMLOGKS("skey", SUBSKEY_FOREGROUND_INFO),
             PMLOGJSON("payload", payload.stringify().c_str()), "");

    if (!LSSubscriptionReply(AppMgrService::instance().serviceHandle(),
    SUBSKEY_FOREGROUND_INFO, payload.stringify().c_str(), NULL)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "subscriptionreply"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", __FUNCTION__), "");
        return;
    }
}

void LifeCycleLunaAdapter::onExtraForegroundInfoChanged(const pbnjson::JValue& foreground_info)
{
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);
    payload.put("subscribed", true);
    payload.put("foregroundAppInfo", foreground_info);

    LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2,
             PMLOGKS("skey", SUBSKEY_FOREGROUND_INFO_EX),
             PMLOGJSON("payload", payload.stringify().c_str()), "");
    if (!LSSubscriptionReply(AppMgrService::instance().serviceHandle(),
                             SUBSKEY_FOREGROUND_INFO_EX,
                             payload.stringify().c_str(),
                             NULL)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "subscriptionreply"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", __FUNCTION__), "");
        return;
    }
}

void LifeCycleLunaAdapter::onLifeCycleEventGenarated(const pbnjson::JValue& event)
{

    pbnjson::JValue payload = event.duplicate();
    payload.put("returnValue", true);

    LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2,
             PMLOGKS("skey", SUBSKEY_GET_APP_LIFE_EVENTS),
             PMLOGJSON("payload", payload.stringify().c_str()), "");
    if (!LSSubscriptionReply(AppMgrService::instance().serviceHandle(),
                             SUBSKEY_GET_APP_LIFE_EVENTS,
                             payload.stringify().c_str(),
                             NULL)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "subscriptionreply"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", __FUNCTION__), "");
        return;
    }
}
