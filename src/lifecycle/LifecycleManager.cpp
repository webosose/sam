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
#include <bus/client/LSM.h>
#include <bus/service/ApplicationManager.h>
#include <lifecycle/LifecycleManager.h>
#include <package/AppPackageManager.h>
#include <setting/Settings.h>
#include "bus/client/WAM.h"
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
    m_lifecycleRouter.initialize();
    RunningInfoManager::getInstance().initialize();

    WAM::getInstance().EventServiceStatusChanged.connect(boost::bind(&LifecycleManager::onWAMStatusChanged, this, _1));

    m_prelauncher.EventPrelaunchingDone.connect(boost::bind(&LifecycleManager::onPrelaunchingDone, this, _1));

    m_memoryChecker.EventMemoryCheckingStart.connect(boost::bind(&LifecycleManager::onMemoryCheckingStart, this, _1));
    m_memoryChecker.EventMemoryCheckingDone.connect(boost::bind(&LifecycleManager::onMemoryCheckingDone, this, _1));

    // receive signal on life status change
    NativeAppLifeHandler::getInstance().EventAppLifeStatusChanged.connect(boost::bind(&LifecycleManager::onRuntimeStatusChanged, this, _1, _2, _3));
    m_webLifecycleHandler.EventAppLifeStatusChanged.connect(boost::bind(&LifecycleManager::onRuntimeStatusChanged, this, _1, _2, _3));
    m_qmlLifecycleHandler.EventAppLifeStatusChanged.connect(boost::bind(&LifecycleManager::onRuntimeStatusChanged, this, _1, _2, _3));

    // receive signal on running list change
    NativeAppLifeHandler::getInstance().EventRunningAppAdded.connect(boost::bind(&LifecycleManager::onRunningAppAdded, this, _1, _2, _3));
    m_webLifecycleHandler.EventRunningAppAdded.connect(boost::bind(&LifecycleManager::onRunningAppAdded, this, _1, _2, _3));
    m_qmlLifecycleHandler.EventRunningAppAdded.connect(boost::bind(&LifecycleManager::onRunningAppAdded, this, _1, _2, _3));

    NativeAppLifeHandler::getInstance().EventRunningAppRemoved.connect(boost::bind(&LifecycleManager::onRunningAppRemoved, this, _1));
    m_webLifecycleHandler.EventRunningAppRemoved.connect(boost::bind(&LifecycleManager::onRunningAppRemoved, this, _1));
    m_qmlLifecycleHandler.EventRunningAppRemoved.connect(boost::bind(&LifecycleManager::onRunningAppRemoved, this, _1));

    // receive signal on launching done
    NativeAppLifeHandler::getInstance().EventLaunchingDone.connect(boost::bind(&LifecycleManager::onLaunchingDone, this, _1));
    m_webLifecycleHandler.EventLaunchingDone.connect(boost::bind(&LifecycleManager::onLaunchingDone, this, _1));
    m_qmlLifecycleHandler.EventLaunchingDone.connect(boost::bind(&LifecycleManager::onLaunchingDone, this, _1));

    // subscriber lsm's foreground info
    LSM::getInstance().EventForegroundInfoChanged.connect(boost::bind(&LifecycleManager::onForegroundInfoChanged, this, _1));
}

void LifecycleManager::launch(LifecycleTaskPtr task)
{
    LaunchAppItemPtr item = AppItemFactory::createLaunchItem(task);
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS(LOG_KEY_APPID, task->getAppId().c_str()),
                  PMLOGKS(LOG_KEY_REASON, "creating_item_fail"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        task->getLunaTask()->replyResultWithError(-101, "not exist");
        return;
    }

    // set start time
    item->setLaunchStartTime(getCurrentTime());

    // put new request into launching queue
    m_appLaunchingItemList.push_back(item);

    // start prelaunching
    runWithPrelauncher(item);
}

void LifecycleManager::pause(LifecycleTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getLunaTask()->getRequestPayload();
    const pbnjson::JValue& params = jmsg.hasKey("params") && jmsg["params"].isObject() ? jmsg["params"] : pbnjson::Object();

    std::string errorText;
    pauseApp(task->getAppId(), params, errorText);

    if (!errorText.empty()) {
        task->finalizeWithError(API_ERR_CODE_GENERAL, errorText);
        return;
    }
    task->finalize();
}

void LifecycleManager::close(LifecycleTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->getLunaTask()->getRequestPayload();

    std::string appId = jmsg["id"].asString();
    std::string callerId = task->getLunaTask()->caller();
    std::string errorText;
    bool preloadOnly = false;
    bool handlePauseAppSelf = false;
    std::string reason;

    LOG_INFO(MSGID_APPCLOSE, 2,
             PMLOGKS(LOG_KEY_APPID, appId.c_str()),
             PMLOGKS("caller", callerId.c_str()),
             "params: %s", jmsg.duplicate().stringify().c_str());

    if (jmsg.hasKey("preloadOnly"))
        preloadOnly = jmsg["preloadOnly"].asBool();
    if (jmsg.hasKey(LOG_KEY_REASON))
        reason = jmsg[LOG_KEY_REASON].asString();
    if (jmsg.hasKey("letAppHandle"))
        handlePauseAppSelf = jmsg["letAppHandle"].asBool();

    if (handlePauseAppSelf) {
        pauseApp(appId, pbnjson::Object(), errorText, false);
    } else {
        LifecycleManager::getInstance().closeByAppId(appId, callerId, reason, errorText, preloadOnly, false);
    }

    if (!errorText.empty()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1,
                  PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                  "err_text: %s", errorText.c_str());
        task->finalizeWithError(API_ERR_CODE_GENERAL, errorText);
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put(LOG_KEY_APPID, appId);
    task->finalize(payload);
}

void LifecycleManager::closeAll(LifecycleTaskPtr task)
{
    closeAllApps();
}

void LifecycleManager::onPrelaunchingDone(const std::string& uid)
{
    LaunchAppItemPtr item = getLaunchingItemByUid(uid);
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS(LOG_KEY_REASON, "null_pointer"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 4,
             PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
             PMLOGKS("uid", uid.c_str()),
             PMLOGKS("status", "prelaunching_done"),
             PMLOGKFV("launching_stage_in_detail", "%d", item->getSubStage()), "");

    if (AppLaunchingStage::PRELAUNCH != item->getStage()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 4,
                  PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS(LOG_KEY_REASON, "not_in_prelaunching_stage"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
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
                  PMLOGKS(LOG_KEY_REASON, "null_pointer"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
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
                  PMLOGKS(LOG_KEY_REASON, "null_pointer"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 4,
             PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
             PMLOGKS("uid", uid.c_str()),
             PMLOGKS("status", "memory_checking_done"),
             PMLOGKFV("launching_stage_in_detail", "%d", item->getSubStage()), "");

    if (AppLaunchingStage::MEMORY_CHECK != item->getStage()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 4,
                  PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                  PMLOGKS("uid", uid.c_str()),
                  PMLOGKS(LOG_KEY_REASON, "not_in_memory_checking_stage"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
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
                  PMLOGKS(LOG_KEY_REASON, "null_pointer"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
             PMLOGKS("uid", uid.c_str()),
             PMLOGKS("status", "launching_done"), "");

    finishLaunching(item);
}

void LifecycleManager::runWithPrelauncher(LaunchAppItemPtr item)
{
    LOG_INFO_WITH_CLOCK(MSGID_APPLAUNCH, 4,
                        PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                        PMLOGKS("status", "start_prelaunching"),
                        PMLOGKS("PerfType", "AppLaunch"),
                        PMLOGKS("PerfGroup", item->getAppId().c_str()), "");

    item->setStage(AppLaunchingStage::PRELAUNCH);
    m_prelauncher.addItem(item);
}

void LifecycleManager::runWithMemoryChecker(LaunchAppItemPtr item)
{
    LOG_INFO_WITH_CLOCK(MSGID_APPLAUNCH, 4,
                        PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
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
                        PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                        PMLOGKS("status", "start_launching"),
                        PMLOGKS("PerfType", "AppLaunch"),
                        PMLOGKS("PerfGroup", item->getAppId().c_str()), "");

    item->setStage(AppLaunchingStage::LAUNCH);
    launchApp(item);
}

void LifecycleManager::runLastappHandler()
{
    if (isFullscreenAppLoading("", "")) {
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 1,
                 PMLOGKS("status", "skip_launching_last_input_app"), "");
        return;
    }

    m_lastappHandler.launch();
}

void LifecycleManager::finishLaunching(LaunchAppItemPtr item)
{
    bool is_last_app_candidate = isLastLaunchingApp(item->getAppId()) && (m_lastLaunchingApps.size() == 1);

    bool redirect_to_lastapplaunch = (is_last_app_candidate) && !item->getErrorText().empty();

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
             PMLOGKS("status", "finish_launching"), "");

    EventLaunchingFinished(item);
    replyWithResult(item->lsmsg(), item->getPid(), item->getErrorText().empty(), item->getErrorCode(), item->getErrorText());
    removeLastLaunchingApp(item->getAppId());

    std::string app_uid = item->getUid();
    removeItem(app_uid);

    // TODO: decide if this is tv specific or not
    //       make tv handler if it's tv specific
    if (redirect_to_lastapplaunch) {
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 1,
                 PMLOGKS(LOG_KEY_ACTION, "trigger_launch_lastapp"), "");
        runLastappHandler();
    }
}

void LifecycleManager::onRuntimeStatusChanged(const std::string& appId, const std::string& uid, const RuntimeStatus& newStatus)
{
    if (appId.empty()) {
        LOG_ERROR(MSGID_LIFE_STATUS_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "empty_app_id"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }

    m_lifecycleRouter.setRuntimeStatus(appId, newStatus);
    LifeStatus newLifeStatus = m_lifecycleRouter.getLifeStatusFromRuntimeStatus(newStatus);

    setAppLifeStatus(appId, uid, newLifeStatus);
}

void LifecycleManager::setAppLifeStatus(const std::string& appId, const std::string& uid, LifeStatus newStatus)
{
    AppPackagePtr appPackagePtr = AppPackageManager::getInstance().getAppById(appId);
    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(appId);
    if (runningInfoPtr == nullptr) {
        runningInfoPtr = RunningInfoManager::getInstance().addRunningInfo(appId);
    }
    const LifecycleRoutePolicy& routePolicy = m_lifecycleRouter.getLifeCycleRoutePolicy(runningInfoPtr->getLifeStatus(), newStatus);
    LifeStatus nextStatus;
    RouteAction routeAction;
    RouteLog routeLog;
    std::tie(nextStatus, routeAction, routeLog) = routePolicy;

    LifeEvent life_event = m_lifecycleRouter.getLifeEventFromLifeStatus(nextStatus);

    // generate lifecycle event
    generateLifeCycleEvent(appId, uid, life_event);

    if (RouteLog::CHECK == routeLog) {
        LOG_INFO(MSGID_LIFE_STATUS, 3,
                 PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                 PMLOGKFV("current", "%d", (int)runningInfoPtr->getLifeStatus()),
                 PMLOGKFV("next", "%d", (int)nextStatus),
                 "just to check it");
    } else if (RouteLog::WARN == routeLog) {
        LOG_WARNING(MSGID_LIFE_STATUS, 3,
                    PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                    PMLOGKFV("current", "%d", (int)runningInfoPtr->getLifeStatus()),
                    PMLOGKFV("next", "%d", (int)nextStatus),
                    "handle exception");
    } else if (RouteLog::ERROR == routeLog) {
        LOG_ERROR(MSGID_LIFE_STATUS, 3,
                  PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                  PMLOGKFV("current", "%d", (int)runningInfoPtr->getLifeStatus()),
                  PMLOGKFV("next", "%d", (int)nextStatus),
                  "unexpected transition");
    }

    if (RouteAction::IGNORE == routeAction)
        return;

    switch (nextStatus) {
    case LifeStatus::LAUNCHING:
    case LifeStatus::RELAUNCHING:
        if (appPackagePtr)
            addLoadingApp(appId, appPackagePtr->getAppType());
        runningInfoPtr->setPreloadMode(false);
        break;

    case LifeStatus::PRELOADING:
        runningInfoPtr->setPreloadMode(true);
        break;

    case LifeStatus::STOP:
    case LifeStatus::FOREGROUND:
        runningInfoPtr->setPreloadMode(false);
        break;

    case LifeStatus::PAUSING:
        removeLoadingApp(appId);
        break;

    default:
        break;
    }

    LOG_INFO(MSGID_LIFE_STATUS, 4,
             PMLOGKS(LOG_KEY_APPID, appId.c_str()),
             PMLOGKS("uid", uid.c_str()),
             PMLOGKFV("current", "%d", (int)runningInfoPtr->getLifeStatus()),
             PMLOGKFV("new", "%d", (int)nextStatus),
             "life_status_changed");

    // set life status
    runningInfoPtr->setLifeStatus(nextStatus);
    signal_app_life_status_changed(appId, nextStatus);
    replySubscriptionForAppLifeStatus(appId, uid, nextStatus);
}

void LifecycleManager::onWAMStatusChanged(bool isConnected)
{
    LOG_INFO(MSGID_APPLAUNCH_ERR, 2,
             PMLOGKS("status", "remove_webapp_in_loading_list"),
             PMLOGKS(LOG_KEY_REASON, "WAM_disconnected"), "");

    std::vector<LoadingAppItem> loading_apps = m_loadingAppList;
    for (const auto& app : loading_apps) {
        if (std::get<1>(app) == AppType::AppType_Web)
            onRuntimeStatusChanged(std::get<0>(app), "", RuntimeStatus::STOP);
    }
}

void LifecycleManager::onRunningAppAdded(const std::string& appId, const std::string& pid, const std::string& webprocid)
{
    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(appId);
    if (runningInfoPtr == nullptr) {
        runningInfoPtr = RunningInfoManager::getInstance().addRunningInfo(appId);
    }
    runningInfoPtr->setPid(pid);
    runningInfoPtr->setWebprocid(webprocid);
    onRunningListChanged(appId);
}

void LifecycleManager::onRunningAppRemoved(const std::string& appId)
{
    RunningInfoManager::getInstance().removeRunningInfo(appId);
    onRunningListChanged(appId);

    if (isInAutomaticPendingList(appId))
        handleAutomaticApp(appId);
}

void LifecycleManager::onRunningListChanged(const std::string& appId)
{
    pbnjson::JValue runningList = pbnjson::Array();
    AppPackagePtr appPackagePtr = AppPackageManager::getInstance().getAppById(appId);
    bool isDevApp = false;
    if (appPackagePtr && AppTypeByDir::AppTypeByDir_Dev == appPackagePtr->getTypeByDir()) {
        isDevApp = true;
    }

    RunningInfoManager::getInstance().getRunningList(runningList, isDevApp);
    postRunning(runningList, isDevApp);
}

void LifecycleManager::replyWithResult(LSMessage* lsmsg, const std::string& pid, bool result, const int& errorCode, const std::string& errorText)
{
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", result);

    if (!result) {
        payload.put(LOG_KEY_ERRORCODE, errorCode);
        payload.put(LOG_KEY_ERRORTEXT, errorText);
    }

    if (lsmsg == NULL) {
        LOG_INFO(MSGID_APPLAUNCH, 3,
                 PMLOGKS(LOG_KEY_REASON, "null_lsmessage"),
                 PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                 PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS("status", "reply_launch_request"),
             PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()), "");

    LSErrorSafe lserror;
    if (!LSMessageRespond(lsmsg, payload.stringify().c_str(), &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS(LOG_KEY_TYPE, "lsrespond"),
                  PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "err: %s", lserror.message);
    }
}

void LifecycleManager::replySubscriptionForAppLifeStatus(const std::string& appId, const std::string& uid, const LifeStatus& lifeStatus)
{
    if (appId.empty()) {
        LOG_WARNING(MSGID_APP_LIFESTATUS_REPLY_ERROR, 1,
                    PMLOGKS(LOG_KEY_REASON, "null appId"), "");
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    if (payload.isNull()) {
        LOG_WARNING(MSGID_APP_LIFESTATUS_REPLY_ERROR, 1,
                    PMLOGKS(LOG_KEY_REASON, "make pbnjson failed"), "");
        return;
    }

    std::string strStatus = RunningInfoManager::toString(lifeStatus);
    payload.put("status", strStatus);    // change enum to string
    payload.put(LOG_KEY_APPID, appId);

    RunningInfoPtr ptr = RunningInfoManager::getInstance().getRunningInfo(appId);
    if (!ptr->getPid().empty())
        payload.put("processId", ptr->getPid());

    AppPackagePtr app_desc = AppPackageManager::getInstance().getAppById(appId);
    if (app_desc != NULL)
        payload.put(LOG_KEY_TYPE, AppPackage::toString(app_desc->getAppType()));

    LaunchAppItemPtr launchItem = uid.empty() ? NULL : getLaunchingItemByUid(uid);
    if (LifeStatus::LAUNCHING == lifeStatus || LifeStatus::RELAUNCHING == lifeStatus) {
        if (launchItem) {
            payload.put(LOG_KEY_REASON, launchItem->getReason());
        }
    } else if (LifeStatus::FOREGROUND == lifeStatus) {
        pbnjson::JValue fg_info = pbnjson::JValue();
        RunningInfoManager::getInstance().getForegroundInfoById(appId, fg_info);
        if (!fg_info.isNull() && fg_info.isObject()) {
            for (auto it : fg_info.children()) {
                const std::string key = it.first.asString();
                if ("windowType" == key || "windowGroup" == key || "windowGroupOwner" == key || "windowGroupOwnerId" == key) {
                    payload.put(key, fg_info[key]);
                }
            }
        }
    } else if (LifeStatus::BACKGROUND == lifeStatus) {
        if (ptr->getPreloadMode())
            payload.put("backgroundStatus", "preload");
        else
            payload.put("backgroundStatus", "normal");
    } else if (LifeStatus::STOP == lifeStatus || LifeStatus::CLOSING == lifeStatus) {
        payload.put(LOG_KEY_REASON, (m_closeReasonInfo.count(appId) == 0) ? "undefined" : m_closeReasonInfo[appId]);

        if (LifeStatus::STOP == lifeStatus)
            m_closeReasonInfo.erase(appId);
    }

    LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2,
             PMLOGKS("skey", "getAppLifeStatus"),
             PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()), "");

    LSErrorSafe lserror;
    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(),
                             "getAppLifeStatus",
                             payload.stringify().c_str(), &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS(LOG_KEY_TYPE, "subscriptionreply"),
                  PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "err: %s", lserror.message);
        return;
    }
}

void LifecycleManager::generateLifeCycleEvent(const std::string& appId, const std::string& uid, LifeEvent event)
{
    AppPackagePtr appDescPtr = AppPackageManager::getInstance().getAppById(appId);
    LaunchAppItemPtr launchItemPtr = uid.empty() ? NULL : getLaunchingItemByUid(uid);
    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(appId);

    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue info = pbnjson::JValue();
    payload.put(LOG_KEY_APPID, appId);

    switch (event) {
    case LifeEvent::SPLASH:
        // generate splash event only for fresh launch case
        if (launchItemPtr && !launchItemPtr->isShowSplash() && !launchItemPtr->isShowSpinner())
            return;
        if (LifeStatus::BACKGROUND == runningInfoPtr->getLifeStatus() && runningInfoPtr->getPreloadMode() == false)
            return;
        if (LifeStatus::STOP != runningInfoPtr->getLifeStatus() && LifeStatus::PRELOADING != runningInfoPtr->getLifeStatus() && LifeStatus::BACKGROUND != runningInfoPtr->getLifeStatus())
            return;

        payload.put("event", "splash");
        payload.put("title", (appDescPtr ? appDescPtr->getTitle() : ""));
        payload.put("showSplash", (launchItemPtr && launchItemPtr->isShowSplash()));
        payload.put("showSpinner", (launchItemPtr && launchItemPtr->isShowSpinner()));

        if (launchItemPtr && launchItemPtr->isShowSplash())
            payload.put("splashBackground", (appDescPtr ? appDescPtr->getSplashBackground() : ""));
        break;

    case LifeEvent::PRELOAD:
        payload.put("event", "preload");
        if (launchItemPtr)
            payload.put("preload", launchItemPtr->getPreload());
        break;

    case LifeEvent::LAUNCH:
        payload.put("event", "launch");
        payload.put(LOG_KEY_REASON, launchItemPtr->getReason());
        break;

    case LifeEvent::FOREGROUND:
        payload.put("event", "foreground");
        RunningInfoManager::getInstance().getForegroundInfoById(appId, info);
        if (!info.isNull() && info.isObject()) {
            for (auto it : info.children()) {
                const std::string key = it.first.asString();
                if ("windowType" == key ||
                    "windowGroup" == key ||
                    "windowGroupOwner" == key ||
                    "windowGroupOwnerId" == key) {
                    payload.put(key, info[key]);
                }
            }
        }
        break;

    case LifeEvent::BACKGROUND:
        payload.put("event", "background");
        if (runningInfoPtr->getPreloadMode())
            payload.put("status", "preload");
        else
            payload.put("status", "normal");
        break;

    case LifeEvent::PAUSE:
        payload.put("event", "pause");
        break;

    case LifeEvent::CLOSE:
        payload.put("event", "close");
        payload.put(LOG_KEY_REASON, (m_closeReasonInfo.count(appId) == 0) ? "undefined" : m_closeReasonInfo[appId]);
        break;

    case LifeEvent::STOP:
        payload.put("event", "stop");
        payload.put(LOG_KEY_REASON, (m_closeReasonInfo.count(appId) == 0) ? "undefined" : m_closeReasonInfo[appId]);
        break;

    default:
        return;
    }
    EventLifecycle(payload);
}

void LifecycleManager::postRunning(const pbnjson::JValue& running, bool devmode)
{
    pbnjson::JValue subscriptionPayload = pbnjson::Object();
    std::string kind = devmode ? SUBSKEY_DEV_RUNNING : SUBSKEY_RUNNING;

    subscriptionPayload.put("running", running);
    subscriptionPayload.put("returnValue", true);

    LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2,
             PMLOGKS("skey", kind.c_str()),
             PMLOGJSON(LOG_KEY_PAYLOAD, subscriptionPayload.stringify().c_str()), "");

    LSErrorSafe lserror;
    if (!LSSubscriptionReply(ApplicationManager::getInstance().get(),
                             kind.c_str(),
                             subscriptionPayload.stringify().c_str(), &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS(LOG_KEY_TYPE, "subscriptionreply"),
                  PMLOGJSON(LOG_KEY_PAYLOAD, subscriptionPayload.stringify().c_str()),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "err: %s", lserror.message);
        return;
    }
}

void LifecycleManager::handleBridgedLaunchRequest(const pbnjson::JValue& params)
{
    std::string item_uid;
    if (!params.hasKey(SYS_LAUNCHING_UID) || params[SYS_LAUNCHING_UID].asString(item_uid) != CONV_OK) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "uid_not_found"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }

    LaunchAppItemPtr launching_item = getLaunchingItemByUid(item_uid);
    if (launching_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("uid", item_uid.c_str()),
                  PMLOGKS(LOG_KEY_REASON, "launching_item_not_found"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }
    m_prelauncher.inputBridgedReturn(launching_item, params);
}

void LifecycleManager::registerApp(const std::string& appId, LSMessage* lsmsg, std::string& errText)
{
    AppPackagePtr appDesc = AppPackageManager::getInstance().getAppById(appId);
    if (!appDesc) {
        errText = "not existing app";
        return;
    }

    if (appDesc->getNativeInterfaceVersion() != 2) {
        errText = "trying to register via unmatched method with nativeLifeCycleInterfaceVersion";
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                  PMLOGKS(LOG_KEY_REASON, "wrong_version_interface_on_register"),
                  PMLOGKS("method", "registerNativeApp"),
                  "nativeLifeCycleInterfaceVersion: %d", appDesc->getNativeInterfaceVersion());
        return;
    }

    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(appId);
    if (RuntimeStatus::RUNNING != runningInfoPtr->getRuntimeStatus() &&
        RuntimeStatus::REGISTERED != runningInfoPtr->getRuntimeStatus()) {
        errText = "invalid status";
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                  PMLOGKS(LOG_KEY_REASON, "invalid_life_status_to_connect_nativeapp"),
                  PMLOGKFV("runtime_status", "%d", (int) runningInfoPtr->getRuntimeStatus()), "");
        return;
    }

    NativeAppLifeHandler::getInstance().registerApp(appId, lsmsg, errText);
}

void LifecycleManager::connectNativeApp(const std::string& appId, LSMessage* lsmsg, std::string& errorText)
{
    AppPackagePtr appPackagePtr = AppPackageManager::getInstance().getAppById(appId);
    if (!appPackagePtr) {
        errorText = "not existing app";
        return;
    }

    if (appPackagePtr->getNativeInterfaceVersion() != 1) {
        errorText = "trying to register via unmatched method with nativeLifeCycleInterfaceVersion";
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                  PMLOGKS(LOG_KEY_REASON, "wrong_version_interface_on_register"),
                  PMLOGKS("method", "registerNativeApp"),
                  "nativeLifeCycleInterfaceVersion: %d", appPackagePtr->getNativeInterfaceVersion());
        return;
    }

    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(appId);
    if (RuntimeStatus::RUNNING != runningInfoPtr->getRuntimeStatus() &&
        RuntimeStatus::REGISTERED != runningInfoPtr->getRuntimeStatus()) {
        errorText = "invalid_status";
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                  PMLOGKS(LOG_KEY_REASON, "invalid_life_status_to_connect_nativeapp"),
                  PMLOGKFV("runtime_status", "%d", (int) runningInfoPtr->getRuntimeStatus()), "");
        return;
    }

    NativeAppLifeHandler::getInstance().registerApp(appId, lsmsg, errorText);
}

void LifecycleManager::triggerToLaunchLastApp()
{
    std::string fg_app_id = RunningInfoManager::getInstance().getForegroundAppId();
    if (!fg_app_id.empty() && RunningInfoManager::getInstance().isRunning(fg_app_id))
        return;

    LOG_INFO(MSGID_LAUNCH_LASTAPP, 1,
             PMLOGKS("status", "trigger_to_launch_last_app"), "");

    runLastappHandler();
}

void LifecycleManager::closeByAppId(const std::string& appId, const std::string& callerId, const std::string& reason, std::string& errorText, bool preload_only, bool clearAllItems)
{
    if (preload_only && !hasOnlyPreloadedItems(appId)) {
        errorText = "app is being launched by user";
        return;
    }

    // keepAlive app policy
    // switch close request to pause request
    // except for below cases
    if (SettingsImpl::getInstance().isKeepAliveApp(appId) &&
        callerId != "com.webos.memorymanager" &&
        callerId != "com.webos.appInstallService" &&
        (callerId != "com.webos.surfacemanager.windowext" || reason != "recent") &&
        callerId != SAM_INTERNAL_ID) {
        pauseApp(appId, pbnjson::Object(), errorText);
        return;
    }

    closeApp(appId, callerId, reason, errorText, clearAllItems);
}

void LifecycleManager::closeAllLoadingApps()
{
    resetLastAppCandidates();

    m_prelauncher.cancelAll();
    m_memoryChecker.cancel_all();

    std::vector<std::string> automatic_pending_list;
    getAutomaticPendingAppIds(automatic_pending_list);

    for (auto& appId : automatic_pending_list)
        handleAutomaticApp(appId, false);

    std::vector<LoadingAppItem> loading_apps = m_loadingAppList;
    for (const auto& app : loading_apps) {
        std::string err_text = "";
        closeByAppId(std::get<0>(app), SAM_INTERNAL_ID, "", err_text, false, true);

        if (err_text.empty() == false) {
            LOG_WARNING(MSGID_APPCLOSE, 2,
                        PMLOGKS(LOG_KEY_APPID, (std::get<0>(app)).c_str()),
                        PMLOGKS("mode", "close_loading_app"),
                        "err: %s", err_text.c_str());
        } else {
            LOG_INFO(MSGID_APPCLOSE, 2,
                     PMLOGKS(LOG_KEY_APPID, (std::get<0>(app)).c_str()),
                     PMLOGKS("mode", "close_loading_app"),
                     "ok");
        }
    }
}

void LifecycleManager::closeAllApps(bool clearAllItems)
{
    LOG_INFO(MSGID_APPCLOSE, 2,
             PMLOGKS(LOG_KEY_ACTION, "close_all_apps"),
             PMLOGKS("status", "start"), "");
    std::vector<std::string> running_apps;
    RunningInfoManager::getInstance().getRunningAppIds(running_apps);
    closeApps(running_apps, clearAllItems);
    resetLastAppCandidates();
}

void LifecycleManager::closeApps(const std::vector<std::string>& appIds, bool clearAllItems)
{
    if (appIds.size() < 1) {
        LOG_INFO(MSGID_APPCLOSE, 2,
                 PMLOGKS(LOG_KEY_ACTION, "close_all_apps"),
                 PMLOGKS("status", "no_apps_to_close"), "");
        return;
    }

    std::string fullscreenAppId;
    bool performedClosingApps = false;
    for (auto& appId : appIds) {
        // kill app on fullscreen later
        if (RunningInfoManager::getInstance().isAppOnFullscreen(appId)) {
            fullscreenAppId = appId;
            continue;
        }

        std::string errorText = "";
        closeByAppId(appId, SAM_INTERNAL_ID, "", errorText, false, clearAllItems);
        performedClosingApps = true;
    }

    // now kill app on fullscreen
    if (!fullscreenAppId.empty()) {
        // we don't guarantee all apps running in background are closed before closing app running on foreground
        // we just raise possibility by adding sleep time
        if (performedClosingApps)
            usleep(SLEEP_TIME_TO_CLOSE_FULLSCREEN_APP);
        std::string err_text = "";
        closeByAppId(fullscreenAppId, SAM_INTERNAL_ID, "", err_text, false, clearAllItems);
    }
}

void LifecycleManager::launchApp(LaunchAppItemPtr item)
{
    IAppLifeHandler* life_handler = getLifeHandlerForApp(item->getAppId());
    if (nullptr == life_handler) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                  PMLOGKS(LOG_KEY_REASON, "null_description"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        return;
    }

    life_handler->launch(item);
}

void LifecycleManager::closeApp(const std::string& appId, const std::string& callerId, const std::string& reason, std::string& errorText, bool clearAllItems)
{
    if (clearAllItems) {
        clearLaunchingAndLoadingItemsByAppId(appId);
    }

    IAppLifeHandler* handler = getLifeHandlerForApp(appId);
    if (nullptr == handler) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1,
                  PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                  "no valid life handler");
        errorText = "no valid life handler";
        return;
    }

    std::string closeReason = SettingsImpl::getInstance().getCloseReason(callerId, reason);

    if (RunningInfoManager::getInstance().isRunning(appId) && m_closeReasonInfo.count(appId) == 0) {
        m_closeReasonInfo[appId] = closeReason;
    }

    RunningInfoPtr ptr = RunningInfoManager::getInstance().getRunningInfo(appId);
    CloseAppItemPtr item = std::make_shared<CloseAppItem>(appId, ptr->getPid(), callerId, closeReason);
    if (nullptr == item) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1,
                  PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                  "memory alloc fail");
        errorText = "memory alloc fail";
        return;
    }

    LOG_INFO(MSGID_APPCLOSE, 2,
             PMLOGKS(LOG_KEY_APPID, appId.c_str()),
             PMLOGKS("pid", ptr->getPid().c_str()), "");

    handler->close(item, errorText);
}

void LifecycleManager::pauseApp(const std::string& appId, const pbnjson::JValue& params, std::string& errorText, bool reportEvent)
{
    if (!RunningInfoManager::getInstance().isRunning(appId)) {
        errorText = "app is not running";
        return;
    }

    IAppLifeHandler* handler = getLifeHandlerForApp(appId);
    if (nullptr == handler) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1,
                  PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                  "no valid life handler");
        errorText = "no valid life handler";
        return;
    }

    handler->pause(appId, params, errorText, reportEvent);
}

bool LifecycleManager::isFullscreenWindowType(const pbnjson::JValue& foregroundInfo)
{
    bool windowGroup = foregroundInfo["windowGroup"].asBool();
    bool windowGroupOwner = (windowGroup == false ? true : foregroundInfo["windowGroupOwner"].asBool());
    std::string windowType = foregroundInfo["windowType"].asString();

    for (auto& type : SettingsImpl::getInstance().m_fullscreenWindowTypes) {
        if (windowType == type && windowGroupOwner) {
            return true;
        }
    }
    return false;
}

void LifecycleManager::onForegroundInfoChanged(const pbnjson::JValue& subscriptionPayload)
{
    pbnjson::JValue rawForegroundAppInfo;
    if (!JValueUtil::getValue(subscriptionPayload, "foregroundAppInfo", rawForegroundAppInfo)) {
        LOG_ERROR(MSGID_GET_FOREGROUND_APP_ERR, 1,
                  PMLOGJSON(LOG_KEY_PAYLOAD, subscriptionPayload.duplicate().stringify().c_str()),
                  "invalid message from LSM");
        return;
    }
    LOG_INFO(MSGID_GET_FOREGROUND_APPINFO, 1,
             PMLOGJSON("ForegroundApps", rawForegroundAppInfo.stringify().c_str()), "");

    std::string oldForegroundAppId = RunningInfoManager::getInstance().getForegroundAppId();
    std::string newForegroundAppId = "";
    std::vector<std::string> oldForegroundApps = RunningInfoManager::getInstance().getForegroundAppIds();
    std::vector<std::string> newForegroundApps;
    pbnjson::JValue oldForegroundInfo = RunningInfoManager::getInstance().getForegroundInfo();
    pbnjson::JValue newForegroundInfo = pbnjson::Array();

    bool foundFullscreenWindow = false;
    for (int i = 0; i < rawForegroundAppInfo.arraySize(); ++i) {
        std::string appId;
        if (!JValueUtil::getValue(rawForegroundAppInfo[i], LOG_KEY_APPID, appId) || appId.empty()) {
            continue;
        }

        RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(appId);
        if (runningInfoPtr == nullptr) {
            runningInfoPtr = RunningInfoManager::getInstance().addRunningInfo(appId);
        }

        LOG_INFO_WITH_CLOCK(MSGID_GET_FOREGROUND_APPINFO, 2,
                            PMLOGKS("PerfType", "AppLaunch"),
                            PMLOGKS("PerfGroup", appId.c_str()), "");

        newForegroundInfo.append(rawForegroundAppInfo[i].duplicate());
        newForegroundApps.push_back(appId);

        if (isFullscreenWindowType(rawForegroundAppInfo[i])) {
            foundFullscreenWindow = true;
            newForegroundAppId = appId;
        }
    }

    LOG_INFO(MSGID_GET_FOREGROUND_APPINFO, 1,
             PMLOGJSON("FilteredForegroundApps", newForegroundInfo.duplicate().stringify().c_str()), "");

    // update foreground info into app info manager
    if (foundFullscreenWindow) {
        resetLastAppCandidates();
    }
    RunningInfoManager::getInstance().setForegroundApp(newForegroundAppId);
    RunningInfoManager::getInstance().setForegroundAppIds(newForegroundApps);
    RunningInfoManager::getInstance().setForegroundInfo(newForegroundInfo);

    LOG_INFO(MSGID_FOREGROUND_INFO, 2,
             PMLOGKS("current_foreground_app", newForegroundAppId.c_str()),
             PMLOGKS("prev_foreground_app", oldForegroundAppId.c_str()), "");

    // set background
    for (auto& oldAppId : oldForegroundApps) {
        bool found = false;
        for (auto& newAppId : newForegroundApps) {
            if ((oldAppId) == (newAppId)) {
                found = true;
                break;
            }
        }

        if (found == false) {
            RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(oldAppId);
            switch (runningInfoPtr->getLifeStatus()) {
            case LifeStatus::FOREGROUND:
            case LifeStatus::PAUSING:
                setAppLifeStatus(oldAppId, "", LifeStatus::BACKGROUND);
                break;

            default:
                break;
            }
        }
    }

    // set foreground
    for (auto& newAppId : newForegroundApps) {
        setAppLifeStatus(newAppId, "", LifeStatus::FOREGROUND);

        if (!RunningInfoManager::getInstance().isRunning(newAppId)) {
            LOG_INFO(MSGID_FOREGROUND_INFO, 1,
                     PMLOGKS(LOG_KEY_APPID, newAppId.c_str()),
                     "no running info, but received foreground info");
        }
    }

    // this is TV specific scenario related to TV POWER (instant booting)
    // improve this tv dependent structure later
    if (subscriptionPayload.hasKey(LOG_KEY_REASON) && subscriptionPayload[LOG_KEY_REASON].asString() == "forceMinimize") {
        LOG_INFO(MSGID_FOREGROUND_INFO, 1,
                 PMLOGKS(LOG_KEY_REASON, "forceMinimize"),
                 "no trigger last input handler");
        resetLastAppCandidates();
    }
    // run last input handler
    else if (foundFullscreenWindow == false) {
        runLastappHandler();
    }

    // signal foreground app changed
    if (oldForegroundAppId != newForegroundAppId) {
        EventForegroundAppChanged(newForegroundAppId);
    }

    // reply subscription foreground with extraInfo
    if (oldForegroundInfo != newForegroundInfo) {
        EventForegroundExtraInfoChanged(newForegroundInfo);
    }
}

bool LifecycleManager::isFullscreenAppLoading(const std::string& newAppId, const std::string& newAppUid)
{
    bool result = false;

    for (auto& item : m_appLaunchingItemList) {
        if (newAppId == item->getAppId() && item->getUid() == newAppUid)
            continue;

        AppPackagePtr app_desc = AppPackageManager::getInstance().getAppById(item->getAppId());
        if (app_desc == NULL) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                     PMLOGKS("status", "not_candidate_checking_launching"),
                     "null description");
            continue;
        }

        if (("com.webos.app.container" == item->getAppId()) ||
            ("com.webos.app.inputcommon" == item->getAppId()))
            continue;

        if (app_desc->isChildWindow()) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                     PMLOGKS("status", "not_candidate_checking_launching"),
                     "child window type");
            continue;
        }
        if (!(item->getPreload().empty())) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                     PMLOGKS("status", "not_candidate_checking_launching"),
                     "preloading app");
            continue;
        }

        if (app_desc->getDefaultWindowType() == "card" || app_desc->getDefaultWindowType() == "minimal") {
            if (isLaunchingItemExpired(item)) {
                LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
                         PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                         PMLOGKS("status", "not_candidate_checking_launching"),
                         PMLOGKFV("launching_stage_in_detail", "%d", item->getSubStage()),
                         "fullscreen app, but expired");
                continue;
            }

            LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
                     PMLOGKS("status", "autolaunch_condition_check"),
                     PMLOGKS("launching_app_id", item->getAppId().c_str()),
                     PMLOGKFV("launching_stage_in_detail", "%d", item->getSubStage()),
                     "fullscreen app is already launching");

            addLastLaunchingApp(item->getAppId());
            result = true;
        } else {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                    PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                    PMLOGKS("status", "not_candidate_checking_launching"),
                    "not fullscreen type: %s", app_desc->getDefaultWindowType().c_str());
        }
    }

    std::vector<std::string> app_ids;
    getLoadingAppIds(app_ids);

    for (auto& appId : app_ids) {
        AppPackagePtr appPackagePtr = AppPackageManager::getInstance().getAppById(appId);
        RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(appId);
        if (appPackagePtr == NULL) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                     PMLOGKS("status", "not_candidate_checking_loading"),
                     "null description");
            continue;
        }
        if (appPackagePtr->isChildWindow()) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                     PMLOGKS("status", "not_candidate_checking_loading"),
                     "child window type");
            continue;
        }
        if (runningInfoPtr->getPreloadMode()) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                     PMLOGKS("status", "not_candidate_checking_loading"),
                     "preloading app");
            continue;
        }

        if (appPackagePtr->getDefaultWindowType() == "card" || appPackagePtr->getDefaultWindowType() == "minimal") {
            if (isLoadingAppExpired(appId)) {
                LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                         PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                         PMLOGKS("status", "not_candidate_checking_loading"),
                         "fullscreen app, but expired");
                continue;
            }

            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS("status", "autolaunch_condition_check"),
                     PMLOGKS("loading_app_id", appId.c_str()),
                     "fullscreen app is already loading");

            setLastLoadingApp(appId);
            result = true;
            break;
        } else {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2,
                     PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                     PMLOGKS("status", "not_candidate_checking_loading"),
                     "not fullscreen type: %s", appPackagePtr->getDefaultWindowType().c_str());
        }
    }

    return result;
}

void LifecycleManager::clearLaunchingAndLoadingItemsByAppId(const std::string& appId)
{
    bool found = false;

    while (true) {
        auto it = std::find_if(m_appLaunchingItemList.begin(), m_appLaunchingItemList.end(), [&appId](LaunchAppItemPtr item) {
            return (item->getAppId() == appId);
        });

        if (it == m_appLaunchingItemList.end())
            break;
        std::string errorText = "stopped launching";
        (*it)->setErrCodeText(APP_LAUNCH_ERR_GENERAL, errorText);
        finishLaunching(*it);
        found = true;
    }

    for (auto& loading_app : m_loadingAppList) {
        if (std::get<0>(loading_app) == appId) {
            found = true;
            break;
        }
    }

    if (found) {
        setAppLifeStatus(appId, "", LifeStatus::STOP);
    }
}

void LifecycleManager::handleAutomaticApp(const std::string& appId, bool continueToLaunch)
{
    LaunchAppItemPtr item = getLaunchingItemByAppId(appId);
    if (item == NULL) {
        LOG_ERROR(MSGID_LAUNCH_LASTAPP_ERR, 3,
                  PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                  PMLOGKS(LOG_KEY_REASON, "launching_item_not_found"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        removeItemFromAutomaticPendingList(appId);

        return;
    }

    removeItemFromAutomaticPendingList(appId);

    if (continueToLaunch) {
        runWithPrelauncher(item);
    } else {
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
                 PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                 PMLOGKS("status", "cancel_to_launch_last_app"),
                 PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        finishLaunching(item);
        return;
    }
}

void LifecycleManager::getLaunchingAppIds(std::vector<std::string>& appIds)
{
    for (auto& launching_item : m_appLaunchingItemList)
        appIds.push_back(launching_item->getAppId());
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
    auto it = std::find_if(m_appLaunchingItemList.begin(), m_appLaunchingItemList.end(), [&uid](LaunchAppItemPtr item) {return (item->getUid() == uid);});
    if (it == m_appLaunchingItemList.end())
        return;
    m_appLaunchingItemList.erase(it);
}

void LifecycleManager::addItemIntoAutomaticPendingList(LaunchAppItemPtr item)
{
    m_automaticPendingList.push_back(item);
    LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
             PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
             PMLOGKS("mode", "pending_automatic_app"),
             PMLOGKS("status", "added_into_list"), "");
}

void LifecycleManager::removeItemFromAutomaticPendingList(const std::string& appId)
{
    auto it = std::find_if(m_automaticPendingList.begin(), m_automaticPendingList.end(), [&appId](LaunchAppItemPtr item) {
        return (item->getAppId() == appId);
    });
    if (it != m_automaticPendingList.end()) {
        m_automaticPendingList.erase(it);
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 3,
                 PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                 PMLOGKS("mode", "pending_automatic_app"),
                 PMLOGKS("status", "removed_from_list"), "");
    }
}

bool LifecycleManager::isInAutomaticPendingList(const std::string& appId)
{
    auto it = std::find_if(m_automaticPendingList.begin(), m_automaticPendingList.end(), [&appId](LaunchAppItemPtr item) {
        return (item->getAppId() == appId);
    });

    if (it != m_automaticPendingList.end())
        return true;
    else
        return false;
}

void LifecycleManager::getAutomaticPendingAppIds(std::vector<std::string>& appIds)
{
    for (auto& launching_item : m_automaticPendingList)
        appIds.push_back(launching_item->getAppId());
}

void LifecycleManager::getLoadingAppIds(std::vector<std::string>& appIds)
{
    for (auto& app : m_loadingAppList)
        appIds.push_back(std::get<0>(app));
}

void LifecycleManager::addLoadingApp(const std::string& appId, const AppType& type)
{
    for (auto& loading_app : m_loadingAppList)
        if (std::get<0>(loading_app) == appId)
            return;
    // exception
    if ("com.webos.app.container" == appId || "com.webos.app.inputcommon" == appId) {
        LOG_INFO(MSGID_LOADING_LIST, 1,
                 PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                 "apply exception (skip)");
        return;
    }

    LoadingAppItem newLoadingApp = std::make_tuple(appId, type, static_cast<double>(getCurrentTime()));
    m_loadingAppList.push_back(newLoadingApp);

    LOG_INFO(MSGID_LOADING_LIST, 1,
             PMLOGKS(LOG_KEY_APPID, appId.c_str()),
             "added");

    if (isLastLaunchingApp(appId))
        setLastLoadingApp(appId);
}

void LifecycleManager::removeLoadingApp(const std::string& appId)
{
    auto it = std::find_if(m_loadingAppList.begin(), m_loadingAppList.end(), [&appId](const LoadingAppItem& loading_app) {
        return (std::get<0>(loading_app) == appId);
    });
    if (it == m_loadingAppList.end())
        return;
    m_loadingAppList.erase(it);
    LOG_INFO(MSGID_LOADING_LIST, 1,
             PMLOGKS(LOG_KEY_APPID, appId.c_str()),
             "removed");

    if (m_lastLoadingAppTimerSet.second == appId)
        removeTimerForLastLoadingApp(true);
}

void LifecycleManager::setLastLoadingApp(const std::string& appId)
{
    auto it = std::find_if(m_loadingAppList.begin(), m_loadingAppList.end(), [&appId](const LoadingAppItem& loading_app) {
        return (std::get<0>(loading_app) == appId);
    });
    if (it == m_loadingAppList.end())
        return;

    addTimerForLastLoadingApp(appId);
    removeLastLaunchingApp(appId);
}

void LifecycleManager::addTimerForLastLoadingApp(const std::string& appId)
{
    if ((m_lastLoadingAppTimerSet.first != 0) && (m_lastLoadingAppTimerSet.second == appId))
        return;

    auto it = std::find_if(m_loadingAppList.begin(), m_loadingAppList.end(),
                          [&appId](const LoadingAppItem& loading_app) {
        return (std::get<0>(loading_app) == appId);
    });
    if (it == m_loadingAppList.end())
        return;

    AppPackagePtr app_desc = AppPackageManager::getInstance().getAppById(appId);
    if (app_desc == NULL) {
        LOG_ERROR(MSGID_LAUNCH_LASTAPP_ERR, 3,
                  PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                  PMLOGKS("status", "cannot_add_timer_for_last_loading_app"),
                  PMLOGKS(LOG_KEY_REASON, "null description"), "");
        return;
    }

    removeTimerForLastLoadingApp(false);

    guint timer = g_timeout_add(SettingsImpl::getInstance().getLastLoadingAppTimeout(), runLastLoadingAppTimeoutHandler, NULL);
    m_lastLoadingAppTimerSet = std::make_pair(timer, appId);
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

gboolean LifecycleManager::runLastLoadingAppTimeoutHandler(gpointer userData)
{
    LifecycleManager::getInstance().removeTimerForLastLoadingApp(true);
    return FALSE;
}

void LifecycleManager::addLastLaunchingApp(const std::string& appId)
{
    auto it = std::find(m_lastLaunchingApps.begin(), m_lastLaunchingApps.end(), appId);
    if (it != m_lastLaunchingApps.end())
        return;

    m_lastLaunchingApps.push_back(appId);
}

void LifecycleManager::removeLastLaunchingApp(const std::string& appId)
{
    auto it = std::find(m_lastLaunchingApps.begin(), m_lastLaunchingApps.end(), appId);
    if (it == m_lastLaunchingApps.end())
        return;

    m_lastLaunchingApps.erase(it);
}

bool LifecycleManager::isLastLaunchingApp(const std::string& appId)
{
    auto it = std::find(m_lastLaunchingApps.begin(), m_lastLaunchingApps.end(), appId);
    return {(it != m_lastLaunchingApps.end()) ? true : false};
}

void LifecycleManager::resetLastAppCandidates()
{
    m_lastLaunchingApps.clear();
    removeTimerForLastLoadingApp(false);
}

bool LifecycleManager::hasOnlyPreloadedItems(const std::string& appId)
{
    for (auto& item : m_appLaunchingItemList) {
        if (item->getAppId() == appId && item->getPreload().empty())
            return false;
    }

    for (auto& item : m_loadingAppList) {
        if (std::get<0>(item) == appId)
            return false;
    }

    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(appId);
    if (RunningInfoManager::getInstance().isRunning(appId) && !runningInfoPtr->getPreloadMode()) {
        return false;
    }

    return true;
}

bool LifecycleManager::isLaunchingItemExpired(LaunchAppItemPtr item)
{
    double current_time = getCurrentTime();
    double elapsed_time = current_time - item->launchStartTime();

    if (elapsed_time > SettingsImpl::getInstance().getLaunchExpiredTimeout())
        return true;

    return false;
}

bool LifecycleManager::isLoadingAppExpired(const std::string& appId)
{
    double loading_start_time = 0;
    for (auto& loading_app : m_loadingAppList) {
        if (std::get<0>(loading_app) == appId) {
            loading_start_time = std::get<2>(loading_app);
            break;
        }
    }

    if (loading_start_time == 0) {
        LOG_WARNING(MSGID_LAUNCH_LASTAPP, 2,
                    PMLOGKS("status", "invalid_loading_start_time"),
                    PMLOGKS(LOG_KEY_APPID, appId.c_str()), "");
        return true;
    }

    double current_time = getCurrentTime();
    double elapsed_time = current_time - loading_start_time;

    if (elapsed_time > SettingsImpl::getInstance().getLoadingExpiredTimeout())
        return true;

    return false;
}

IAppLifeHandler* LifecycleManager::getLifeHandlerForApp(const std::string& appId)
{
    AppPackagePtr app_desc = AppPackageManager::getInstance().getAppById(appId);
    if (app_desc == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                  PMLOGKS(LOG_KEY_REASON, "null_description"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
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
