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

#include <bus/AppMgrService.h>
#include <bus/ServiceObserver.h>
#include <lifecycle/AppInfoManager.h>
#include <lifecycle/handler/WebAppLifeHandler.h>
#include <package/PackageManager.h>
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

    ServiceObserver::instance().add(WEBOS_SERVICE_WAM, std::bind(&WebAppLifeHandler::onWAMServiceStatusChanged, this, std::placeholders::_1));

    AppMgrService::instance().signalOnServiceReady.connect(boost::bind(&WebAppLifeHandler::onServiceReady, this));
}

WebAppLifeHandler::~WebAppLifeHandler()
{
}

//////////////////////////////////////////////////////////////
/// launch
//////////////////////////////////////////////////////////////
void WebAppLifeHandler::launch(LaunchAppItemPtr item)
{
    AppDescPtr app_desc = PackageManager::instance().getAppById(item->getAppId());
    if (app_desc == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", item->getAppId().c_str()),
                  PMLOGKS("reason", "null_description"),
                  PMLOGKS("where", "webapp_launch"), "");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        signal_launching_done(item->getUid());
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("appDesc", app_desc->toJValue());
    payload.put("reason", item->getReason());
    payload.put("parameters", item->getParams());
    payload.put("launchingAppId", item->getCallerId());
    payload.put("launchingProcId", item->getCallerPid());
    payload.put("keepAlive", item->isKeepAlive());

    if (!(item->getPreload().empty()))
        payload.put("preload", item->getPreload());

    LSMessageToken token = 0;
    LSErrorSafe lserror;
    if (!LSCallOneReply(AppMgrService::instance().serviceHandle(),
                        "luna://com.palm.webappmanager/launchApp",
                        payload.stringify().c_str(),
                        onReturnForLaunchRequest,
                        this,
                        &token,
                        &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "lscallonereply"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", "wam_launchApp"), "err: %s", lserror.message);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        signal_launching_done(item->getUid());
        return;
    }

    addLoadingApp(item->getAppId());

    if (item->getPreload().empty())
        signal_app_life_status_changed(item->getAppId(), item->getUid(), RuntimeStatus::LAUNCHING);
    else
        signal_app_life_status_changed(item->getAppId(), item->getUid(), RuntimeStatus::PRELOADING);

    double current_time = getCurrentTime();
    double elapsed_time = current_time - item->launchStartTime();
    LOG_INFO(MSGID_APP_LAUNCHED, 5,
             PMLOGKS("app_id", item->getAppId().c_str()),
             PMLOGKS("type", "web"), PMLOGKS("pid", ""),
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
                  PMLOGKS("reason", "null_launching_item"),
                  PMLOGKS("where", "wam_launch_cb_return"), "internal error");
        return false;
    }

    uid = item->getUid();
    g_this->removeItemFromLSCallRequestList(uid);
    item->resetReturnToken();

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "parsing_fail"),
                  PMLOGKS("where", "wam_launch_cb_return"), "internal error");
        // How could we get the previous status? We should restore it.
        // g_this->signal_app_life_status_changed(item->app_id(), "", RuntimeStatus::STOP);
        return false;
    }

    std::string app_id = jmsg.hasKey("appId") && jmsg["appId"].isString() ? jmsg["appId"].asString() : item->getAppId();
    std::string proc_id = jmsg.hasKey("procId") && jmsg["procId"].isString() ? jmsg["procId"].asString() : "";

    // TODO: WAM doesn't return "returnValue" on success
    //       report this issue to WAM
    //       SAM should handled after WAM's work
    //       This "returnValue" is from ls-hubd when service is disconnected.
    if (jmsg.hasKey("returnValue") && jmsg["returnValue"].asBool() == false) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("app_type", "webapp"),
                  PMLOGKS("app_id", app_id.c_str()),
                  "err: %s", jmsg.stringify().c_str());

        if (app_id.empty()) {
            LOG_CRITICAL(MSGID_LIFE_STATUS_ERR, 2,
                         PMLOGKS("reason", "cannot_handle_life_status"),
                         PMLOGKS("where", "wam_launch_cb_return"), "");
        }

        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "WebAppMgr's launchApp is failed");
        g_this->signal_launching_done(item->getUid());

        g_this->signal_app_life_status_changed(app_id, "", RuntimeStatus::STOP);
        return true;
    }

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS("app_id", app_id.c_str()),
             PMLOGKS("status", "received_launch_return_from_wam"), "");

    item->setPid(proc_id);
    g_this->signal_launching_done(item->getUid());

    return true;
}

//////////////////////////////////////////////////////////////
/// close
//////////////////////////////////////////////////////////////
void WebAppLifeHandler::close(CloseAppItemPtr item, std::string& err_text)
{
    bool need_to_handle_fake_stop = false;
    if (!AppInfoManager::instance().isRunning(item->getAppId())) {
        if (isLoading(item->getAppId())) {
            need_to_handle_fake_stop = true;
        } else {
            LOG_INFO(MSGID_APPCLOSE, 1, PMLOGKS("app_id", item->getAppId().c_str()), "webapp is not running");
            err_text = "app is not running";
            return;
        }
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("appId", item->getAppId());
    payload.put("reason", item->getReason());

    LSErrorSafe lserror;
    if (!LSCallOneReply(AppMgrService::instance().serviceHandle(),
                        "luna://com.palm.webappmanager/killApp",
                        payload.stringify().c_str(),
                        onReturnForCloseRequest, this, NULL, &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "lscallonereply"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", "wam_killApp"), "err: %s", lserror.message);
        err_text = "kill request fail";
        return;
    }

    signal_app_life_status_changed(item->getAppId(), "", RuntimeStatus::CLOSING);
    if (need_to_handle_fake_stop) {
        removeLoadingApp(item->getAppId());
        LOG_INFO(MSGID_APPCLOSE, 1, PMLOGKS("app_id", item->getAppId().c_str()), "running is not updated yet. set stop status manually");
        signal_app_life_status_changed(item->getAppId(), "", RuntimeStatus::STOP);
    }
}

bool WebAppLifeHandler::onReturnForCloseRequest(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS("reason", "parsing_fail"),
                  PMLOGKS("where", "wam_kill_cb_return"), "internal error");
        return false;
    }

    if (jmsg["returnValue"].asBool()) {
        std::string app_id = jmsg["appId"].asString();
        std::string pid = jmsg["processId"].asString();
        LOG_INFO(MSGID_APPCLOSE, 3,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("pid", pid.c_str()),
                 PMLOGKS("status", "received_close_return_from_wam"), "");
    } else {
        std::string err_text = jmsg["errorText"].asString();
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS("reason", "return_false"),
                  PMLOGKS("where", "wam_kill_cb_return"), "err: %s", err_text.c_str());
        // How could we get the previous status? We should restore it.
        // g_this->signal_app_life_status_changed(app_id(), "", RuntimeStatus::STOP);

    }

    return true;
}

//////////////////////////////////////////////////////////////
/// pause
//////////////////////////////////////////////////////////////
void WebAppLifeHandler::pause(const std::string& app_id, const pbnjson::JValue& params, std::string& err_text, bool send_life_event)
{

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("appId", app_id.c_str());
    payload.put("reason", "pause");
    payload.put("parameters", params);

    LSErrorSafe lserror;
    if (!LSCallOneReply(AppMgrService::instance().serviceHandle(),
                        "luna://com.palm.webappmanager/pauseApp",
                        payload.stringify().c_str(),
                        onReturnForPauseRequest,
                        this,
                        NULL,
                        &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "lscallonereply"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", __FUNCTION__), "err: %s", lserror.message);
        err_text = "pause request fail";
        return;
    }

    if (send_life_event)
        signal_app_life_status_changed(app_id, "", RuntimeStatus::PAUSING);
}

bool WebAppLifeHandler::onReturnForPauseRequest(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPPAUSE_ERR, 2,
                  PMLOGKS("reason", "parsing_fail"),
                  PMLOGKS("where", __FUNCTION__), "internal error");
        return false;
    }

    if (jmsg["returnValue"].asBool()) {
        std::string app_id = jmsg["appId"].asString();
        LOG_INFO(MSGID_APPPAUSE, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("status", "received_pause_return_from_wam"), "");
    } else {
        std::string err_text = jmsg["errorText"].asString();
        LOG_ERROR(MSGID_APPPAUSE, 2,
                  PMLOGKS("reason", "return_false"),
                  PMLOGKS("where", __FUNCTION__), "err: %s", err_text.c_str());
    }

    return true;
}

//////////////////////////////////////////////////////////////
/// running list
//////////////////////////////////////////////////////////////
void WebAppLifeHandler::onWAMServiceStatusChanged(bool connection)
{
    if (connection) {
        subscribeWAMRunningList();
    } else {
        LSErrorSafe lserror;
        if (0 != m_wamSubscriptionToken) {
            if (!LSCallCancel(AppMgrService::instance().serviceHandle(), m_wamSubscriptionToken, &lserror)) {
                LOG_ERROR(MSGID_LSCALL_ERR, 3,
                          PMLOGKS("type", "lscallcancel"),
                          PMLOGKS("payload", ""),
                          PMLOGKS("where", "wam_listRunningApps"),
                          "err: %s", lserror.message);
            }
            m_wamSubscriptionToken = 0;
        }
        signal_service_disconnected();
    }
}

void WebAppLifeHandler::onServiceReady()
{
    subscribeWAMRunningList();
}

void WebAppLifeHandler::subscribeWAMRunningList()
{
    if (AppMgrService::instance().isServiceReady() == false || ServiceObserver::instance().isConnected(WEBOS_SERVICE_WAM) == false || m_wamSubscriptionToken != 0) {
        return;
    }

    LSErrorSafe lserror;
    if (!LSCall(AppMgrService::instance().serviceHandle(), "luna://com.palm.webappmanager/listRunningApps", "{\"includeSysApps\":true,\"subscribe\":true}", onWAMSubscriptionRunninglist, this,
            &m_wamSubscriptionToken, &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "lscall"),
                  PMLOGJSON("payload", "{\"includeSysApps\":true,\"subscribe\":true}"),
                  PMLOGKS("where", "wam_listRUnningApps"),
                  "err: %s", lserror.message);
    }
}
/*
 {
 "running": [
 {
 "id": "com.webos.app.tvuserguide",
 "webprocessid": "2904",
 "processid": "1002"
 },
 {
 "id": "com.palm.app.settings",
 "webprocessid": "2206",
 "processid": "1001"
 },
 {
 "id": "com.webos.app.camera",
 "webprocessid": "2933",
 "processid": "1003"
 }
 ],
 "returnValue": true
 }
 */

bool WebAppLifeHandler::onWAMSubscriptionRunninglist(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));

    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_WAM_RUNNING_ERR, 2,
                  PMLOGKS("reason", "parsing_fail"),
                  PMLOGKS("where", "wam_running_cb_return"), "");
        return false;
    }

    pbnjson::JValue running_list = pbnjson::Array();
    if (!jmsg.hasKey("running") || !jmsg["running"].isArray()) {
        LOG_ERROR(MSGID_WAM_RUNNING_ERR, 3,
                  PMLOGKS("reason", "invalid_running"),
                  PMLOGKS("where", "wam_running_cb_return"),
                  PMLOGJSON("payload", jmsg.stringify().c_str()), "");
    } else {
        LOG_DEBUG("WAM_RUNNING: %s", jmsg.stringify().c_str());
        running_list = jmsg["running"];
    }

    g_this->handleRunningListChange(running_list);

    return true;
}

void WebAppLifeHandler::handleRunningListChange(const pbnjson::JValue& new_list)
{
    int new_list_len = new_list.arraySize();
    int old_list_len = m_runningList.arraySize();

    for (int i = 0; i < old_list_len; ++i) {
        std::string old_app_id = m_runningList[i]["id"].asString();
        std::string old_webprocid = m_runningList[i]["webprocessid"].asString();

        bool is_removed = true;
        bool is_changed = false;
        for (int j = 0; j < new_list_len; ++j) {
            if (new_list[j]["id"].asString() == old_app_id) {
                if (new_list[j]["webprocessid"].asString() == old_webprocid)
                    is_removed = false;
                else
                    is_changed = true;

                break;
            }
        }

        if (is_removed) {
            LOG_INFO(MSGID_APP_CLOSED, 3,
                     PMLOGKS("app_id", old_app_id.c_str()),
                     PMLOGKS("type", "web"),
                     PMLOGKS("pid", ""), "");

            LOG_INFO(MSGID_WAM_RUNNING, 5,
                     PMLOGKS("app_id", old_app_id.c_str()),
                     PMLOGKS("status", "removed"),
                     PMLOGKS("invalid_pid_removed", (old_webprocid == INVALID_PROCESS_ID)?"yes":"no"),
                     PMLOGKS("zero_pid_removed", (old_webprocid == PROCESS_ID_ZERO)?"yes":"no"),
                     PMLOGKS("is_changed", is_changed?"true":"false"), "");

            if (!is_changed)
                signal_app_life_status_changed(old_app_id, "", RuntimeStatus::STOP);

            if ((old_webprocid != INVALID_PROCESS_ID) && (old_webprocid != PROCESS_ID_ZERO))
                signal_running_app_removed(old_app_id);
        }
    }

    for (int i = 0; i < new_list_len; ++i) {
        std::string new_app_id = new_list[i]["id"].asString();
        std::string new_pid = new_list[i]["processid"].asString();
        std::string new_webprocid = new_list[i]["webprocessid"].asString();

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
                     PMLOGKS("app_id", new_app_id.c_str()),
                     PMLOGKS("status", "invalid_pid_added"),
                     PMLOGKS("pid", new_pid.c_str()),
                     PMLOGKS("webprocid", new_webprocid.c_str()),
                     PMLOGKS("is_changed", is_changed?"true":"false"), "");
            continue;
        }

        if (is_added) {
            LOG_INFO(MSGID_WAM_RUNNING, 5,
                     PMLOGKS("app_id", new_app_id.c_str()),
                     PMLOGKS("status", "added"),
                     PMLOGKS("pid", new_pid.c_str()),
                     PMLOGKS("webprocid", new_webprocid.c_str()),
                     PMLOGKS("is_changed", is_changed?"true":"false"), "");
            signal_running_app_added(new_app_id, new_webprocid, new_webprocid);
            removeLoadingApp(new_app_id);

            if (!is_changed)
                signal_app_life_status_changed(new_app_id, "", RuntimeStatus::RUNNING);
        }
    }

    m_runningList = new_list.arraySize() > 0 ? new_list.duplicate() : pbnjson::Array();
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
             PMLOGKS("app_id", (*it)->getAppId().c_str()),
             PMLOGKS("uid", uid.c_str()), "removed from checking queue");

    m_lscallRequestList.erase(it);
}

void WebAppLifeHandler::addLoadingApp(const std::string& app_id)
{
    if (std::find(m_loadingList.begin(), m_loadingList.end(), app_id) == m_loadingList.end()) {
        LOG_INFO(MSGID_APPLAUNCH, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("status", "add_to_wam_loading"), "");
        m_loadingList.push_back(app_id);
    }
}

void WebAppLifeHandler::removeLoadingApp(const std::string& app_id)
{
    auto it = std::find(m_loadingList.begin(), m_loadingList.end(), app_id);
    if (it != m_loadingList.end()) {
        LOG_INFO(MSGID_APPLAUNCH, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("status", "remove_from_wam_loading"), "");
        m_loadingList.erase(it);
    }
}

bool WebAppLifeHandler::isLoading(const std::string& app_id)
{
    return std::find(m_loadingList.begin(), m_loadingList.end(), app_id) != m_loadingList.end();
}


