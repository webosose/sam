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

#include <boost/signals2.hpp>
#include <bus/AppMgrService.h>
#include <bus/LSMSubscriber.h>
#include <bus/LunaserviceAPI.h>
#include <lifecycle/LifecycleManager.h>
#include <package/PackageManager.h>
#include <setting/Settings.h>
#include <unistd.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/LSUtils.h>
#include <util/JValueUtil.h>
#include <util/Utils.h>

#define SAM_INTERNAL_ID  "com.webos.applicationManager"
#define SLEEP_TIME_TO_CLOSE_FULLSCREEN_APP 500000

LifecycleManager::LifecycleManager()
{
}

LifecycleManager::~LifecycleManager()
{
}

void LifecycleManager::initialize()
{
    m_lifecycleRouter.init();
    AppInfoManager::instance().initialize();
    m_prelauncher.signal_prelaunching_done.connect(boost::bind(&LifecycleManager::onPrelaunchingDone, this, _1));

    m_memoryChecker.signal_memory_checking_start.connect(boost::bind(&LifecycleManager::onMemoryCheckingStart, this, _1));
    m_memoryChecker.signal_memory_checking_done.connect(boost::bind(&LifecycleManager::onMemoryCheckingDone, this, _1));

    // receive signal on service disconnected
    m_webLifecycleHandler.signal_service_disconnected.connect(boost::bind(&LifecycleManager::stopAllWebappItem, this));

    // receive signal on life status change
    NativeAppLifeHandler::getInstance().signal_app_life_status_changed.connect(boost::bind(&LifecycleManager::onRuntimeStatusChanged, this, _1, _2, _3));
    m_webLifecycleHandler.signal_app_life_status_changed.connect(boost::bind(&LifecycleManager::onRuntimeStatusChanged, this, _1, _2, _3));
    m_qmlLifecycleHandler.signal_app_life_status_changed.connect(boost::bind(&LifecycleManager::onRuntimeStatusChanged, this, _1, _2, _3));

    // receive signal on running list change
    NativeAppLifeHandler::getInstance().signal_running_app_added.connect(boost::bind(&LifecycleManager::onRunningAppAdded, this, _1, _2, _3));
    m_webLifecycleHandler.signal_running_app_added.connect(boost::bind(&LifecycleManager::onRunningAppAdded, this, _1, _2, _3));
    m_qmlLifecycleHandler.signal_running_app_added.connect(boost::bind(&LifecycleManager::onRunningAppAdded, this, _1, _2, _3));
    NativeAppLifeHandler::getInstance().signal_running_app_removed.connect(boost::bind(&LifecycleManager::onRunningAppRemoved, this, _1));
    m_webLifecycleHandler.signal_running_app_removed.connect(boost::bind(&LifecycleManager::onRunningAppRemoved, this, _1));
    m_qmlLifecycleHandler.signal_running_app_removed.connect(boost::bind(&LifecycleManager::onRunningAppRemoved, this, _1));

    // receive signal on launching done
    NativeAppLifeHandler::getInstance().signal_launching_done.connect(boost::bind(&LifecycleManager::onLaunchingDone, this, _1));
    m_webLifecycleHandler.signal_launching_done.connect(boost::bind(&LifecycleManager::onLaunchingDone, this, _1));
    m_qmlLifecycleHandler.signal_launching_done.connect(boost::bind(&LifecycleManager::onLaunchingDone, this, _1));

    // subscriber lsm's foreground info
    LSMSubscriber::instance().signal_foreground_info.connect(boost::bind(&LifecycleManager::onForegroundInfoChanged, this, _1));

    // set fullscreen window type
    m_fullscreenWindowTypes = SettingsImpl::instance().m_fullscreenWindowTypes;
}

void LifecycleManager::launch(LifeCycleTaskPtr task)
{
    LaunchAppItemPtr item = AppItemFactory::createLaunchItem(task);
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", task->getAppId().c_str()),
                  PMLOGKS("reason", "creating_item_fail"),
                  PMLOGKS("where", "launch_in_app_life_manager"), "");
        replyWithResult(task->getLunaTask()->lsmsg(), "", false, -101, "not exist");
        return;
    }

    // set start time
    item->setLaunchStartTime(getCurrentTime());

    // put new request into launching queue
    m_appLaunchingItemList.push_back(item);

    // start prelaunching
    runWithPrelauncher(item);
}

void LifecycleManager::Pause(LifeCycleTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getLunaTask()->jmsg();

    const pbnjson::JValue& params = jmsg.hasKey("params") && jmsg["params"].isObject() ? jmsg["params"] : pbnjson::Object();
    std::string err_text;
    pauseApp(task->getAppId(), params, err_text);

    if (!err_text.empty()) {
        task->finalizeWithError(API_ERR_CODE_GENERAL, err_text);
        return;
    }
    task->finalize();
}

void LifecycleManager::Close(LifeCycleTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getLunaTask()->jmsg();

    std::string appId = jmsg["id"].asString();
    std::string callerId = task->getLunaTask()->caller();
    std::string errorText;
    bool preloadOnly = false;
    bool handlePauseAppSelf = false;
    std::string reason;

    LOG_INFO(MSGID_APPCLOSE, 2,
             PMLOGKS("app_id", appId.c_str()),
             PMLOGKS("caller", callerId.c_str()),
             "params: %s", jmsg.duplicate().stringify().c_str());

    if (jmsg.hasKey("preloadOnly"))
        preloadOnly = jmsg["preloadOnly"].asBool();
    if (jmsg.hasKey("reason"))
        reason = jmsg["reason"].asString();
    if (jmsg.hasKey("letAppHandle"))
        handlePauseAppSelf = jmsg["letAppHandle"].asBool();

    if (handlePauseAppSelf) {
        pauseApp(appId, pbnjson::Object(), errorText, false);
    } else {
        LifecycleManager::instance().closeByAppId(appId, callerId, reason, errorText, preloadOnly, false);
    }

    if (!errorText.empty()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", appId.c_str()), "err_text: %s", errorText.c_str());
        task->finalizeWithError(API_ERR_CODE_GENERAL, errorText);
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("appId", appId);
    task->finalize(payload);
}

void LifecycleManager::CloseByPid(LifeCycleTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getLunaTask()->jmsg();

    std::string pid, err_text;
    std::string caller_id = task->getLunaTask()->caller();

    pid = jmsg["processId"].asString();

    LifecycleManager::instance().closeByPid(pid, caller_id, err_text);

    if (!err_text.empty()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("pid", pid.c_str()), "err_text: %s", err_text.c_str());
        task->finalizeWithError(API_ERR_CODE_GENERAL, err_text);
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("processId", pid);
    task->finalize(payload);
}

void LifecycleManager::CloseAll(LifeCycleTaskPtr task)
{
    closeAllApps();
}

void LifecycleManager::onPrelaunchingDone(const std::string& uid)
{
    LaunchAppItemPtr item = getLaunchingItemByUid(uid);
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS("reason", "null_pointer"),
                  PMLOGKS("where", "on_prelaunching_done"), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 4,
             PMLOGKS("app_id", item->getAppId().c_str()),
             PMLOGKS("uid", uid.c_str()),
             PMLOGKS("status", "prelaunching_done"),
             PMLOGKFV("launching_stage_in_detail", "%d", item->getSubStage()), "");

    if (AppLaunchingStage::PRELAUNCH != item->getStage()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 4,
                  PMLOGKS("app_id", item->getAppId().c_str()),
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS("reason", "not_in_prelaunching_stage"),
                  PMLOGKS("where", "on_prelaunching_done"), "");
        return;
    }

    // just finish launch if error occurs
    if (item->getErrorText().empty() == false) {
        finishLaunching(item);
        return;
    }

    // go next stage
    runWithMemoryChecker(item);
}

void LifecycleManager::onMemoryCheckingStart(const std::string& uid)
{
    LaunchAppItemPtr item = getLaunchingItemByUid(uid);
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS("reason", "null_pointer"),
                  PMLOGKS("where", "on_memory_checking_start"), "");
        return;
    }
    generateLifeCycleEvent(item->getAppId(), uid, LifeEvent::SPLASH);
}

void LifecycleManager::onMemoryCheckingDone(const std::string& uid)
{
    LaunchAppItemPtr item = getLaunchingItemByUid(uid);
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS("reason", "null_pointer"),
                  PMLOGKS("where", "on_memory_checking_done"), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 4,
             PMLOGKS("app_id", item->getAppId().c_str()),
             PMLOGKS("uid", uid.c_str()),
             PMLOGKS("status", "memory_checking_done"),
             PMLOGKFV("launching_stage_in_detail", "%d", item->getSubStage()), "");

    if (AppLaunchingStage::MEMORY_CHECK != item->getStage()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 4,
                  PMLOGKS("app_id", item->getAppId().c_str()),
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS("reason", "not_in_memory_checking_stage"),
                  PMLOGKS("where", "on_memory_checking_done"), "");
        return;
    }

    // just finish launch if error occurs
    if (item->getErrorText().empty() == false) {
        finishLaunching(item);
        return;
    }

    // go next stage
    runWithLauncher(item);
}

void LifecycleManager::onLaunchingDone(const std::string& uid)
{
    LaunchAppItemPtr item = getLaunchingItemByUid(uid);
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS("reason", "null_pointer"),
                  PMLOGKS("where", "on_launching_done"), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", item->getAppId().c_str()),
             PMLOGKS("uid", uid.c_str()),
             PMLOGKS("status", "launching_done"), "");

    finishLaunching(item);
}

void LifecycleManager::runWithPrelauncher(LaunchAppItemPtr item)
{
    LOG_INFO_WITH_CLOCK(MSGID_APPLAUNCH, 4,
                        PMLOGKS("app_id", item->getAppId().c_str()),
                        PMLOGKS("status", "start_prelaunching"),
                        PMLOGKS("PerfType", "AppLaunch"),
                        PMLOGKS("PerfGroup", item->getAppId().c_str()), "");

    item->setStage(AppLaunchingStage::PRELAUNCH);
    m_prelauncher.addItem(item);
}

void LifecycleManager::runWithMemoryChecker(LaunchAppItemPtr item)
{
    LOG_INFO_WITH_CLOCK(MSGID_APPLAUNCH, 4,
                        PMLOGKS("app_id", item->getAppId().c_str()),
                        PMLOGKS("status", "start_memory_checking"),
                        PMLOGKS("PerfType", "AppLaunch"),
                        PMLOGKS("PerfGroup", item->getAppId().c_str()), "");
    item->setStage(AppLaunchingStage::MEMORY_CHECK);
    m_memoryChecker.add_item(item);
    m_memoryChecker.run();
}

void LifecycleManager::runWithLauncher(LaunchAppItemPtr item)
{
    LOG_INFO_WITH_CLOCK(MSGID_APPLAUNCH, 4,
                        PMLOGKS("app_id", item->getAppId().c_str()),
                        PMLOGKS("status", "start_launching"),
                        PMLOGKS("PerfType", "AppLaunch"),
                        PMLOGKS("PerfGroup", item->getAppId().c_str()), "");

    item->setStage(AppLaunchingStage::LAUNCH);
    launchApp(item);
}

void LifecycleManager::runLastappHandler()
{
    if (isFullscreenAppLoading("", "")) {
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 1, PMLOGKS("status", "skip_launching_last_input_app"), "");
        return;
    }

    m_lastappHandler.launch();
}

void LifecycleManager::finishLaunching(LaunchAppItemPtr item)
{
    bool is_last_app_candidate = isLastLaunchingApp(item->getAppId()) && (m_lastLaunchingApps.size() == 1);

    bool redirect_to_lastapplaunch = (is_last_app_candidate) && !item->getErrorText().empty();

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS("app_id", item->getAppId().c_str()),
             PMLOGKS("status", "finish_launching"), "");

    signal_launching_finished(item);
    replyWithResult(item->lsmsg(), item->getPid(), item->getErrorText().empty(), item->getErrorCode(), item->getErrorText());
    removeLastLaunchingApp(item->getAppId());

    std::string app_uid = item->getUid();
    removeItem(app_uid);

    // TODO: decide if this is tv specific or not
    //       make tv handler if it's tv specific
    if (redirect_to_lastapplaunch) {
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 1, PMLOGKS("action", "trigger_launch_lastapp"), "");
        runLastappHandler();
    }
}

void LifecycleManager::onRuntimeStatusChanged(const std::string& app_id, const std::string& uid, const RuntimeStatus& new_status)
{

    if (app_id.empty()) {
        LOG_ERROR(MSGID_LIFE_STATUS_ERR, 2, PMLOGKS("reason", "empty_app_id"), PMLOGKS("where", __FUNCTION__), "");
        return;
    }

    m_lifecycleRouter.setRuntimeStatus(app_id, new_status);
    LifeStatus new_life_status = m_lifecycleRouter.getLifeStatusFromRuntimeStatus(new_status);

    setAppLifeStatus(app_id, uid, new_life_status);
}

void LifecycleManager::setAppLifeStatus(const std::string& appId, const std::string& uid, LifeStatus newStatus)
{
    AppDescPtr appDesc = PackageManager::instance().getAppById(appId);
    LifeStatus currentStatus = AppInfoManager::instance().lifeStatus(appId);

    const LifeCycleRoutePolicy& routePolicy = m_lifecycleRouter.getLifeCycleRoutePolicy(currentStatus, newStatus);
    LifeStatus next_status;
    RouteAction route_action;
    RouteLog routeLog;
    std::tie(next_status, route_action, routeLog) = routePolicy;

    LifeEvent life_event = m_lifecycleRouter.getLifeEventFromLifeStatus(next_status);

    // generate lifecycle event
    generateLifeCycleEvent(appId, uid, life_event);

    if (RouteLog::CHECK == routeLog) {
        LOG_INFO(MSGID_LIFE_STATUS, 3,
                 PMLOGKS("app_id", appId.c_str()),
                 PMLOGKFV("current", "%d", (int)currentStatus),
                 PMLOGKFV("next", "%d", (int)next_status), "just to check it");
    } else if (RouteLog::WARN == routeLog) {
        LOG_WARNING(MSGID_LIFE_STATUS, 3,
                    PMLOGKS("app_id", appId.c_str()),
                    PMLOGKFV("current", "%d", (int)currentStatus),
                    PMLOGKFV("next", "%d", (int)next_status), "handle exception");
    } else if (RouteLog::ERROR == routeLog) {
        LOG_ERROR(MSGID_LIFE_STATUS, 3,
                  PMLOGKS("app_id", appId.c_str()),
                  PMLOGKFV("current", "%d", (int)currentStatus),
                  PMLOGKFV("next", "%d", (int)next_status), "unexpected transition");
    }

    if (RouteAction::IGNORE == route_action)
        return;

    switch (next_status) {
    case LifeStatus::LAUNCHING:
    case LifeStatus::RELAUNCHING:
        if (appDesc)
            addLoadingApp(appId, appDesc->getAppType());
        AppInfoManager::instance().setPreloadMode(appId, false);
        break;
    case LifeStatus::PRELOADING:
        AppInfoManager::instance().setPreloadMode(appId, true);
        break;
    case LifeStatus::STOP:
    case LifeStatus::FOREGROUND:
        AppInfoManager::instance().setPreloadMode(appId, false);
    case LifeStatus::PAUSING:
        removeLoadingApp(appId);
        break;
    default:
        break;
    }

    LOG_INFO(MSGID_LIFE_STATUS, 4,
             PMLOGKS("app_id", appId.c_str()),
             PMLOGKS("uid", uid.c_str()),
             PMLOGKFV("current", "%d", (int)currentStatus),
             PMLOGKFV("new", "%d", (int)next_status), "life_status_changed");

    // set life status
    AppInfoManager::instance().setLifeStatus(appId, next_status);
    signal_app_life_status_changed(appId, next_status);
    replySubscriptionForAppLifeStatus(appId, uid, next_status);
}

void LifecycleManager::stopAllWebappItem()
{
    LOG_INFO(MSGID_APPLAUNCH_ERR, 2,
             PMLOGKS("status", "remove_webapp_in_loading_list"),
             PMLOGKS("reason", "WAM_disconnected"), "");

    std::vector<LoadingAppItem> loading_apps = m_loadingAppList;
    for (const auto& app : loading_apps) {
        if (std::get<1>(app) == AppType::AppType_Web)
            onRuntimeStatusChanged(std::get<0>(app), "", RuntimeStatus::STOP);
    }
}

void LifecycleManager::onRunningAppAdded(const std::string& app_id, const std::string& pid, const std::string& webprocid)
{
    AppInfoManager::instance().addRunningInfo(app_id, pid, webprocid);
    onRunningListChanged(app_id);
}

void LifecycleManager::onRunningAppRemoved(const std::string& app_id)
{

    AppInfoManager::instance().removeRunningInfo(app_id);
    onRunningListChanged(app_id);

    if (isInAutomaticPendingList(app_id))
        handleAutomaticApp(app_id);
}

void LifecycleManager::onRunningListChanged(const std::string& app_id)
{
    pbnjson::JValue running_list = pbnjson::Array();
    AppInfoManager::instance().getRunningList(running_list);
    replySubscriptionForRunning(running_list);

    AppDescPtr app_desc = PackageManager::instance().getAppById(app_id);
    if (app_desc != NULL && AppTypeByDir::AppTypeByDir_Dev == app_desc->getTypeByDir()) {
        running_list = pbnjson::Array();
        AppInfoManager::instance().getRunningList(running_list, true);
        replySubscriptionForRunning(running_list, true);
    }
}

void LifecycleManager::replyWithResult(LSMessage* lsmsg, const std::string& pid, bool result, const int& err_code, const std::string& err_text)
{

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", result);

    if (!result) {
        payload.put("errorCode", err_code);
        payload.put("errorText", err_text);
    }

    if (lsmsg == NULL) {
        LOG_INFO(MSGID_APPLAUNCH, 3,
                 PMLOGKS("reason", "null_lsmessage"),
                 PMLOGKS("where", "reply_with_result_in_app_life_manager"),
                 PMLOGJSON("payload", payload.stringify().c_str()), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS("status", "reply_launch_request"),
             PMLOGJSON("payload", payload.stringify().c_str()), "");

    LSErrorSafe lserror;
    if (!LSMessageRespond(lsmsg, payload.stringify().c_str(), &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "lsrespond"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", "respond_in_app_life_manager"),
                  "err: %s", lserror.message);
    }
}

void LifecycleManager::replySubscriptionForAppLifeStatus(const std::string& app_id, const std::string& uid, const LifeStatus& life_status)
{

    if (app_id.empty()) {
        LOG_WARNING(MSGID_APP_LIFESTATUS_REPLY_ERROR, 1, PMLOGKS("reason", "null app_id"), "");
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    if (payload.isNull()) {
        LOG_WARNING(MSGID_APP_LIFESTATUS_REPLY_ERROR, 1, PMLOGKS("reason", "make pbnjson failed"), "");
        return;
    }

    LaunchAppItemPtr launch_item = uid.empty() ? NULL : getLaunchingItemByUid(uid);
    std::string str_status = AppInfoManager::instance().toString(life_status);
    bool preloaded = AppInfoManager::instance().preloadModeOn(app_id);

    payload.put("status", str_status);    // change enum to string
    payload.put("appId", app_id);

    std::string pid = AppInfoManager::instance().getPid(app_id);
    if (!pid.empty())
        payload.put("processId", pid);

    AppDescPtr app_desc = PackageManager::instance().getAppById(app_id);

    if (app_desc != NULL)
        payload.put("type", AppDescription::toString(app_desc->getAppType()));

    if (LifeStatus::LAUNCHING == life_status || LifeStatus::RELAUNCHING == life_status) {
        if (launch_item) {
            payload.put("reason", launch_item->getReason());
        }
    } else if (LifeStatus::FOREGROUND == life_status) {
        pbnjson::JValue fg_info = pbnjson::JValue();
        AppInfoManager::instance().getJsonForegroundInfoById(app_id, fg_info);
        if (!fg_info.isNull() && fg_info.isObject()) {
            for (auto it : fg_info.children()) {
                const std::string key = it.first.asString();
                if ("windowType" == key || "windowGroup" == key || "windowGroupOwner" == key || "windowGroupOwnerId" == key) {
                    payload.put(key, fg_info[key]);
                }
            }
        }
    } else if (LifeStatus::BACKGROUND == life_status) {
        if (preloaded)
            payload.put("backgroundStatus", "preload");
        else
            payload.put("backgroundStatus", "normal");
    } else if (LifeStatus::STOP == life_status || LifeStatus::CLOSING == life_status) {
        payload.put("reason", (m_closeReasonInfo.count(app_id) == 0) ? "undefined" : m_closeReasonInfo[app_id]);

        if (LifeStatus::STOP == life_status)
            m_closeReasonInfo.erase(app_id);
    }

    LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2,
             PMLOGKS("skey", "getAppLifeStatus"),
             PMLOGJSON("payload", payload.stringify().c_str()), "");

    LSErrorSafe lserror;
    if (!LSSubscriptionReply(AppMgrService::instance().serviceHandle(),
                             "getAppLifeStatus",
                             payload.stringify().c_str(), &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "subscriptionreply"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", "reply_get_app_life_status"), "err: %s", lserror.message);
        return;
    }
}

void LifecycleManager::generateLifeCycleEvent(const std::string& app_id, const std::string& uid, LifeEvent event)
{

    AppDescPtr app_desc = PackageManager::instance().getAppById(app_id);
    LaunchAppItemPtr launch_item = uid.empty() ? NULL : getLaunchingItemByUid(uid);
    LifeStatus current_status = AppInfoManager::instance().lifeStatus(app_id);
    bool preloaded = AppInfoManager::instance().preloadModeOn(app_id);

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("appId", app_id);

    if (LifeEvent::SPLASH == event) {
        // generate splash event only for fresh launch case
        if (launch_item && !launch_item->isShowSplash() && !launch_item->isShowSpinner())
            return;
        if (LifeStatus::BACKGROUND == current_status && preloaded == false)
            return;
        if (LifeStatus::STOP != current_status && LifeStatus::PRELOADING != current_status && LifeStatus::BACKGROUND != current_status)
            return;

        payload.put("event", "splash");
        payload.put("title", (app_desc ? app_desc->getTitle() : ""));
        payload.put("showSplash", (launch_item && launch_item->isShowSplash()));
        payload.put("showSpinner", (launch_item && launch_item->isShowSpinner()));

        if (launch_item && launch_item->isShowSplash())
            payload.put("splashBackground", (app_desc ? app_desc->getSplashBackground() : ""));
    } else if (LifeEvent::PRELOAD == event) {
        payload.put("event", "preload");
        if (launch_item)
            payload.put("preload", launch_item->getPreload());
    } else if (LifeEvent::LAUNCH == event) {
        payload.put("event", "launch");
        payload.put("reason", launch_item->getReason());
    } else if (LifeEvent::FOREGROUND == event) {
        payload.put("event", "foreground");

        pbnjson::JValue fg_info = pbnjson::JValue();
        AppInfoManager::instance().getJsonForegroundInfoById(app_id, fg_info);
        if (!fg_info.isNull() && fg_info.isObject()) {
            for (auto it : fg_info.children()) {
                const std::string key = it.first.asString();
                if ("windowType" == key ||
                    "windowGroup" == key ||
                    "windowGroupOwner" == key ||
                    "windowGroupOwnerId" == key) {
                    payload.put(key, fg_info[key]);
                }
            }
        }
    } else if (LifeEvent::BACKGROUND == event) {
        payload.put("event", "background");
        if (preloaded)
            payload.put("status", "preload");
        else
            payload.put("status", "normal");
    } else if (LifeEvent::PAUSE == event) {
        payload.put("event", "pause");
    } else if (LifeEvent::CLOSE == event) {
        payload.put("event", "close");
        payload.put("reason", (m_closeReasonInfo.count(app_id) == 0) ? "undefined" : m_closeReasonInfo[app_id]);
    } else if (LifeEvent::STOP == event) {
        payload.put("event", "stop");
        payload.put("reason", (m_closeReasonInfo.count(app_id) == 0) ? "undefined" : m_closeReasonInfo[app_id]);
    } else {
        return;
    }

    signal_lifecycle_event(payload);
}

void LifecycleManager::replySubscriptionForRunning(const pbnjson::JValue& running_list, bool devmode)
{
    pbnjson::JValue payload = pbnjson::Object();
    std::string subscription_key = devmode ? "dev_running" : "running";

    payload.put("running", running_list);
    payload.put("returnValue", true);

    LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2,
             PMLOGKS("skey", subscription_key.c_str()),
             PMLOGJSON("payload", payload.stringify().c_str()), "");

    LSErrorSafe lserror;
    if (!LSSubscriptionReply(AppMgrService::instance().serviceHandle(),
                             subscription_key.c_str(),
                             payload.stringify().c_str(), &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "subscriptionreply"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", "reply_running_in_app_life_manager"), "err: %s", lserror.message);
        return;
    }
}

void LifecycleManager::handleBridgedLaunchRequest(const pbnjson::JValue& params)
{
    std::string item_uid;
    if (!params.hasKey(SYS_LAUNCHING_UID) || params[SYS_LAUNCHING_UID].asString(item_uid) != CONV_OK) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "uid_not_found"),
                  PMLOGKS("where", "handle_bridged_launch"), "");
        return;
    }

    LaunchAppItemPtr launching_item = getLaunchingItemByUid(item_uid);
    if (launching_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("uid", item_uid.c_str()),
                  PMLOGKS("reason", "launching_item_not_found"),
                  PMLOGKS("where", "handle_bridged_launch"), "");
        return;
    }
    m_prelauncher.inputBridgedReturn(launching_item, params);
}

void LifecycleManager::registerApp(const std::string& appId, LSMessage* lsmsg, std::string& errText)
{
    AppDescPtr appDesc = PackageManager::instance().getAppById(appId);
    if (!appDesc) {
        errText = "not existing app";
        return;
    }

    if (appDesc->getNativeInterfaceVersion() != 2) {
        errText = "trying to register via unmatched method with nativeLifeCycleInterfaceVersion";
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", appId.c_str()),
                  PMLOGKS("reason", "wrong_version_interface_on_register"),
                  PMLOGKS("method", "registerNativeApp"),
                  "nativeLifeCycleInterfaceVersion: %d", appDesc->getNativeInterfaceVersion());
        return;
    }

    RuntimeStatus runtimeStatus = AppInfoManager::instance().runtimeStatus(appId);

    if (RuntimeStatus::RUNNING != runtimeStatus && RuntimeStatus::REGISTERED != runtimeStatus) {
        errText = "invalid status";
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", appId.c_str()),
                  PMLOGKS("reason", "invalid_life_status_to_connect_nativeapp"),
                  PMLOGKFV("runtime_status", "%d", (int) runtimeStatus), "");
        return;
    }

    NativeAppLifeHandler::getInstance().registerApp(appId, lsmsg, errText);
}

void LifecycleManager::connectNativeApp(const std::string& app_id, LSMessage* lsmsg, std::string& err_text)
{

    AppDescPtr app_desc = PackageManager::instance().getAppById(app_id);
    if (!app_desc) {
        err_text = "not existing app";
        return;
    }

    if (app_desc->getNativeInterfaceVersion() != 1) {
        err_text = "trying to register via unmatched method with nativeLifeCycleInterfaceVersion";
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("reason", "wrong_version_interface_on_register"),
                  PMLOGKS("method", "registerNativeApp"),
                  "nativeLifeCycleInterfaceVersion: %d", app_desc->getNativeInterfaceVersion());
        return;
    }

    RuntimeStatus runtime_status = AppInfoManager::instance().runtimeStatus(app_id);

    if (RuntimeStatus::RUNNING != runtime_status && RuntimeStatus::REGISTERED != runtime_status) {
        err_text = "invalid_status";
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("reason", "invalid_life_status_to_connect_nativeapp"),
                  PMLOGKFV("runtime_status", "%d", (int) runtime_status), "");
        return;
    }

    NativeAppLifeHandler::getInstance().registerApp(app_id, lsmsg, err_text);
}

void LifecycleManager::triggerToLaunchLastApp()
{

    std::string fg_app_id = AppInfoManager::instance().getCurrentForegroundAppId();
    if (!fg_app_id.empty() && AppInfoManager::instance().isRunning(fg_app_id))
        return;

    LOG_INFO(MSGID_LAUNCH_LASTAPP, 1, PMLOGKS("status", "trigger_to_launch_last_app"), "");

    runLastappHandler();
}

void LifecycleManager::closeByAppId(const std::string& app_id, const std::string& caller_id, const std::string& reason, std::string& err_text, bool preload_only, bool clear_all_items)
{

    if (preload_only && !hasOnlyPreloadedItems(app_id)) {
        err_text = "app is being launched by user";
        return;
    }

    // keepAlive app policy
    // swith close request to pause request
    // except for below cases
    if (SettingsImpl::instance().isKeepAliveApp(app_id) &&
        caller_id != "com.webos.memorymanager" &&
        caller_id != "com.webos.appInstallService" &&
        (caller_id != "com.webos.surfacemanager.windowext" || reason != "recent") &&
        caller_id != SAM_INTERNAL_ID) {
        pauseApp(app_id, pbnjson::Object(), err_text);
        return;
    }

    closeApp(app_id, caller_id, reason, err_text, clear_all_items);
}

void LifecycleManager::closeByPid(const std::string& pid, const std::string& caller_id, std::string& err_text)
{
    const std::string& app_id = AppInfoManager::instance().getAppIdByPid(pid);
    if (app_id.empty()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS("pid", pid.c_str()),
                  PMLOGKS("mode", "by_pid"), "no appid matched by pid");
        err_text = "no app matched by pid";
        return;
    }

    closeApp(app_id, caller_id, "", err_text);
}

void LifecycleManager::closeAllLoadingApps()
{
    resetLastAppCandidates();

    m_prelauncher.cancelAll();
    m_memoryChecker.cancel_all();

    std::vector<std::string> automatic_pending_list;
    getAutomaticPendingAppIds(automatic_pending_list);

    for (auto& app_id : automatic_pending_list)
        handleAutomaticApp(app_id, false);

    std::vector<LoadingAppItem> loading_apps = m_loadingAppList;
    for (const auto& app : loading_apps) {
        std::string err_text = "";
        closeByAppId(std::get<0>(app), SAM_INTERNAL_ID, "", err_text, false, true);

        if (err_text.empty() == false) {
            LOG_WARNING(MSGID_APPCLOSE, 2,
                        PMLOGKS("app_id", (std::get<0>(app)).c_str()),
                        PMLOGKS("mode", "close_loading_app"), "err: %s", err_text.c_str());
        } else {
            LOG_INFO(MSGID_APPCLOSE, 2,
                     PMLOGKS("app_id", (std::get<0>(app)).c_str()),
                     PMLOGKS("mode", "close_loading_app"), "ok");
        }
    }
}

void LifecycleManager::closeAllApps(bool clear_all_items)
{
    LOG_INFO(MSGID_APPCLOSE, 2, PMLOGKS("action", "close_all_apps"), PMLOGKS("status", "start"), "");
    std::vector<std::string> running_apps;
    AppInfoManager::instance().getRunningAppIds(running_apps);
    closeApps(running_apps, clear_all_items);
    resetLastAppCandidates();
}

void LifecycleManager::closeApps(const std::vector<std::string>& app_ids, bool clear_all_items)
{
    if (app_ids.size() < 1) {
        LOG_INFO(MSGID_APPCLOSE, 2,
                 PMLOGKS("action", "close_all_apps"),
                 PMLOGKS("status", "no_apps_to_close"), "");
        return;
    }

    std::string fullscreen_app_id;
    bool performed_closing_apps = false;
    for (auto& app_id : app_ids) {
        // kill app on fullscreen later
        if (AppInfoManager::instance().isAppOnFullscreen(app_id)) {
            fullscreen_app_id = app_id;
            continue;
        }

        std::string err_text = "";
        closeByAppId(app_id, SAM_INTERNAL_ID, "", err_text, false, clear_all_items);
        performed_closing_apps = true;
    }

    // now kill app on fullscreen
    if (!fullscreen_app_id.empty()) {
        // we don't guarantee all apps running in background are closed before closing app running on foreground
        // we just raise possibility by adding sleep time
        if (performed_closing_apps)
            usleep(SLEEP_TIME_TO_CLOSE_FULLSCREEN_APP);
        std::string err_text = "";
        closeByAppId(fullscreen_app_id, SAM_INTERNAL_ID, "", err_text, false, clear_all_items);
    }
}

void LifecycleManager::launchApp(LaunchAppItemPtr item)
{
    IAppLifeHandler* life_handler = getLifeHandlerForApp(item->getAppId());
    if (nullptr == life_handler) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", item->getAppId().c_str()),
                  PMLOGKS("reason", "null_description"),
                  PMLOGKS("where", "launch_app_in_app_life_manager"), "");
        return;
    }

    life_handler->launch(item);
}

void LifecycleManager::closeApp(const std::string& appId, const std::string& callerId, const std::string& reason, std::string& errText, bool clearAllItems)
{
    if (clearAllItems) {
        clearLaunchingAndLoadingItemsByAppId(appId);
    }

    IAppLifeHandler* handler = getLifeHandlerForApp(appId);
    if (nullptr == handler) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", appId.c_str()), "no valid life handler");
        errText = "no valid life handler";
        return;
    }

    std::string closeReason = SettingsImpl::instance().getCloseReason(callerId, reason);

    if (AppInfoManager::instance().isRunning(appId) && m_closeReasonInfo.count(appId) == 0) {
        m_closeReasonInfo[appId] = closeReason;
    }

    const std::string& pid = AppInfoManager::instance().getPid(appId);
    CloseAppItemPtr item = std::make_shared<CloseAppItem>(appId, pid, callerId, closeReason);
    if (nullptr == item) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", appId.c_str()), "memory alloc fail");
        errText = "memory alloc fail";
        return;
    }

    LOG_INFO(MSGID_APPCLOSE, 2, PMLOGKS("app_id", appId.c_str()), PMLOGKS("pid", pid.c_str()), "");

    handler->close(item, errText);
}

void LifecycleManager::pauseApp(const std::string& app_id, const pbnjson::JValue& params, std::string& err_text, bool report_event)
{

    if (!AppInfoManager::instance().isRunning(app_id)) {
        err_text = "app is not running";
        return;
    }

    IAppLifeHandler* life_handler = getLifeHandlerForApp(app_id);
    if (nullptr == life_handler) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", app_id.c_str()), "no valid life handler");
        err_text = "no valid life handler";
        return;
    }

    life_handler->pause(app_id, params, err_text, report_event);
}

bool LifecycleManager::isFullscreenWindowType(const pbnjson::JValue& foreground_info)
{
    bool window_group = foreground_info["windowGroup"].asBool();
    bool window_group_owner = (window_group == false ? true : foreground_info["windowGroupOwner"].asBool());
    std::string window_type = foreground_info["windowType"].asString();

    for (auto& win_type : m_fullscreenWindowTypes) {
        if (win_type == window_type && window_group_owner) {
            return true;
        }
    }
    return false;
}

void LifecycleManager::onForegroundInfoChanged(const pbnjson::JValue& jmsg)
{
    if (jmsg.isNull() ||
        !jmsg["returnValue"].asBool() ||
        !jmsg.hasKey("foregroundAppInfo") ||
        !jmsg["foregroundAppInfo"].isArray()) {
        LOG_ERROR(MSGID_GET_FOREGROUND_APP_ERR, 1,
                  PMLOGJSON("payload", jmsg.duplicate().stringify().c_str()), "invalid message from LSM");
        return;
    }

    pbnjson::JValue foregroundAppInfo = jmsg["foregroundAppInfo"];

    LOG_INFO(MSGID_GET_FOREGROUND_APPINFO, 1, PMLOGJSON("ForegroundApps", foregroundAppInfo.stringify().c_str()), "");

    std::string prev_foreground_app_id = AppInfoManager::instance().getCurrentForegroundAppId();
    std::vector<std::string> prev_foreground_apps = AppInfoManager::instance().getForegroundApps();
    std::vector<std::string> newForegroundApps;
    pbnjson::JValue prev_foreground_info = AppInfoManager::instance().getJsonForegroundInfo();
    std::string new_foreground_app_id = "";
    bool found_fullscreen_window = false;

    pbnjson::JValue newForegroundInfo = pbnjson::Array();
    for (int i = 0; i < foregroundAppInfo.arraySize(); ++i) {
        std::string appId;
        if (!JValueUtil::getValue(foregroundAppInfo[i], "appId", appId) || appId.empty()) {
            continue;
        }

        LifeStatus currentStatus = AppInfoManager::instance().lifeStatus(appId);
        if (LifeStatus::STOP == currentStatus) {
            LOG_INFO(MSGID_FOREGROUND_INFO, 1, PMLOGKS("filter_out", appId.c_str()), "remove not running app");
            continue;
        }

        LOG_INFO_WITH_CLOCK(MSGID_GET_FOREGROUND_APPINFO, 2,
                            PMLOGKS("PerfType", "AppLaunch"),
                            PMLOGKS("PerfGroup", appId.c_str()), "");

        newForegroundInfo.append(foregroundAppInfo[i].duplicate());
        newForegroundApps.push_back(appId);

        if (isFullscreenWindowType(foregroundAppInfo[i])) {
            found_fullscreen_window = true;
            new_foreground_app_id = appId;
        }
    }

    LOG_INFO(MSGID_GET_FOREGROUND_APPINFO, 1,
             PMLOGJSON("FilteredForegroundApps", newForegroundInfo.duplicate().stringify().c_str()), "");

    // update foreground info into app info manager
    if (found_fullscreen_window) {
        resetLastAppCandidates();
    }
    AppInfoManager::instance().setCurrentForegroundAppId(new_foreground_app_id);
    AppInfoManager::instance().setForegroundApps(newForegroundApps);
    AppInfoManager::instance().setForegroundInfo(newForegroundInfo);

    LOG_INFO(MSGID_FOREGROUND_INFO, 2,
             PMLOGKS("current_foreground_app", new_foreground_app_id.c_str()),
             PMLOGKS("prev_foreground_app", prev_foreground_app_id.c_str()), "");

    // set background
    for (auto& prev_app_id : prev_foreground_apps) {
        bool found = false;
        for (auto& current_app_id : newForegroundApps) {
            if ((prev_app_id) == (current_app_id)) {
                found = true;
                break;
            }
        }

        if (found == false) {
            switch (AppInfoManager::instance().lifeStatus(prev_app_id)) {
            case LifeStatus::FOREGROUND:
            case LifeStatus::PAUSING:
                setAppLifeStatus(prev_app_id, "", LifeStatus::BACKGROUND);
                break;
            default:
                break;
            }
        }
    }

    // set foreground
    for (auto& current_app_id : newForegroundApps) {
        setAppLifeStatus(current_app_id, "", LifeStatus::FOREGROUND);

        if (!AppInfoManager::instance().isRunning(current_app_id)) {
            LOG_INFO(MSGID_FOREGROUND_INFO, 1, PMLOGKS("app_id", current_app_id.c_str()), "no running info, but received foreground info");
        }
    }

    // this is TV specific scenario related to TV POWER (instant booting)
    // improve this tv dependent structure later
    if (jmsg.hasKey("reason") && jmsg["reason"].asString() == "forceMinimize") {
        LOG_INFO(MSGID_FOREGROUND_INFO, 1, PMLOGKS("reason", "forceMinimize"), "no trigger last input handler");
        resetLastAppCandidates();
    }
    // run last input handler
    else if (found_fullscreen_window == false) {
        runLastappHandler();
    }

    // signal foreground app changed
    if (prev_foreground_app_id != new_foreground_app_id) {
        signal_foreground_app_changed(new_foreground_app_id);
    }

    // reply subscription foreground with extraInfo
    if (prev_foreground_info != newForegroundInfo) {
        signal_foreground_extra_info_changed(newForegroundInfo);
    }
}

bool LifecycleManager::isFullscreenAppLoading(const std::string& new_launching_app_id, const std::string& new_launching_app_uid)
{
    bool result = false;

    for (auto& launching_item : m_appLaunchingItemList) {
        if (new_launching_app_id == launching_item->getAppId() && launching_item->getUid() == new_launching_app_uid)
            continue;

        AppDescPtr app_desc = PackageManager::instance().getAppById(launching_item->getAppId());
        if (app_desc == NULL) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", launching_item->getAppId().c_str()),
                     PMLOGKS("status", "not_candidate_checking_launching"), "null description");
            continue;
        }

        if (("com.webos.app.container" == launching_item->getAppId()) ||
            ("com.webos.app.inputcommon" == launching_item->getAppId()))
            continue;

        if (app_desc->isChildWindow()) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", launching_item->getAppId().c_str()),
                     PMLOGKS("status", "not_candidate_checking_launching"), "child window type");
            continue;
        }
        if (!(launching_item->getPreload().empty())) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", launching_item->getAppId().c_str()),
                     PMLOGKS("status", "not_candidate_checking_launching"), "preloading app");
            continue;
        }

        if (app_desc->getDefaultWindowType() == "card" || app_desc->getDefaultWindowType() == "minimal") {
            if (isLaunchingItemExpired(launching_item)) {
                LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
                         PMLOGKS("app_id", launching_item->getAppId().c_str()),
                         PMLOGKS("status", "not_candidate_checking_launching"),
                         PMLOGKFV("launching_stage_in_detail", "%d", launching_item->getSubStage()),
                         "fullscreen app, but expired");
                continue;
            }

            LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
                     PMLOGKS("status", "autolaunch_condition_check"),
                     PMLOGKS("launching_app_id", launching_item->getAppId().c_str()),
                     PMLOGKFV("launching_stage_in_detail", "%d", launching_item->getSubStage()),
                     "fullscreen app is already launching");

            addLastLaunchingApp(launching_item->getAppId());
            result = true;
        } else {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                    PMLOGKS("app_id", launching_item->getAppId().c_str()),
                    PMLOGKS("status", "not_candidate_checking_launching"),
                    "not fullscreen type: %s", app_desc->getDefaultWindowType().c_str());
        }
    }

    std::vector<std::string> app_ids;
    getLoadingAppIds(app_ids);

    for (auto& appId : app_ids) {
        AppDescPtr app_desc = PackageManager::instance().getAppById(appId);
        if (app_desc == NULL) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", appId.c_str()),
                     PMLOGKS("status", "not_candidate_checking_loading"), "null description");
            continue;
        }
        if (app_desc->isChildWindow()) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", appId.c_str()),
                     PMLOGKS("status", "not_candidate_checking_loading"), "child window type");
            continue;
        }
        if (AppInfoManager::instance().preloadModeOn(appId)) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", appId.c_str()),
                     PMLOGKS("status", "not_candidate_checking_loading"), "preloading app");
            continue;
        }

        if (app_desc->getDefaultWindowType() == "card" || app_desc->getDefaultWindowType() == "minimal") {
            if (isLoadingAppExpired(appId)) {
                LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                         PMLOGKS("app_id", appId.c_str()),
                         PMLOGKS("status", "not_candidate_checking_loading"), "fullscreen app, but expired");
                continue;
            }

            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("status", "autolaunch_condition_check"),
                     PMLOGKS("loading_app_id", appId.c_str()), "fullscreen app is already loading");

            setLastLoadingApp(appId);
            result = true;
            break;
        } else {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", appId.c_str()),
                     PMLOGKS("status", "not_candidate_checking_loading"), "not fullscreen type: %s", app_desc->getDefaultWindowType().c_str());
        }
    }

    return result;
}

void LifecycleManager::clearLaunchingAndLoadingItemsByAppId(const std::string& app_id)
{
    bool found = false;

    while (true) {
        auto it = std::find_if(m_appLaunchingItemList.begin(), m_appLaunchingItemList.end(), [&app_id](LaunchAppItemPtr item) {return (item->getAppId() == app_id);});

        if (it == m_appLaunchingItemList.end())
            break;
        std::string err_text = "stopped launching";
        (*it)->setErrCodeText(APP_LAUNCH_ERR_GENERAL, err_text);
        finishLaunching(*it);
        found = true;
    }

    for (auto& loading_app : m_loadingAppList) {
        if (std::get<0>(loading_app) == app_id) {
            found = true;
            break;
        }
    }

    if (found) {
        setAppLifeStatus(app_id, "", LifeStatus::STOP);
    }
}

void LifecycleManager::handleAutomaticApp(const std::string& app_id, bool continue_to_launch)
{
    LaunchAppItemPtr item = getLaunchingItemByAppId(app_id);
    if (item == NULL) {
        LOG_ERROR(MSGID_LAUNCH_LASTAPP_ERR, 3,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("reason", "launching_item_not_found"),
                  PMLOGKS("where", "handle_automatic_app"), "");
        removeItemFromAutomaticPendingList(app_id);

        return;
    }

    removeItemFromAutomaticPendingList(app_id);

    if (continue_to_launch) {
        runWithPrelauncher(item);
    } else {
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("status", "cancel_to_launch_last_app"),
                 PMLOGKS("where", "handle_automatic_app"), "");
        finishLaunching(item);
        return;
    }
}

void LifecycleManager::getLaunchingAppIds(std::vector<std::string>& app_ids)
{
    for (auto& launching_item : m_appLaunchingItemList)
        app_ids.push_back(launching_item->getAppId());
}

LaunchAppItemPtr LifecycleManager::getLaunchingItemByUid(const std::string& uid)
{
    for (auto& launching_item : m_appLaunchingItemList) {
        if (launching_item->getUid() == uid)
            return launching_item;
    }
    return NULL;
}

LaunchAppItemPtr LifecycleManager::getLaunchingItemByAppId(const std::string& app_id)
{
    for (auto& launching_item : m_appLaunchingItemList) {
        if (launching_item->getAppId() == app_id)
            return launching_item;
    }
    return NULL;
}

void LifecycleManager::removeItem(const std::string& uid)
{
    auto it = std::find_if(m_appLaunchingItemList.begin(), m_appLaunchingItemList.end(),
                          [&uid](LaunchAppItemPtr item) {return (item->getUid() == uid);});
    if (it == m_appLaunchingItemList.end())
        return;
    m_appLaunchingItemList.erase(it);
}

void LifecycleManager::addItemIntoAutomaticPendingList(LaunchAppItemPtr item)
{
    m_automaticPendingList.push_back(item);
    LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
             PMLOGKS("app_id", item->getAppId().c_str()),
             PMLOGKS("mode", "pending_automatic_app"),
             PMLOGKS("status", "added_into_list"), "");
}

void LifecycleManager::removeItemFromAutomaticPendingList(const std::string& app_id)
{
    auto it = std::find_if(m_automaticPendingList.begin(), m_automaticPendingList.end(),
                          [&app_id](LaunchAppItemPtr item) {return (item->getAppId() == app_id);});
    if (it != m_automaticPendingList.end()) {
        m_automaticPendingList.erase(it);
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("mode", "pending_automatic_app"),
                 PMLOGKS("status", "removed_from_list"), "");
    }
}

bool LifecycleManager::isInAutomaticPendingList(const std::string& app_id)
{
    auto it = std::find_if(m_automaticPendingList.begin(), m_automaticPendingList.end(),
                          [&app_id](LaunchAppItemPtr item) {return (item->getAppId() == app_id);});

    if (it != m_automaticPendingList.end())
        return true;
    else
        return false;
}

void LifecycleManager::getAutomaticPendingAppIds(std::vector<std::string>& app_ids)
{
    for (auto& launching_item : m_automaticPendingList)
        app_ids.push_back(launching_item->getAppId());
}

void LifecycleManager::getLoadingAppIds(std::vector<std::string>& app_ids)
{
    for (auto& app : m_loadingAppList)
        app_ids.push_back(std::get<0>(app));
}

void LifecycleManager::addLoadingApp(const std::string& app_id, const AppType& type)
{
    for (auto& loading_app : m_loadingAppList)
        if (std::get<0>(loading_app) == app_id)
            return;
    // exception
    if ("com.webos.app.container" == app_id || "com.webos.app.inputcommon" == app_id) {
        LOG_INFO(MSGID_LOADING_LIST, 1, PMLOGKS("app_id", app_id.c_str()), "apply exception (skip)");
        return;
    }

    LoadingAppItem new_loading_app = std::make_tuple(app_id, type, static_cast<double>(getCurrentTime()));
    m_loadingAppList.push_back(new_loading_app);

    LOG_INFO(MSGID_LOADING_LIST, 1, PMLOGKS("app_id", app_id.c_str()), "added");

    if (isLastLaunchingApp(app_id))
        setLastLoadingApp(app_id);
}

void LifecycleManager::removeLoadingApp(const std::string& app_id)
{
    auto it = std::find_if(m_loadingAppList.begin(), m_loadingAppList.end(), [&app_id](const LoadingAppItem& loading_app) {return (std::get<0>(loading_app) == app_id);});
    if (it == m_loadingAppList.end())
        return;
    m_loadingAppList.erase(it);
    LOG_INFO(MSGID_LOADING_LIST, 1, PMLOGKS("app_id", app_id.c_str()), "removed");

    if (m_lastLoadingAppTimerSet.second == app_id)
        removeTimerForLastLoadingApp(true);
}

void LifecycleManager::setLastLoadingApp(const std::string& app_id)
{
    auto it = std::find_if(m_loadingAppList.begin(), m_loadingAppList.end(),
                          [&app_id](const LoadingAppItem& loading_app) {return (std::get<0>(loading_app) == app_id);});
    if (it == m_loadingAppList.end())
        return;

    addTimerForLastLoadingApp(app_id);
    removeLastLaunchingApp(app_id);
}

void LifecycleManager::addTimerForLastLoadingApp(const std::string& app_id)
{
    if ((m_lastLoadingAppTimerSet.first != 0) && (m_lastLoadingAppTimerSet.second == app_id))
        return;

    auto it = std::find_if(m_loadingAppList.begin(), m_loadingAppList.end(),
                          [&app_id](const LoadingAppItem& loading_app) {return (std::get<0>(loading_app) == app_id);});
    if (it == m_loadingAppList.end())
        return;

    AppDescPtr app_desc = PackageManager::instance().getAppById(app_id);
    if (app_desc == NULL) {
        LOG_ERROR(MSGID_LAUNCH_LASTAPP_ERR, 3,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("status", "cannot_add_timer_for_last_loading_app"),
                  PMLOGKS("reason", "null description"), "");
        return;
    }

    removeTimerForLastLoadingApp(false);

    guint timer = g_timeout_add(SettingsImpl::instance().getLastLoadingAppTimeout(), runLastLoadingAppTimeoutHandler, NULL);
    m_lastLoadingAppTimerSet = std::make_pair(timer, app_id);
}

void LifecycleManager::removeTimerForLastLoadingApp(bool trigger)
{
    if (m_lastLoadingAppTimerSet.first == 0)
        return;

    g_source_remove(m_lastLoadingAppTimerSet.first);
    m_lastLoadingAppTimerSet = std::make_pair(0, "");

    if (trigger)
        triggerToLaunchLastApp();
}

gboolean LifecycleManager::runLastLoadingAppTimeoutHandler(gpointer user_data)
{
    LifecycleManager::instance().removeTimerForLastLoadingApp(true);
    return FALSE;
}

void LifecycleManager::addLastLaunchingApp(const std::string& app_id)
{
    auto it = std::find(m_lastLaunchingApps.begin(), m_lastLaunchingApps.end(), app_id);
    if (it != m_lastLaunchingApps.end())
        return;

    m_lastLaunchingApps.push_back(app_id);
}

void LifecycleManager::removeLastLaunchingApp(const std::string& app_id)
{
    auto it = std::find(m_lastLaunchingApps.begin(), m_lastLaunchingApps.end(), app_id);
    if (it == m_lastLaunchingApps.end())
        return;

    m_lastLaunchingApps.erase(it);
}

bool LifecycleManager::isLastLaunchingApp(const std::string& app_id)
{
    auto it = std::find(m_lastLaunchingApps.begin(), m_lastLaunchingApps.end(), app_id);
    return {(it != m_lastLaunchingApps.end()) ? true : false};
}

void LifecycleManager::resetLastAppCandidates()
{
    m_lastLaunchingApps.clear();
    removeTimerForLastLoadingApp(false);
}

bool LifecycleManager::hasOnlyPreloadedItems(const std::string& app_id)
{
    for (auto& launching_item : m_appLaunchingItemList) {
        if (launching_item->getAppId() == app_id && launching_item->getPreload().empty())
            return false;
    }

    for (auto& loading_item : m_loadingAppList) {
        if (std::get<0>(loading_item) == app_id)
            return false;
    }

    if (AppInfoManager::instance().isRunning(app_id) && !AppInfoManager::instance().preloadModeOn(app_id)) {
        return false;
    }

    return true;
}

bool LifecycleManager::isLaunchingItemExpired(LaunchAppItemPtr item)
{
    double current_time = getCurrentTime();
    double elapsed_time = current_time - item->launchStartTime();

    if (elapsed_time > SettingsImpl::instance().getLaunchExpiredTimeout())
        return true;

    return false;
}

bool LifecycleManager::isLoadingAppExpired(const std::string& app_id)
{
    double loading_start_time = 0;
    for (auto& loading_app : m_loadingAppList) {
        if (std::get<0>(loading_app) == app_id) {
            loading_start_time = std::get<2>(loading_app);
            break;
        }
    }

    if (loading_start_time == 0) {
        LOG_WARNING(MSGID_LAUNCH_LASTAPP, 2,
                    PMLOGKS("status", "invalid_loading_start_time"),
                    PMLOGKS("app_id", app_id.c_str()), "");
        return true;
    }

    double current_time = getCurrentTime();
    double elapsed_time = current_time - loading_start_time;

    if (elapsed_time > SettingsImpl::instance().getLoadingExpiredTimeout())
        return true;

    return false;
}

IAppLifeHandler* LifecycleManager::getLifeHandlerForApp(const std::string& app_id)
{
    AppDescPtr app_desc = PackageManager::instance().getAppById(app_id);
    if (app_desc == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("reason", "null_description"),
                  PMLOGKS("where", __FUNCTION__), "");
        return nullptr;
    }

    switch (app_desc->getHandlerType()) {
    case LifeHandlerType::LifeHandlerType_Web:
        return &m_webLifecycleHandler;

    case LifeHandlerType::LifeHandlerType_Qml:
        return &m_qmlLifecycleHandler;

    case LifeHandlerType::LifeHandlerType_Native:
        return &NativeAppLifeHandler::getInstance();

    default:
        break;
    }

    return nullptr;
}
