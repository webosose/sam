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

#include <bus/service/ApplicationManager.h>
#include "bus/client/WAM.h"
#include <lifecycle/handler/WebAppLifeHandler.h>
#include <lifecycle/RunningInfoManager.h>
#include <package/AppPackageManager.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/LSUtils.h>
#include <util/Utils.h>

const std::string INVALID_PROCESS_ID = "-1";
const std::string PROCESS_ID_ZERO = "0";
static WebAppLifeHandler* g_this = NULL;

WebAppLifeHandler::WebAppLifeHandler()
    : m_wamSubscriptionToken(0),
      m_runningList(pbnjson::Array())
{
    g_this = this;

    WAM::getInstance().EventListRunningAppsChanged.connect(boost::bind(&WebAppLifeHandler::onListRunningAppsChanged, this, _1));
}

WebAppLifeHandler::~WebAppLifeHandler()
{
}

void WebAppLifeHandler::launch(LaunchAppItemPtr item)
{
    AppPackagePtr app_desc = AppPackageManager::getInstance().getAppById(item->getAppId());
    if (app_desc == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                  PMLOGKS(LOG_KEY_REASON, "null_description"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        EventLaunchingDone(item->getUid());
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("appDesc", app_desc->toJValue());
    payload.put(LOG_KEY_REASON, item->getReason());
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
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS(LOG_KEY_TYPE, "lscallonereply"),
                  PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "err: %s", lserror.message);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        EventLaunchingDone(item->getUid());
        return;
    }

    addLoadingApp(item->getAppId());

    if (item->getPreload().empty())
        EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::LAUNCHING);
    else
        EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::PRELOADING);

    double current_time = getCurrentTime();
    double elapsed_time = current_time - item->launchStartTime();
    LOG_INFO(MSGID_APP_LAUNCHED, 5,
             PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
             PMLOGKS(LOG_KEY_TYPE, "web"), PMLOGKS("pid", ""),
             PMLOGKFV("start_time", "%f", item->launchStartTime()),
             PMLOGKFV("collapse_time", "%f", elapsed_time), "");

    item->setReturnToken(token);
    m_lscallRequestList.push_back(item);
}

bool WebAppLifeHandler::onReturnForLaunchRequest(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    LSMessageToken token = 0;
    std::string uid = "";
    LaunchAppItemPtr item = NULL;

    token = LSMessageGetResponseToken(lsmsg);
    item = g_this->getLSCallRequestItemByToken(token);

    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "null_launching_item"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "internal error");
        return false;
    }

    uid = item->getUid();
    g_this->removeItemFromLSCallRequestList(uid);
    item->resetReturnToken();

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "parsing_fail"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "internal error");
        // How could we get the previous status? We should restore it.
        // g_this->signal_app_life_status_changed(item->app_id(), "", RuntimeStatus::STOP);
        return false;
    }

    std::string app_id = jmsg.hasKey(LOG_KEY_APPID) && jmsg[LOG_KEY_APPID].isString() ? jmsg[LOG_KEY_APPID].asString() : item->getAppId();
    std::string proc_id = jmsg.hasKey("procId") && jmsg["procId"].isString() ? jmsg["procId"].asString() : "";

    // TODO: WAM doesn't return "returnValue" on success
    //       report this issue to WAM
    //       SAM should handled after WAM's work
    //       This "returnValue" is from ls-hubd when service is disconnected.
    if (jmsg.hasKey("returnValue") && jmsg["returnValue"].asBool() == false) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("app_type", "webapp"),
                  PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
                  "err: %s", jmsg.stringify().c_str());

        if (app_id.empty()) {
            LOG_CRITICAL(MSGID_LIFE_STATUS_ERR, 2,
                         PMLOGKS(LOG_KEY_REASON, "cannot_handle_life_status"),
                         PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");
        }

        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "WebAppMgr's launchApp is failed");
        g_this->EventLaunchingDone(item->getUid());

        g_this->EventAppLifeStatusChanged(app_id, "", RuntimeStatus::STOP);
        return true;
    }

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
             PMLOGKS("status", "received_launch_return_from_wam"), "");

    item->setPid(proc_id);
    g_this->EventLaunchingDone(item->getUid());

    return true;
}

void WebAppLifeHandler::close(CloseAppItemPtr item, std::string& err_text)
{
    bool need_to_handle_fake_stop = false;
    if (!RunningInfoManager::getInstance().isRunning(item->getAppId())) {
        if (isLoading(item->getAppId())) {
            need_to_handle_fake_stop = true;
        } else {
            LOG_INFO(MSGID_APPCLOSE, 1,
                     PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                     "webapp is not running");
            err_text = "app is not running";
            return;
        }
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put(LOG_KEY_APPID, item->getAppId());
    payload.put(LOG_KEY_REASON, item->getReason());

    LSErrorSafe lserror;
    if (!LSCallOneReply(ApplicationManager::getInstance().get(),
                        "luna://com.palm.webappmanager/killApp",
                        payload.stringify().c_str(),
                        onReturnForCloseRequest, this, NULL, &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS(LOG_KEY_TYPE, "lscallonereply"),
                  PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "err: %s", lserror.message);
        err_text = "kill request fail";
        return;
    }

    EventAppLifeStatusChanged(item->getAppId(), "", RuntimeStatus::CLOSING);
    if (need_to_handle_fake_stop) {
        removeLoadingApp(item->getAppId());
        LOG_INFO(MSGID_APPCLOSE, 1,
                 PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                 "running is not updated yet. set stop status manually");
        EventAppLifeStatusChanged(item->getAppId(), "", RuntimeStatus::STOP);
    }
}

bool WebAppLifeHandler::onReturnForCloseRequest(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "parsing_fail"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "internal error");
        return false;
    }

    if (jmsg["returnValue"].asBool()) {
        std::string app_id = jmsg[LOG_KEY_APPID].asString();
        std::string pid = jmsg["processId"].asString();
        LOG_INFO(MSGID_APPCLOSE, 3,
                 PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
                 PMLOGKS("pid", pid.c_str()),
                 PMLOGKS("status", "received_close_return_from_wam"), "");
    } else {
        std::string err_text = jmsg[LOG_KEY_ERRORTEXT].asString();
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "return_false"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "err: %s", err_text.c_str());
        // How could we get the previous status? We should restore it.
        // g_this->signal_app_life_status_changed(app_id(), "", RuntimeStatus::STOP);

    }

    return true;
}

void WebAppLifeHandler::pause(const std::string& app_id, const pbnjson::JValue& params, std::string& err_text, bool send_life_event)
{

    pbnjson::JValue payload = pbnjson::Object();
    payload.put(LOG_KEY_APPID, app_id.c_str());
    payload.put(LOG_KEY_REASON, "pause");
    payload.put("parameters", params);

    LSErrorSafe lserror;
    if (!LSCallOneReply(ApplicationManager::getInstance().get(),
                        "luna://com.palm.webappmanager/pauseApp",
                        payload.stringify().c_str(),
                        onReturnForPauseRequest,
                        this,
                        NULL,
                        &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS(LOG_KEY_TYPE, "lscallonereply"),
                  PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "err: %s", lserror.message);
        err_text = "pause request fail";
        return;
    }

    if (send_life_event)
        EventAppLifeStatusChanged(app_id, "", RuntimeStatus::PAUSING);
}

bool WebAppLifeHandler::onReturnForPauseRequest(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPPAUSE_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "parsing_fail"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "internal error");
        return false;
    }

    if (jmsg["returnValue"].asBool()) {
        std::string app_id = jmsg[LOG_KEY_APPID].asString();
        LOG_INFO(MSGID_APPPAUSE, 2,
                 PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
                 PMLOGKS("status", "received_pause_return_from_wam"), "");
    } else {
        std::string err_text = jmsg[LOG_KEY_ERRORTEXT].asString();
        LOG_ERROR(MSGID_APPPAUSE, 2,
                  PMLOGKS(LOG_KEY_REASON, "return_false"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "err: %s", err_text.c_str());
    }

    return true;
}

void WebAppLifeHandler::onListRunningAppsChanged(const pbnjson::JValue& newList)
{
    int new_list_len = newList.arraySize();
    int old_list_len = m_runningList.arraySize();

    for (int i = 0; i < old_list_len; ++i) {
        std::string old_app_id = m_runningList[i]["id"].asString();
        std::string old_webprocid = m_runningList[i]["webprocessid"].asString();

        bool is_removed = true;
        bool is_changed = false;
        for (int j = 0; j < new_list_len; ++j) {
            if (newList[j]["id"].asString() == old_app_id) {
                if (newList[j]["webprocessid"].asString() == old_webprocid)
                    is_removed = false;
                else
                    is_changed = true;

                break;
            }
        }

        if (is_removed) {
            LOG_INFO(MSGID_APP_CLOSED, 3,
                     PMLOGKS(LOG_KEY_APPID, old_app_id.c_str()),
                     PMLOGKS(LOG_KEY_TYPE, "web"),
                     PMLOGKS("pid", ""), "");

            LOG_INFO(MSGID_WAM_RUNNING, 5,
                     PMLOGKS(LOG_KEY_APPID, old_app_id.c_str()),
                     PMLOGKS("status", "removed"),
                     PMLOGKS("invalid_pid_removed", (old_webprocid == INVALID_PROCESS_ID)?"yes":"no"),
                     PMLOGKS("zero_pid_removed", (old_webprocid == PROCESS_ID_ZERO)?"yes":"no"),
                     PMLOGKS("is_changed", is_changed?"true":"false"), "");

            if (!is_changed)
                EventAppLifeStatusChanged(old_app_id, "", RuntimeStatus::STOP);

            if ((old_webprocid != INVALID_PROCESS_ID) && (old_webprocid != PROCESS_ID_ZERO))
                EventRunningAppRemoved(old_app_id);
        }
    }

    for (int i = 0; i < new_list_len; ++i) {
        std::string new_app_id = newList[i]["id"].asString();
        std::string new_pid = newList[i]["processid"].asString();
        std::string new_webprocid = newList[i]["webprocessid"].asString();

        LOG_DEBUG("WAM_RUNNING item (%s:%s:%s)", new_app_id.c_str(), new_pid.c_str(), new_webprocid.c_str());

        bool is_added = true;
        bool is_changed = false;

        for (int j = 0; j < old_list_len; ++j) {
            if (m_runningList[j]["id"].asString() == new_app_id) {
                if (m_runningList[j]["webprocessid"].asString() == new_webprocid)
                    is_added = false;
                else
                    is_changed = true;

                break;
            }
        }

        // protect to invalid process id "-1" from WAM
        if ((new_webprocid == INVALID_PROCESS_ID) || (new_webprocid == PROCESS_ID_ZERO)) {
            LOG_INFO(MSGID_WAM_RUNNING_ERR, 5,
                     PMLOGKS(LOG_KEY_APPID, new_app_id.c_str()),
                     PMLOGKS("status", "invalid_pid_added"),
                     PMLOGKS("pid", new_pid.c_str()),
                     PMLOGKS("webprocid", new_webprocid.c_str()),
                     PMLOGKS("is_changed", is_changed?"true":"false"), "");
            continue;
        }

        if (is_added) {
            LOG_INFO(MSGID_WAM_RUNNING, 5,
                     PMLOGKS(LOG_KEY_APPID, new_app_id.c_str()),
                     PMLOGKS("status", "added"),
                     PMLOGKS("pid", new_pid.c_str()),
                     PMLOGKS("webprocid", new_webprocid.c_str()),
                     PMLOGKS("is_changed", is_changed?"true":"false"), "");
            EventRunningAppAdded(new_app_id, new_webprocid, new_webprocid);
            removeLoadingApp(new_app_id);

            if (!is_changed)
                EventAppLifeStatusChanged(new_app_id, "", RuntimeStatus::RUNNING);
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

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS(LOG_KEY_APPID, (*it)->getAppId().c_str()),
             PMLOGKS("uid", uid.c_str()), "removed from checking queue");

    m_lscallRequestList.erase(it);
}

void WebAppLifeHandler::addLoadingApp(const std::string& app_id)
{
    if (std::find(m_loadingList.begin(), m_loadingList.end(), app_id) == m_loadingList.end()) {
        LOG_INFO(MSGID_APPLAUNCH, 2,
                 PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
                 PMLOGKS("status", "add_to_wam_loading"), "");
        m_loadingList.push_back(app_id);
    }
}

void WebAppLifeHandler::removeLoadingApp(const std::string& app_id)
{
    auto it = std::find(m_loadingList.begin(), m_loadingList.end(), app_id);
    if (it != m_loadingList.end()) {
        LOG_INFO(MSGID_APPLAUNCH, 2,
                 PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
                 PMLOGKS("status", "remove_from_wam_loading"), "");
        m_loadingList.erase(it);
    }
}

bool WebAppLifeHandler::isLoading(const std::string& app_id)
{
    return std::find(m_loadingList.begin(), m_loadingList.end(), app_id) != m_loadingList.end();
}


