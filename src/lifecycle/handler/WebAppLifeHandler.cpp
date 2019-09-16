// Copyright (c) 2012-2019 LG Electronics, Inc.
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
#include "bus/client/WAM.h"
#include <lifecycle/handler/WebAppLifeHandler.h>
#include <lifecycle/RunningInfoManager.h>
#include <package/AppPackageManager.h>
#include <util/Time.h>
#include <util/LSUtils.h>

const std::string INVALID_PROCESS_ID = "-1";
const std::string PROCESS_ID_ZERO = "0";

WebAppLifeHandler::WebAppLifeHandler()
    : m_wamSubscriptionToken(0),
      m_runningList(pbnjson::Array())
{
    WAM::getInstance().EventListRunningAppsChanged.connect(boost::bind(&WebAppLifeHandler::onListRunningAppsChanged, this, _1));
}

WebAppLifeHandler::~WebAppLifeHandler()
{
}

void WebAppLifeHandler::launch(LaunchAppItemPtr item)
{
    AppPackagePtr app_desc = AppPackageManager::getInstance().getAppById(item->getAppId());
    if (app_desc == NULL) {
        Logger::error(getClassName(), __FUNCTION__, item->getAppId(), "null_description");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        EventLaunchingDone(item->getUid());
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("appDesc", app_desc->toJValue());
    payload.put(Logger::LOG_KEY_REASON, item->getReason());
    payload.put("parameters", item->getParams());
    payload.put("launchingAppId", item->getCallerId());
    payload.put("launchingProcId", item->getCallerPid());
    payload.put("keepAlive", item->isKeepAlive());

    if (!(item->getPreload().empty()))
        payload.put("preload", item->getPreload());

    LSMessageToken token = 0;
    LSErrorSafe lserror;
    if (!LSCallOneReply(ApplicationManager::getInstance().get(),
                        "luna://com.palm.webappmanager/launchApp",
                        payload.stringify().c_str(),
                        onReturnForLaunchRequest,
                        this,
                        &token,
                        &lserror)) {
        Logger::error(getClassName(), __FUNCTION__, "lscallonereply", lserror.message);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        EventLaunchingDone(item->getUid());
        return;
    }

    addLoadingApp(item->getAppId());

    if (item->getPreload().empty())
        EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::LAUNCHING);
    else
        EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::PRELOADING);

    double current_time = Time::getCurrentTime();
    double elapsed_time = current_time - item->launchStartTime();
    Logger::info(getClassName(), __FUNCTION__, item->getAppId(), Logger::format("elapsed_time(%f)", elapsed_time));
    item->setReturnToken(token);
    m_lscallRequestList.push_back(item);
}

bool WebAppLifeHandler::onReturnForLaunchRequest(LSHandle* sh, LSMessage* message, void* context)
{
    LSMessageToken token = 0;
    std::string uid = "";
    LaunchAppItemPtr item = NULL;

    token = LSMessageGetResponseToken(message);
    item = WebAppLifeHandler::getInstance().getLSCallRequestItemByToken(token);

    if (item == NULL) {
        Logger::error(WebAppLifeHandler::getInstance().getClassName(), __FUNCTION__, "null_launching_item");
        return false;
    }

    uid = item->getUid();
    WebAppLifeHandler::getInstance().removeItemFromLSCallRequestList(uid);
    item->resetReturnToken();

    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull()) {
        Logger::error(WebAppLifeHandler::getInstance().getClassName(), __FUNCTION__, "parsing_fail");

        // How could we get the previous status? We should restore it.
        // WebAppLifeHandler::getInstance().signal_app_life_status_changed(item->appId(), "", RuntimeStatus::STOP);
        return false;
    }

    std::string appId = responsePayload.hasKey(Logger::LOG_KEY_APPID) && responsePayload[Logger::LOG_KEY_APPID].isString() ? responsePayload[Logger::LOG_KEY_APPID].asString() : item->getAppId();
    std::string proc_id = responsePayload.hasKey("procId") && responsePayload["procId"].isString() ? responsePayload["procId"].asString() : "";

    // TODO: WAM doesn't return "returnValue" on success
    //       report this issue to WAM
    //       SAM should handled after WAM's work
    //       This "returnValue" is from ls-hubd when service is disconnected.
    if (responsePayload.hasKey("returnValue") && responsePayload["returnValue"].asBool() == false) {
        Logger::error(WebAppLifeHandler::getInstance().getClassName(), __FUNCTION__, appId, responsePayload.stringify());

        if (appId.empty()) {
            Logger::error(WebAppLifeHandler::getInstance().getClassName(), __FUNCTION__, appId, "cannot_handle_life_status");
        }

        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "WebAppMgr's launchApp is failed");
        WebAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());

        WebAppLifeHandler::getInstance().EventAppLifeStatusChanged(appId, "", RuntimeStatus::STOP);
        return true;
    }

    Logger::info(WebAppLifeHandler::getInstance().getClassName(), __FUNCTION__, appId, "received_launch_return_from_wam");
    item->setPid(proc_id);
    WebAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());

    return true;
}

void WebAppLifeHandler::close(CloseAppItemPtr item, std::string& errorText)
{
    bool need_to_handle_fake_stop = false;
    if (!RunningInfoManager::getInstance().isRunning(item->getAppId())) {
        if (isLoading(item->getAppId())) {
            need_to_handle_fake_stop = true;
        } else {
            errorText = "app is not running";
            Logger::info(getClassName(), __FUNCTION__, item->getAppId(), errorText);
            return;
        }
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put(Logger::LOG_KEY_APPID, item->getAppId());
    payload.put(Logger::LOG_KEY_REASON, item->getReason());

    LSErrorSafe lserror;
    if (!LSCallOneReply(ApplicationManager::getInstance().get(),
                        "luna://com.palm.webappmanager/killApp",
                        payload.stringify().c_str(),
                        onReturnForCloseRequest, this, NULL, &lserror)) {
        Logger::info(getClassName(), __FUNCTION__, "lscallonereply", lserror.message);
        errorText = "kill request fail";
        return;
    }

    EventAppLifeStatusChanged(item->getAppId(), "", RuntimeStatus::CLOSING);
    if (need_to_handle_fake_stop) {
        removeLoadingApp(item->getAppId());
        Logger::info(getClassName(), __FUNCTION__, item->getAppId(), "running is not updated yet. set stop status manually");
        EventAppLifeStatusChanged(item->getAppId(), "", RuntimeStatus::STOP);
    }
}

bool WebAppLifeHandler::onReturnForCloseRequest(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull()) {
        Logger::error(WebAppLifeHandler::getInstance().getClassName(), __FUNCTION__, "parsing_fail");
        return false;
    }

    if (responsePayload["returnValue"].asBool()) {
        std::string appId = responsePayload[Logger::LOG_KEY_APPID].asString();
        std::string pid = responsePayload["processId"].asString();
        Logger::info(WebAppLifeHandler::getInstance().getClassName(), __FUNCTION__, appId, "received_close_return_from_wam");
    } else {
        std::string errorText = responsePayload[Logger::LOG_KEY_ERRORTEXT].asString();
        Logger::error(WebAppLifeHandler::getInstance().getClassName(), __FUNCTION__, errorText);
        // How could we get the previous status? We should restore it.
        // WebAppLifeHandler::getInstance().signal_app_life_status_changed(appId(), "", RuntimeStatus::STOP);

    }

    return true;
}

void WebAppLifeHandler::pause(const std::string& appId, const pbnjson::JValue& params, std::string& errorText, bool send_life_event)
{

    pbnjson::JValue payload = pbnjson::Object();
    payload.put(Logger::LOG_KEY_APPID, appId.c_str());
    payload.put(Logger::LOG_KEY_REASON, "pause");
    payload.put("parameters", params);

    LSErrorSafe lserror;
    if (!LSCallOneReply(ApplicationManager::getInstance().get(),
                        "luna://com.palm.webappmanager/pauseApp",
                        payload.stringify().c_str(),
                        onReturnForPauseRequest,
                        this,
                        NULL,
                        &lserror)) {
        errorText = "pause request fail";
        Logger::error(getClassName(), __FUNCTION__, lserror.message);
        return;
    }

    if (send_life_event)
        EventAppLifeStatusChanged(appId, "", RuntimeStatus::PAUSING);
}

bool WebAppLifeHandler::onReturnForPauseRequest(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull()) {
        Logger::error(WebAppLifeHandler::getInstance().getClassName(), __FUNCTION__, "parsing_fail");
        return false;
    }

    if (responsePayload["returnValue"].asBool()) {
        std::string appId = responsePayload[Logger::LOG_KEY_APPID].asString();
        Logger::info(WebAppLifeHandler::getInstance().getClassName(), __FUNCTION__, appId, "received_pause_return_from_wam");
    } else {
        std::string errorText = responsePayload[Logger::LOG_KEY_ERRORTEXT].asString();
        Logger::info(WebAppLifeHandler::getInstance().getClassName(), __FUNCTION__, errorText);
    }

    return true;
}

void WebAppLifeHandler::onListRunningAppsChanged(const pbnjson::JValue& newList)
{
    int new_list_len = newList.arraySize();
    int old_list_len = m_runningList.arraySize();

    for (int i = 0; i < old_list_len; ++i) {
        std::string old_appId = m_runningList[i]["id"].asString();
        std::string old_webprocid = m_runningList[i]["webprocessid"].asString();

        bool is_removed = true;
        bool is_changed = false;
        for (int j = 0; j < new_list_len; ++j) {
            if (newList[j]["id"].asString() == old_appId) {
                if (newList[j]["webprocessid"].asString() == old_webprocid)
                    is_removed = false;
                else
                    is_changed = true;

                break;
            }
        }

        if (is_removed) {
            Logger::info(getClassName(), __FUNCTION__, old_appId, "removed");

            if (!is_changed)
                EventAppLifeStatusChanged(old_appId, "", RuntimeStatus::STOP);

            if ((old_webprocid != INVALID_PROCESS_ID) && (old_webprocid != PROCESS_ID_ZERO))
                EventRunningAppRemoved(old_appId);
        }
    }

    for (int i = 0; i < new_list_len; ++i) {
        std::string new_appId = newList[i]["id"].asString();
        std::string new_pid = newList[i]["processid"].asString();
        std::string new_webprocid = newList[i]["webprocessid"].asString();

        Logger::debug(getClassName(), __FUNCTION__, Logger::format("WAM_RUNNING item (%s:%s:%s)", new_appId.c_str(), new_pid.c_str(), new_webprocid.c_str()));

        bool is_added = true;
        bool is_changed = false;

        for (int j = 0; j < old_list_len; ++j) {
            if (m_runningList[j]["id"].asString() == new_appId) {
                if (m_runningList[j]["webprocessid"].asString() == new_webprocid)
                    is_added = false;
                else
                    is_changed = true;

                break;
            }
        }

        // protect to invalid process id "-1" from WAM
        if ((new_webprocid == INVALID_PROCESS_ID) || (new_webprocid == PROCESS_ID_ZERO)) {
            Logger::info(getClassName(), __FUNCTION__, new_appId, Logger::format("status(invalid_pid_added) new_pid(%d) new_webprocid(%d) is_changed(%B)", new_pid.c_str(), new_webprocid.c_str(), is_changed));
            continue;
        }

        if (is_added) {
            Logger::info(getClassName(), __FUNCTION__, new_appId, Logger::format("status(added) new_pid(%d) new_webprocid(%d) is_changed(%B)", new_pid.c_str(), new_webprocid.c_str(), is_changed));
            EventRunningAppAdded(new_appId, new_webprocid, new_webprocid);
            removeLoadingApp(new_appId);

            if (!is_changed)
                EventAppLifeStatusChanged(new_appId, "", RuntimeStatus::RUNNING);
        }
    }

    m_runningList = newList.arraySize() > 0 ? newList.duplicate() : pbnjson::Array();
}

LaunchAppItemPtr WebAppLifeHandler::getLSCallRequestItemByToken(const LSMessageToken& token)
{
    auto it = m_lscallRequestList.begin();
    auto it_end = m_lscallRequestList.end();
    for (; it != it_end; ++it) {
        if ((*it)->returnToken() == token)
            return *it;
    }
    return NULL;
}

void WebAppLifeHandler::removeItemFromLSCallRequestList(const std::string& uid)
{
    auto it = std::find_if(m_lscallRequestList.begin(), m_lscallRequestList.end(), [&uid](LaunchAppItemPtr item) {return (item->getUid() == uid);});
    if (it == m_lscallRequestList.end())
        return;

    Logger::info(getClassName(), __FUNCTION__, (*it)->getAppId(), "removed from checking queue");
    m_lscallRequestList.erase(it);
}

void WebAppLifeHandler::addLoadingApp(const std::string& appId)
{
    if (std::find(m_loadingList.begin(), m_loadingList.end(), appId) == m_loadingList.end()) {
        Logger::info(getClassName(), __FUNCTION__, appId, "add_to_wam_loading");
        m_loadingList.push_back(appId);
    }
}

void WebAppLifeHandler::removeLoadingApp(const std::string& appId)
{
    auto it = std::find(m_loadingList.begin(), m_loadingList.end(), appId);
    if (it != m_loadingList.end()) {
        Logger::info(getClassName(), __FUNCTION__, appId, "remove_from_wam_loading");
        m_loadingList.erase(it);
    }
}

bool WebAppLifeHandler::isLoading(const std::string& appId)
{
    return std::find(m_loadingList.begin(), m_loadingList.end(), appId) != m_loadingList.end();
}


