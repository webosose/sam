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

#include "core/lifecycle/app_life_manager.h"

#include <boost/signals2.hpp>
#include <unistd.h>

#include "core/base/jutil.h"
#include "core/base/logging.h"
#include "core/base/lsutils.h"
#include "core/base/utils.h"
#include "core/bus/appmgr_service.h"
#include "core/bus/lunaservice_api.h"
#include "core/module/subscriber_of_lsm.h"
#include "core/package/application_manager.h"
#include "core/setting/settings.h"

#define SAM_INTERNAL_ID  "com.webos.applicationManager"
#define SLEEP_TIME_TO_CLOSE_FULLSCREEN_APP 500000

AppLifeManager::AppLifeManager()
    : m_launchItemFactory(NULL),
      m_prelauncher(NULL),
      m_memoryChecker(NULL),
      m_lastappHandler(NULL)
{
}

AppLifeManager::~AppLifeManager()
{
}

void AppLifeManager::init()
{
    m_lifecycleRouter.init();
    AppInfoManager::instance().init();

    // receive signal on service disconnected
    m_webLifecycleHandler.signal_service_disconnected.connect(boost::bind(&AppLifeManager::stopAllWebappItem, this));

    // receive signal on life status change
    m_nativeLifecycleHandler.signal_app_life_status_changed.connect(boost::bind(&AppLifeManager::onRuntimeStatusChanged, this, _1, _2, _3));
    m_webLifecycleHandler.signal_app_life_status_changed.connect(boost::bind(&AppLifeManager::onRuntimeStatusChanged, this, _1, _2, _3));
    m_qmlLifecycleHandler.signal_app_life_status_changed.connect(boost::bind(&AppLifeManager::onRuntimeStatusChanged, this, _1, _2, _3));

    // receive signal on running list change
    m_nativeLifecycleHandler.signal_running_app_added.connect(boost::bind(&AppLifeManager::onRunningAppAdded, this, _1, _2, _3));
    m_webLifecycleHandler.signal_running_app_added.connect(boost::bind(&AppLifeManager::onRunningAppAdded, this, _1, _2, _3));
    m_qmlLifecycleHandler.signal_running_app_added.connect(boost::bind(&AppLifeManager::onRunningAppAdded, this, _1, _2, _3));
    m_nativeLifecycleHandler.signal_running_app_removed.connect(boost::bind(&AppLifeManager::onRunningAppRemoved, this, _1));
    m_webLifecycleHandler.signal_running_app_removed.connect(boost::bind(&AppLifeManager::onRunningAppRemoved, this, _1));
    m_qmlLifecycleHandler.signal_running_app_removed.connect(boost::bind(&AppLifeManager::onRunningAppRemoved, this, _1));

    // receive signal on launching done
    m_nativeLifecycleHandler.signal_launching_done.connect(boost::bind(&AppLifeManager::onLaunchingDone, this, _1));
    m_webLifecycleHandler.signal_launching_done.connect(boost::bind(&AppLifeManager::onLaunchingDone, this, _1));
    m_qmlLifecycleHandler.signal_launching_done.connect(boost::bind(&AppLifeManager::onLaunchingDone, this, _1));

    // subscriber lsm's foreground info
    LSMSubscriber::instance().signal_foreground_info.connect(boost::bind(&AppLifeManager::onForegroundInfoChanged, this, _1));

    // set fullscreen window type
    m_fullscreenWindowTypes = SettingsImpl::instance().fullscreen_window_types;
}

void AppLifeManager::Launch(LifeCycleTaskPtr task)
{
    launch(AppLaunchRequestType::EXTERNAL, task->app_id(), task->LunaTask()->jmsg(), task->LunaTask()->lsmsg());
}

void AppLifeManager::Pause(LifeCycleTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->LunaTask()->jmsg();

    const pbnjson::JValue& params = jmsg.hasKey("params") && jmsg["params"].isObject() ? jmsg["params"] : pbnjson::Object();
    std::string err_text;
    pauseApp(task->app_id(), params, err_text);

    if (!err_text.empty()) {
        task->FinalizeWithError(API_ERR_CODE_GENERAL, err_text);
        return;
    }
    task->Finalize();
}

void AppLifeManager::Close(LifeCycleTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->LunaTask()->jmsg();

    std::string app_id = jmsg["id"].asString();
    std::string caller_id = task->LunaTask()->caller();
    std::string err_text;
    bool preload_only = false;
    bool handle_pause_app_self = false;
    std::string reason;

    LOG_INFO(MSGID_APPCLOSE, 2,
             PMLOGKS("app_id", app_id.c_str()),
             PMLOGKS("caller", caller_id.c_str()),
             "params: %s", jmsg.duplicate().stringify().c_str());

    if (jmsg.hasKey("preloadOnly"))
        preload_only = jmsg["preloadOnly"].asBool();
    if (jmsg.hasKey("reason"))
        reason = jmsg["reason"].asString();
    if (jmsg.hasKey("letAppHandle"))
        handle_pause_app_self = jmsg["letAppHandle"].asBool();

    if (handle_pause_app_self) {
        pauseApp(app_id, pbnjson::Object(), err_text, false);
    } else {
        AppLifeManager::instance().closeByAppId(app_id, caller_id, reason, err_text, preload_only, false);
    }

    if (!err_text.empty()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", app_id.c_str()), "err_text: %s", err_text.c_str());
        task->FinalizeWithError(API_ERR_CODE_GENERAL, err_text);
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("appId", app_id);
    task->Finalize(payload);
}

void AppLifeManager::CloseByPid(LifeCycleTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->LunaTask()->jmsg();

    std::string pid, err_text;
    std::string caller_id = task->LunaTask()->caller();

    pid = jmsg["processId"].asString();

    AppLifeManager::instance().closeByPid(pid, caller_id, err_text);

    if (!err_text.empty()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("pid", pid.c_str()), "err_text: %s", err_text.c_str());
        task->FinalizeWithError(API_ERR_CODE_GENERAL, err_text);
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("processId", pid);
    task->Finalize(payload);
}

void AppLifeManager::CloseAll(LifeCycleTaskPtr task)
{
    closeAllApps();
}

void AppLifeManager::setApplifeitemFactory(AppLaunchingItemFactoryInterface& factory)
{
    m_launchItemFactory = &factory;
}

void AppLifeManager::setPrelauncherHandler(PrelauncherInterface& prelauncher)
{
    m_prelauncher = &prelauncher;
    prelauncher.signal_prelaunching_done.connect(boost::bind(&AppLifeManager::onPrelaunchingDone, this, _1));
}

void AppLifeManager::setMemoryCheckerHandler(MemoryCheckerInterface& memory_checker)
{
    m_memoryChecker = &memory_checker;
    memory_checker.signal_memory_checking_start.connect(boost::bind(&AppLifeManager::onMemoryCheckingStart, this, _1));
    memory_checker.signal_memory_checking_done.connect(boost::bind(&AppLifeManager::onMemoryCheckingDone, this, _1));
}

void AppLifeManager::setLastappHandler(LastAppHandlerInterface& lastapp_handler)
{
    m_lastappHandler = &lastapp_handler;
}

void AppLifeManager::onPrelaunchingDone(const std::string& uid)
{
    AppLaunchingItemPtr item = getLaunchingItemByUid(uid);
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS("reason", "null_pointer"),
                  PMLOGKS("where", "on_prelaunching_done"), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 4,
             PMLOGKS("app_id", item->appId().c_str()),
             PMLOGKS("uid", uid.c_str()),
             PMLOGKS("status", "prelaunching_done"),
             PMLOGKFV("launching_stage_in_detail", "%d", item->subStage()), "");

    if (AppLaunchingStage::PRELAUNCH != item->stage()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 4,
                  PMLOGKS("app_id", item->appId().c_str()),
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS("reason", "not_in_prelaunching_stage"),
                  PMLOGKS("where", "on_prelaunching_done"), "");
        return;
    }

    // just finish launch if error occurs
    if (item->errText().empty() == false) {
        finishLaunching(item);
        return;
    }

    // go next stage
    runWithMemoryChecker(item);
}

void AppLifeManager::onMemoryCheckingStart(const std::string& uid)
{
    AppLaunchingItemPtr item = getLaunchingItemByUid(uid);
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS("reason", "null_pointer"),
                  PMLOGKS("where", "on_memory_checking_start"), "");
        return;
    }
    generateLifeCycleEvent(item->appId(), uid, LifeEvent::SPLASH);
}

void AppLifeManager::onMemoryCheckingDone(const std::string& uid)
{
    AppLaunchingItemPtr item = getLaunchingItemByUid(uid);
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS("reason", "null_pointer"),
                  PMLOGKS("where", "on_memory_checking_done"), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 4,
             PMLOGKS("app_id", item->appId().c_str()),
             PMLOGKS("uid", uid.c_str()),
             PMLOGKS("status", "memory_checking_done"),
             PMLOGKFV("launching_stage_in_detail", "%d", item->subStage()), "");

    if (AppLaunchingStage::MEMORY_CHECK != item->stage()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 4,
                  PMLOGKS("app_id", item->appId().c_str()),
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS("reason", "not_in_memory_checking_stage"),
                  PMLOGKS("where", "on_memory_checking_done"), "");
        return;
    }

    // just finish launch if error occurs
    if (item->errText().empty() == false) {
        finishLaunching(item);
        return;
    }

    // go next stage
    runWithLauncher(item);
}

void AppLifeManager::onLaunchingDone(const std::string& uid)
{
    AppLaunchingItemPtr item = getLaunchingItemByUid(uid);
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS("reason", "null_pointer"),
                  PMLOGKS("where", "on_launching_done"), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", item->appId().c_str()),
             PMLOGKS("uid", uid.c_str()),
             PMLOGKS("status", "launching_done"), "");

    finishLaunching(item);
}

void AppLifeManager::runWithPrelauncher(AppLaunchingItemPtr item)
{
    LOG_INFO_WITH_CLOCK(MSGID_APPLAUNCH, 4,
                        PMLOGKS("app_id", item->appId().c_str()),
                        PMLOGKS("status", "start_prelaunching"),
                        PMLOGKS("PerfType", "AppLaunch"),
                        PMLOGKS("PerfGroup", item->appId().c_str()), "");

    if (m_prelauncher == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", item->appId().c_str()),
                  PMLOGKS("reason", "no_prelauncher_handler"),
                  PMLOGKS("where", "run_with_prelauncher"), "");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        finishLaunching(item);
        return;
    }

    item->setStage(AppLaunchingStage::PRELAUNCH);
    m_prelauncher->addItem(item);
}

void AppLifeManager::runWithMemoryChecker(AppLaunchingItemPtr item)
{
    LOG_INFO_WITH_CLOCK(MSGID_APPLAUNCH, 4,
                        PMLOGKS("app_id", item->appId().c_str()),
                        PMLOGKS("status", "start_memory_checking"),
                        PMLOGKS("PerfType", "AppLaunch"),
                        PMLOGKS("PerfGroup", item->appId().c_str()), "");

    if (m_memoryChecker == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 1, PMLOGKS("app_id", item->appId().c_str()), "memorychecker is not registered");
        return;
    }
    item->setStage(AppLaunchingStage::MEMORY_CHECK);
    m_memoryChecker->add_item(item);
    m_memoryChecker->run();
}

void AppLifeManager::runWithLauncher(AppLaunchingItemPtr item)
{
    LOG_INFO_WITH_CLOCK(MSGID_APPLAUNCH, 4,
                        PMLOGKS("app_id", item->appId().c_str()),
                        PMLOGKS("status", "start_launching"),
                        PMLOGKS("PerfType", "AppLaunch"),
                        PMLOGKS("PerfGroup", item->appId().c_str()), "");

    item->setStage(AppLaunchingStage::LAUNCH);
    launchApp(item);
}

void AppLifeManager::runLastappHandler()
{
    if (m_lastappHandler == NULL) {
        LOG_ERROR(MSGID_LAUNCH_LASTAPP_ERR, 2,
                  PMLOGKS("reason", "no_lastapp_handler"),
                  PMLOGKS("where", "run_lastapp_handler"), "");
        return;
    }

    if (isFullscreenAppLoading("", "")) {
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 1, PMLOGKS("status", "skip_launching_last_input_app"), "");
        return;
    }

    m_lastappHandler->launch();
}

void AppLifeManager::finishLaunching(AppLaunchingItemPtr item)
{
    bool is_automatic_launch = item->automaticLaunch();
    bool is_last_app_candidate = isLastLaunchingApp(item->appId()) && (m_lastLaunchingApps.size() == 1);

    bool redirect_to_lastapplaunch = (is_automatic_launch || item->isLastInputApp() || is_last_app_candidate) && !item->errText().empty();

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", item->appId().c_str()),
             PMLOGKS("status", "finish_launching"),
             PMLOGKS("mode", (is_automatic_launch ? "automatic_launch":"normal")), "");

    signal_launching_finished(item);
    replyWithResult(item->lsmsg(), item->pid(), item->errText().empty(), item->errCode(), item->errText());
    removeLastLaunchingApp(item->appId());

    std::string app_uid = item->uid();
    removeItem(app_uid);

    // TODO: decide if this is tv specific or not
    //       make tv handler if it's tv specific
    if (redirect_to_lastapplaunch) {
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 1, PMLOGKS("action", "trigger_launch_lastapp"), "");
        runLastappHandler();
    }
}

void AppLifeManager::onRuntimeStatusChanged(const std::string& app_id, const std::string& uid, const RuntimeStatus& new_status)
{

    if (app_id.empty()) {
        LOG_ERROR(MSGID_LIFE_STATUS_ERR, 2, PMLOGKS("reason", "empty_app_id"), PMLOGKS("where", __FUNCTION__), "");
        return;
    }

    m_lifecycleRouter.setRuntimeStatus(app_id, new_status);
    LifeStatus new_life_status = m_lifecycleRouter.getLifeStatusFromRuntimeStatus(new_status);

    setAppLifeStatus(app_id, uid, new_life_status);
}

void AppLifeManager::setAppLifeStatus(const std::string& appId, const std::string& uid, LifeStatus newStatus)
{
    std::cout << "uid : " << uid << std::endl;
    AppDescPtr appDesc = ApplicationManager::instance().getAppById(appId);
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
            addLoadingApp(appId, appDesc->type());
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

void AppLifeManager::stopAllWebappItem()
{
    LOG_INFO(MSGID_APPLAUNCH_ERR, 2,
             PMLOGKS("status", "remove_webapp_in_loading_list"),
             PMLOGKS("reason", "WAM_disconnected"), "");

    std::vector<LoadingAppItem> loading_apps = m_loadingAppList;
    for (const auto& app : loading_apps) {
        if (std::get<1>(app) == AppType::Web)
            onRuntimeStatusChanged(std::get<0>(app), "", RuntimeStatus::STOP);
    }
}

void AppLifeManager::onRunningAppAdded(const std::string& app_id, const std::string& pid, const std::string& webprocid)
{
    AppInfoManager::instance().addRunningInfo(app_id, pid, webprocid);
    onRunningListChanged(app_id);
}

void AppLifeManager::onRunningAppRemoved(const std::string& app_id)
{

    AppInfoManager::instance().removeRunningInfo(app_id);
    onRunningListChanged(app_id);

    if (isInAutomaticPendingList(app_id))
        handleAutomaticApp(app_id);
}

void AppLifeManager::onRunningListChanged(const std::string& app_id)
{
    pbnjson::JValue running_list = pbnjson::Array();
    AppInfoManager::instance().getRunningList(running_list);
    replySubscriptionForRunning(running_list);

    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if (app_desc != NULL && AppTypeByDir::Dev == app_desc->getTypeByDir()) {
        running_list = pbnjson::Array();
        AppInfoManager::instance().getRunningList(running_list, true);
        replySubscriptionForRunning(running_list, true);
    }
}

void AppLifeManager::replyWithResult(LSMessage* lsmsg, const std::string& pid, bool result, const int& err_code, const std::string& err_text)
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

void AppLifeManager::replySubscriptionForAppLifeStatus(const std::string& app_id, const std::string& uid, const LifeStatus& life_status)
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

    AppLaunchingItemPtr launch_item = uid.empty() ? NULL : getLaunchingItemByUid(uid);
    std::string str_status = AppInfoManager::instance().toString(life_status);
    bool preloaded = AppInfoManager::instance().preloadModeOn(app_id);

    payload.put("status", str_status);    // change enum to string
    payload.put("appId", app_id);

    std::string pid = AppInfoManager::instance().pid(app_id);
    if (!pid.empty())
        payload.put("processId", pid);

    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);

    if (app_desc != NULL)
        payload.put("type", ApplicationDescription::appTypeToString(app_desc->type()));

    if (LifeStatus::LAUNCHING == life_status || LifeStatus::RELAUNCHING == life_status) {
        if (launch_item) {
            payload.put("reason", launch_item->launchReason());
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

void AppLifeManager::generateLifeCycleEvent(const std::string& app_id, const std::string& uid, LifeEvent event)
{

    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    AppLaunchingItemPtr launch_item = uid.empty() ? NULL : getLaunchingItemByUid(uid);
    LifeStatus current_status = AppInfoManager::instance().lifeStatus(app_id);
    bool preloaded = AppInfoManager::instance().preloadModeOn(app_id);

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("appId", app_id);

    if (LifeEvent::SPLASH == event) {
        // generate splash event only for fresh launch case
        if (launch_item && !launch_item->showSplash() && !launch_item->showSpinner())
            return;
        if (LifeStatus::BACKGROUND == current_status && preloaded == false)
            return;
        if (LifeStatus::STOP != current_status && LifeStatus::PRELOADING != current_status && LifeStatus::BACKGROUND != current_status)
            return;

        payload.put("event", "splash");
        payload.put("title", (app_desc ? app_desc->title() : ""));
        payload.put("showSplash", (launch_item && launch_item->showSplash()));
        payload.put("showSpinner", (launch_item && launch_item->showSpinner()));

        if (launch_item && launch_item->showSplash())
            payload.put("splashBackground", (app_desc ? app_desc->splashBackground() : ""));
    } else if (LifeEvent::PRELOAD == event) {
        payload.put("event", "preload");
        if (launch_item)
            payload.put("preload", launch_item->preload());
    } else if (LifeEvent::LAUNCH == event) {
        payload.put("event", "launch");
        payload.put("reason", launch_item->launchReason());
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

void AppLifeManager::replySubscriptionForRunning(const pbnjson::JValue& running_list, bool devmode)
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

void AppLifeManager::launch(AppLaunchRequestType rtype, const std::string& appId, const pbnjson::JValue& params, LSMessage* lsmsg)
{
    // check launching item factory
    if (m_launchItemFactory == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 1, PMLOGKS("status", "no_launch_item_factory_registered"), "");
        replyWithResult(lsmsg, "", false, (int) APP_LAUNCH_ERR_GENERAL, "internal error");
        return;
    }

    int errCode = 0;
    std::string errText = "";
    AppLaunchingItemPtr newItem = m_launchItemFactory->Create(appId, rtype, params, lsmsg, errCode, errText);
    if (newItem == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", appId.c_str()),
                  PMLOGKS("reason", "creating_item_fail"),
                  PMLOGKS("where", "launch_in_app_life_manager"), "");
        if (errText.empty()) {
            replyWithResult(lsmsg, "", false, (int) APP_LAUNCH_ERR_GENERAL, "internal error");
        } else {
            replyWithResult(lsmsg, "", false, errCode, errText);
        }
        return;
    }

    // set start time
    newItem->setLaunchStartTime(get_current_time());

    // put new request into launching queue
    m_launchItemList.push_back(newItem);

    if (newItem->automaticLaunch()) {
        if (isFullscreenAppLoading(newItem->appId(), newItem->uid())) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", newItem->appId().c_str()),
                     PMLOGKS("status", "skip_launching_last_app"), "");
            finishLaunching(newItem);
            return;
        }

        if (newItem->launchReason() == "autoReload" && AppInfoManager::instance().isRunning(newItem->appId())) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", newItem->appId().c_str()),
                     PMLOGKS("status", "waiting_for_foreground_app_removed"), "");
            addItemIntoAutomaticPendingList(newItem);
            return;
        }
    }

    // start prelaunching
    runWithPrelauncher(newItem);
}

void AppLifeManager::handleBridgedLaunchRequest(const pbnjson::JValue& params)
{
    std::string item_uid;
    if (!params.hasKey(SYS_LAUNCHING_UID) || params[SYS_LAUNCHING_UID].asString(item_uid) != CONV_OK) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "uid_not_found"),
                  PMLOGKS("where", "handle_bridged_launch"), "");
        return;
    }

    AppLaunchingItemPtr launching_item = getLaunchingItemByUid(item_uid);
    if (launching_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("uid", item_uid.c_str()),
                  PMLOGKS("reason", "launching_item_not_found"),
                  PMLOGKS("where", "handle_bridged_launch"), "");
        return;
    }

    // set return payload
    launching_item->setCallReturnJmsg(params);

    if (m_prelauncher)
        m_prelauncher->inputBridgedReturn(launching_item, params);
}

void AppLifeManager::registerApp(const std::string& appId, LSMessage* lsmsg, std::string& errText)
{
    AppDescPtr appDesc = ApplicationManager::instance().getAppById(appId);
    if (!appDesc) {
        errText = "not existing app";
        return;
    }

    if (appDesc->NativeInterfaceVersion() != 2) {
        errText = "trying to register via unmatched method with nativeLifeCycleInterfaceVersion";
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", appId.c_str()),
                  PMLOGKS("reason", "wrong_version_interface_on_register"),
                  PMLOGKS("method", "registerNativeApp"),
                  "nativeLifeCycleInterfaceVersion: %d", appDesc->NativeInterfaceVersion());
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

    m_nativeLifecycleHandler.RegisterApp(appId, lsmsg, errText);
}

void AppLifeManager::connectNativeApp(const std::string& app_id, LSMessage* lsmsg, std::string& err_text)
{

    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if (!app_desc) {
        err_text = "not existing app";
        return;
    }

    if (app_desc->NativeInterfaceVersion() != 1) {
        err_text = "trying to register via unmatched method with nativeLifeCycleInterfaceVersion";
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("reason", "wrong_version_interface_on_register"),
                  PMLOGKS("method", "registerNativeApp"),
                  "nativeLifeCycleInterfaceVersion: %d", app_desc->NativeInterfaceVersion());
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

    m_nativeLifecycleHandler.RegisterApp(app_id, lsmsg, err_text);
}

bool AppLifeManager::changeRunningAppId(const std::string& current_id, const std::string& target_id, ErrorInfo& err_info)
{
    return m_nativeLifecycleHandler.ChangeRunningAppId(current_id, target_id, err_info);
}

void AppLifeManager::triggerToLaunchLastApp()
{

    std::string fg_app_id = AppInfoManager::instance().getCurrentForegroundAppId();
    if (!fg_app_id.empty() && AppInfoManager::instance().isRunning(fg_app_id))
        return;

    LOG_INFO(MSGID_LAUNCH_LASTAPP, 1, PMLOGKS("status", "trigger_to_launch_last_app"), "");

    runLastappHandler();
}

void AppLifeManager::closeByAppId(const std::string& app_id, const std::string& caller_id, const std::string& reason, std::string& err_text, bool preload_only, bool clear_all_items)
{

    if (preload_only && !hasOnlyPreloadedItems(app_id)) {
        err_text = "app is being launched by user";
        return;
    }

    // keepAlive app policy
    // swith close request to pause request
    // except for below cases
    if (SettingsImpl::instance().IsKeepAliveApp(app_id) &&
        caller_id != "com.webos.memorymanager" &&
        caller_id != "com.webos.appInstallService" &&
        (caller_id != "com.webos.surfacemanager.windowext" || reason != "recent") &&
        caller_id != SAM_INTERNAL_ID) {
        pauseApp(app_id, pbnjson::Object(), err_text);
        return;
    }

    closeApp(app_id, caller_id, reason, err_text, clear_all_items);
}

void AppLifeManager::closeByPid(const std::string& pid, const std::string& caller_id, std::string& err_text)
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

void AppLifeManager::closeAllLoadingApps()
{
    resetLastAppCandidates();

    m_prelauncher->cancelAll();
    m_memoryChecker->cancel_all();

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

void AppLifeManager::closeAllApps(bool clear_all_items)
{
    LOG_INFO(MSGID_APPCLOSE, 2, PMLOGKS("action", "close_all_apps"), PMLOGKS("status", "start"), "");
    std::vector<std::string> running_apps;
    AppInfoManager::instance().getRunningAppIds(running_apps);
    closeApps(running_apps, clear_all_items);
    resetLastAppCandidates();
}

void AppLifeManager::closeApps(const std::vector<std::string>& app_ids, bool clear_all_items)
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

void AppLifeManager::launchApp(AppLaunchingItemPtr item)
{
    AppLifeHandlerInterface* life_handler = getLifeHandlerForApp(item->appId());
    if (nullptr == life_handler) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", item->appId().c_str()),
                  PMLOGKS("reason", "null_description"),
                  PMLOGKS("where", "launch_app_in_app_life_manager"), "");
        return;
    }

    life_handler->launch(item);
}

void AppLifeManager::closeApp(const std::string& appId, const std::string& callerId, const std::string& reason, std::string& errText, bool clearAllItems)
{
    if (clearAllItems) {
        clearLaunchingAndLoadingItemsByAppId(appId);
    }

    AppLifeHandlerInterface* life_handler = getLifeHandlerForApp(appId);
    if (nullptr == life_handler) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", appId.c_str()), "no valid life handler");
        errText = "no valid life handler";
        return;
    }

    std::string close_reason = SettingsImpl::instance().GetCloseReason(callerId, reason);

    if (AppInfoManager::instance().isRunning(appId) && m_closeReasonInfo.count(appId) == 0) {
        m_closeReasonInfo[appId] = close_reason;
    }

    const std::string& pid = AppInfoManager::instance().pid(appId);
    AppCloseItemPtr item = std::make_shared<AppCloseItem>(appId, pid, callerId, close_reason);
    if (nullptr == item) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", appId.c_str()), "memory alloc fail");
        errText = "memory alloc fail";
        return;
    }

    LOG_INFO(MSGID_APPCLOSE, 2, PMLOGKS("app_id", appId.c_str()), PMLOGKS("pid", pid.c_str()), "");

    life_handler->close(item, errText);
}

void AppLifeManager::pauseApp(const std::string& app_id, const pbnjson::JValue& params, std::string& err_text, bool report_event)
{

    if (!AppInfoManager::instance().isRunning(app_id)) {
        err_text = "app is not running";
        return;
    }

    AppLifeHandlerInterface* life_handler = getLifeHandlerForApp(app_id);
    if (nullptr == life_handler) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", app_id.c_str()), "no valid life handler");
        err_text = "no valid life handler";
        return;
    }

    life_handler->pause(app_id, params, err_text, report_event);
}

bool AppLifeManager::isFullscreenWindowType(const pbnjson::JValue& foreground_info)
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

void AppLifeManager::onForegroundInfoChanged(const pbnjson::JValue& jmsg)
{
    if (jmsg.isNull() ||
        !jmsg["returnValue"].asBool() ||
        !jmsg.hasKey("foregroundAppInfo") ||
        !jmsg["foregroundAppInfo"].isArray()) {
        LOG_ERROR(MSGID_GET_FOREGROUND_APP_ERR, 1, PMLOGJSON("payload", jmsg.duplicate().stringify().c_str()), "invalid message from LSM");
        return;
    }

    pbnjson::JValue lsm_foreground_info = jmsg["foregroundAppInfo"];

    LOG_INFO(MSGID_GET_FOREGROUND_APPINFO, 1, PMLOGJSON("ForegroundApps", lsm_foreground_info.stringify().c_str()), "");

    std::string prev_foreground_app_id = AppInfoManager::instance().getCurrentForegroundAppId();
    std::vector<std::string> prev_foreground_apps = AppInfoManager::instance().getForegroundApps();
    std::vector<std::string> new_foreground_apps;
    pbnjson::JValue prev_foreground_info = AppInfoManager::instance().getJsonForegroundInfo();
    std::string new_foreground_app_id = "";
    bool found_fullscreen_window = false;

    pbnjson::JValue new_foreground_info = pbnjson::Array();
    int array_size = lsm_foreground_info.arraySize();
    for (int i = 0; i < array_size; ++i) {
        std::string app_id;
        if (!lsm_foreground_info[i].hasKey("appId") ||
            !lsm_foreground_info[i]["appId"].isString() ||
            lsm_foreground_info[i]["appId"].asString(app_id) != CONV_OK ||
            app_id.empty()) {
            continue;
        }

        LifeStatus current_status = AppInfoManager::instance().lifeStatus(app_id);
        if (LifeStatus::STOP == current_status) {
            LOG_INFO(MSGID_FOREGROUND_INFO, 1, PMLOGKS("filter_out", app_id.c_str()), "remove not running app");
            continue;
        }

        LOG_INFO_WITH_CLOCK(MSGID_GET_FOREGROUND_APPINFO, 2,
                            PMLOGKS("PerfType", "AppLaunch"),
                            PMLOGKS("PerfGroup", app_id.c_str()), "");

        new_foreground_info.append(lsm_foreground_info[i].duplicate());
        new_foreground_apps.push_back(app_id);

        if (isFullscreenWindowType(lsm_foreground_info[i])) {
            found_fullscreen_window = true;
            new_foreground_app_id = app_id;
        }
    }

    LOG_INFO(MSGID_GET_FOREGROUND_APPINFO, 1, PMLOGJSON("FilteredForegroundApps", new_foreground_info.duplicate().stringify().c_str()), "");

    // update foreground info into app info manager
    if (found_fullscreen_window) {
        AppInfoManager::instance().setLastForegroundAppId(new_foreground_app_id);
        resetLastAppCandidates();
    }
    AppInfoManager::instance().setCurrentForegroundAppId(new_foreground_app_id);
    AppInfoManager::instance().setForegroundApps(new_foreground_apps);
    AppInfoManager::instance().setForegroundInfo(new_foreground_info);

    LOG_INFO(MSGID_FOREGROUND_INFO, 2,
             PMLOGKS("current_foreground_app", new_foreground_app_id.c_str()),
             PMLOGKS("prev_foreground_app", prev_foreground_app_id.c_str()), "");

    // set background
    for (auto& prev_app_id : prev_foreground_apps) {
        bool found = false;
        for (auto& current_app_id : new_foreground_apps) {
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
    for (auto& current_app_id : new_foreground_apps) {
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
    if (prev_foreground_info != new_foreground_info) {
        signal_foreground_extra_info_changed(new_foreground_info);
    }
}

bool AppLifeManager::isFullscreenAppLoading(const std::string& new_launching_app_id, const std::string& new_launching_app_uid)
{
    bool result = false;

    for (auto& launching_item : m_launchItemList) {
        if (new_launching_app_id == launching_item->appId() && launching_item->uid() == new_launching_app_uid)
            continue;

        AppDescPtr app_desc = ApplicationManager::instance().getAppById(launching_item->appId());
        if (app_desc == NULL) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", launching_item->appId().c_str()),
                     PMLOGKS("status", "not_candidate_checking_launching"), "null description");
            continue;
        }

        if (("com.webos.app.container" == launching_item->appId()) || ("com.webos.app.inputcommon" == launching_item->appId()))
            continue;

        if (app_desc->is_child_window()) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", launching_item->appId().c_str()),
                     PMLOGKS("status", "not_candidate_checking_launching"), "child window type");
            continue;
        }
        if (!(launching_item->preload().empty())) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", launching_item->appId().c_str()),
                     PMLOGKS("status", "not_candidate_checking_launching"), "preloading app");
            continue;
        }

        if (app_desc->defaultWindowType() == "card" || app_desc->defaultWindowType() == "minimal") {
            if (isLaunchingItemExpired(launching_item)) {
                LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
                         PMLOGKS("app_id", launching_item->appId().c_str()),
                         PMLOGKS("status", "not_candidate_checking_launching"),
                         PMLOGKFV("launching_stage_in_detail", "%d", launching_item->subStage()),
                         "fullscreen app, but expired");
                continue;
            }

            LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
                     PMLOGKS("status", "autolaunch_condition_check"),
                     PMLOGKS("launching_app_id", launching_item->appId().c_str()),
                     PMLOGKFV("launching_stage_in_detail", "%d", launching_item->subStage()),
                     "fullscreen app is already launching");

            addLastLaunchingApp(launching_item->appId());
            result = true;
        } else {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                    PMLOGKS("app_id", launching_item->appId().c_str()),
                    PMLOGKS("status", "not_candidate_checking_launching"),
                    "not fullscreen type: %s", app_desc->defaultWindowType().c_str());
        }
    }

    std::vector<std::string> app_ids;
    getLoadingAppIds(app_ids);

    for (auto& app_id : app_ids) {
        AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
        if (app_desc == NULL) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", app_id.c_str()),
                     PMLOGKS("status", "not_candidate_checking_loading"), "null description");
            continue;
        }
        if (app_desc->is_child_window()) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", app_id.c_str()),
                     PMLOGKS("status", "not_candidate_checking_loading"), "child window type");
            continue;
        }
        if (AppInfoManager::instance().preloadModeOn(app_id)) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", app_id.c_str()),
                     PMLOGKS("status", "not_candidate_checking_loading"), "preloading app");
            continue;
        }

        if (app_desc->defaultWindowType() == "card" || app_desc->defaultWindowType() == "minimal") {
            if (isLoadingAppExpired(app_id)) {
                LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                         PMLOGKS("app_id", app_id.c_str()),
                         PMLOGKS("status", "not_candidate_checking_loading"), "fullscreen app, but expired");
                continue;
            }

            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("status", "autolaunch_condition_check"),
                     PMLOGKS("loading_app_id", app_id.c_str()), "fullscreen app is already loading");

            setLastLoadingApp(app_id);
            result = true;
            break;
        } else {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("app_id", app_id.c_str()),
                     PMLOGKS("status", "not_candidate_checking_loading"), "not fullscreen type: %s", app_desc->defaultWindowType().c_str());
        }
    }

    return result;
}

void AppLifeManager::clearLaunchingAndLoadingItemsByAppId(const std::string& app_id)
{
    bool found = false;

    while (true) {
        auto it = std::find_if(m_launchItemList.begin(), m_launchItemList.end(), [&app_id](AppLaunchingItemPtr item) {return (item->appId() == app_id);});

        if (it == m_launchItemList.end())
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

void AppLifeManager::handleAutomaticApp(const std::string& app_id, bool continue_to_launch)
{
    AppLaunchingItemPtr item = getLaunchingItemByAppId(app_id);
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

void AppLifeManager::getLaunchingAppIds(std::vector<std::string>& app_ids)
{
    for (auto& launching_item : m_launchItemList)
        app_ids.push_back(launching_item->appId());
}

AppLaunchingItemPtr AppLifeManager::getLaunchingItemByUid(const std::string& uid)
{
    for (auto& launching_item : m_launchItemList) {
        if (launching_item->uid() == uid)
            return launching_item;
    }
    return NULL;
}

AppLaunchingItemPtr AppLifeManager::getLaunchingItemByAppId(const std::string& app_id)
{
    for (auto& launching_item : m_launchItemList) {
        if (launching_item->appId() == app_id)
            return launching_item;
    }
    return NULL;
}

void AppLifeManager::removeItem(const std::string& uid)
{
    auto it = std::find_if(m_launchItemList.begin(), m_launchItemList.end(),
                          [&uid](AppLaunchingItemPtr item) {return (item->uid() == uid);});
    if (it == m_launchItemList.end())
        return;
    m_launchItemList.erase(it);
}

void AppLifeManager::addItemIntoAutomaticPendingList(AppLaunchingItemPtr item)
{
    m_automaticPendingList.push_back(item);
    LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
             PMLOGKS("app_id", item->appId().c_str()),
             PMLOGKS("mode", "pending_automatic_app"),
             PMLOGKS("status", "added_into_list"), "");
}

void AppLifeManager::removeItemFromAutomaticPendingList(const std::string& app_id)
{
    auto it = std::find_if(m_automaticPendingList.begin(), m_automaticPendingList.end(),
                          [&app_id](AppLaunchingItemPtr item) {return (item->appId() == app_id);});
    if (it != m_automaticPendingList.end()) {
        m_automaticPendingList.erase(it);
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("mode", "pending_automatic_app"),
                 PMLOGKS("status", "removed_from_list"), "");
    }
}

bool AppLifeManager::isInAutomaticPendingList(const std::string& app_id)
{
    auto it = std::find_if(m_automaticPendingList.begin(), m_automaticPendingList.end(),
                          [&app_id](AppLaunchingItemPtr item) {return (item->appId() == app_id);});

    if (it != m_automaticPendingList.end())
        return true;
    else
        return false;
}

void AppLifeManager::getAutomaticPendingAppIds(std::vector<std::string>& app_ids)
{
    for (auto& launching_item : m_automaticPendingList)
        app_ids.push_back(launching_item->appId());
}

void AppLifeManager::getLoadingAppIds(std::vector<std::string>& app_ids)
{
    for (auto& app : m_loadingAppList)
        app_ids.push_back(std::get<0>(app));
}

void AppLifeManager::addLoadingApp(const std::string& app_id, const AppType& type)
{
    for (auto& loading_app : m_loadingAppList)
        if (std::get<0>(loading_app) == app_id)
            return;
    // exception
    if ("com.webos.app.container" == app_id || "com.webos.app.inputcommon" == app_id) {
        LOG_INFO(MSGID_LOADING_LIST, 1, PMLOGKS("app_id", app_id.c_str()), "apply exception (skip)");
        return;
    }

    LoadingAppItem new_loading_app = std::make_tuple(app_id, type, static_cast<double>(get_current_time()));
    m_loadingAppList.push_back(new_loading_app);

    LOG_INFO(MSGID_LOADING_LIST, 1, PMLOGKS("app_id", app_id.c_str()), "added");

    if (isLastLaunchingApp(app_id))
        setLastLoadingApp(app_id);
}

void AppLifeManager::removeLoadingApp(const std::string& app_id)
{
    auto it = std::find_if(m_loadingAppList.begin(), m_loadingAppList.end(), [&app_id](const LoadingAppItem& loading_app) {return (std::get<0>(loading_app) == app_id);});
    if (it == m_loadingAppList.end())
        return;
    m_loadingAppList.erase(it);
    LOG_INFO(MSGID_LOADING_LIST, 1, PMLOGKS("app_id", app_id.c_str()), "removed");

    if (m_lastLoadingAppTimerSet.second == app_id)
        removeTimerForLastLoadingApp(true);
}

void AppLifeManager::setLastLoadingApp(const std::string& app_id)
{
    auto it = std::find_if(m_loadingAppList.begin(), m_loadingAppList.end(),
                          [&app_id](const LoadingAppItem& loading_app) {return (std::get<0>(loading_app) == app_id);});
    if (it == m_loadingAppList.end())
        return;

    addTimerForLastLoadingApp(app_id);
    removeLastLaunchingApp(app_id);
}

void AppLifeManager::addTimerForLastLoadingApp(const std::string& app_id)
{
    if ((m_lastLoadingAppTimerSet.first != 0) && (m_lastLoadingAppTimerSet.second == app_id))
        return;

    auto it = std::find_if(m_loadingAppList.begin(), m_loadingAppList.end(),
                          [&app_id](const LoadingAppItem& loading_app) {return (std::get<0>(loading_app) == app_id);});
    if (it == m_loadingAppList.end())
        return;

    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if (app_desc == NULL) {
        LOG_ERROR(MSGID_LAUNCH_LASTAPP_ERR, 3,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("status", "cannot_add_timer_for_last_loading_app"),
                  PMLOGKS("reason", "null description"), "");
        return;
    }

    removeTimerForLastLoadingApp(false);

    guint timer = g_timeout_add(SettingsImpl::instance().GetLastLoadingAppTimeout(), runLastLoadingAppTimeoutHandler, NULL);
    m_lastLoadingAppTimerSet = std::make_pair(timer, app_id);
}

void AppLifeManager::removeTimerForLastLoadingApp(bool trigger)
{
    if (m_lastLoadingAppTimerSet.first == 0)
        return;

    g_source_remove(m_lastLoadingAppTimerSet.first);
    m_lastLoadingAppTimerSet = std::make_pair(0, "");

    if (trigger)
        triggerToLaunchLastApp();
}

gboolean AppLifeManager::runLastLoadingAppTimeoutHandler(gpointer user_data)
{
    AppLifeManager::instance().removeTimerForLastLoadingApp(true);
    return FALSE;
}

void AppLifeManager::addLastLaunchingApp(const std::string& app_id)
{
    auto it = std::find(m_lastLaunchingApps.begin(), m_lastLaunchingApps.end(), app_id);
    if (it != m_lastLaunchingApps.end())
        return;

    m_lastLaunchingApps.push_back(app_id);
}

void AppLifeManager::removeLastLaunchingApp(const std::string& app_id)
{
    auto it = std::find(m_lastLaunchingApps.begin(), m_lastLaunchingApps.end(), app_id);
    if (it == m_lastLaunchingApps.end())
        return;

    m_lastLaunchingApps.erase(it);
}

bool AppLifeManager::isLastLaunchingApp(const std::string& app_id)
{
    auto it = std::find(m_lastLaunchingApps.begin(), m_lastLaunchingApps.end(), app_id);
    return {(it != m_lastLaunchingApps.end()) ? true : false};
}

void AppLifeManager::resetLastAppCandidates()
{
    m_lastLaunchingApps.clear();
    removeTimerForLastLoadingApp(false);
}

bool AppLifeManager::hasOnlyPreloadedItems(const std::string& app_id)
{
    for (auto& launching_item : m_launchItemList) {
        if (launching_item->appId() == app_id && launching_item->preload().empty())
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

bool AppLifeManager::isLaunchingItemExpired(AppLaunchingItemPtr item)
{
    double current_time = get_current_time();
    double elapsed_time = current_time - item->launchStartTime();

    if (elapsed_time > SettingsImpl::instance().GetLaunchExpiredTimeout())
        return true;

    return false;
}

bool AppLifeManager::isLoadingAppExpired(const std::string& app_id)
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

    double current_time = get_current_time();
    double elapsed_time = current_time - loading_start_time;

    if (elapsed_time > SettingsImpl::instance().GetLoadingExpiredTimeout())
        return true;

    return false;
}

AppLifeHandlerInterface* AppLifeManager::getLifeHandlerForApp(const std::string& app_id)
{
    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if (app_desc == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("reason", "null_description"),
                  PMLOGKS("where", __FUNCTION__), "");
        return nullptr;
    }

    switch (app_desc->handler_type()) {
    case LifeHandlerType::Web:
        return &m_webLifecycleHandler;
        break;
    case LifeHandlerType::Qml:
        return &m_qmlLifecycleHandler;
        break;
    case LifeHandlerType::Native:
        return &m_nativeLifecycleHandler;
        break;
    default:
        break;
    }

    return nullptr;
}
