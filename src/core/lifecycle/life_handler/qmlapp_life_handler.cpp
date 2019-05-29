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

#include "core/lifecycle/life_handler/qmlapp_life_handler.h"

#include <boost/lexical_cast.hpp>
#include <boost/signals2.hpp>
#include <core/package/package_manager.h>
#include <core/util/jutil.h>
#include <core/util/logging.h>
#include <core/util/lsutils.h>
#include <core/util/utils.h>

#include "core/bus/appmgr_service.h"
#include "core/lifecycle/app_info_manager.h"

static QmlAppLifeHandler* g_this = NULL;

QmlAppLifeHandler::QmlAppLifeHandler()
{
    g_this = this;
    AppMgrService::instance().signalOnServiceReady.connect(boost::bind(&QmlAppLifeHandler::on_service_ready, this));
}

QmlAppLifeHandler::~QmlAppLifeHandler()
{
}

void QmlAppLifeHandler::on_service_ready()
{
    LSErrorSafe lserror;
    if (!LSCall(AppMgrService::instance().serviceHandle(), "luna://com.webos.service.bus/signal/addmatch", R"({"category":"/booster","method":"processFinished"})", qml_process_watcher, this, NULL,
            &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "lscall"), PMLOGJSON("payload", R"({"category":"/booster","method":"processFinished"})"), PMLOGKS("where", "booster_addmatch"), "err: %s",
                lserror.message);
    }
}

void QmlAppLifeHandler::launch(AppLaunchingItemPtr item)
{
    AppDescPtr app_desc = PackageManager::instance().getAppById(item->appId());
    if (app_desc == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("app_id", item->appId().c_str()), PMLOGKS("reason", "null_description"), PMLOGKS("where", "qmlapp_launch"), "");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        signal_launching_done(item->uid());
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("main", app_desc->entryPoint());
    payload.put("appId", item->appId());
    payload.put("params", item->params().duplicate().stringify());

    LSMessageToken token = 0;
    LSErrorSafe lserror;
    if (!LSCallOneReply(AppMgrService::instance().serviceHandle(),
                        "luna://com.webos.booster/launch",
                        payload.stringify().c_str(),
                        cb_return_booster_launch,
                        this,
                        &token,
                        &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "lscallonereply"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", "booster_launch"), "err: %s", lserror.message);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        signal_launching_done(item->uid());
        return;
    }

    if (item->preload().empty())
        signal_app_life_status_changed(item->appId(), item->uid(), RuntimeStatus::LAUNCHING);
    else
        signal_app_life_status_changed(item->appId(), item->uid(), RuntimeStatus::PRELOADING);

    double current_time = get_current_time();
    double elapsed_time = current_time - item->launchStartTime();
    LOG_INFO(MSGID_APP_LAUNCHED, 5, PMLOGKS("app_id", item->appId().c_str()), PMLOGKS("type", "qml"), PMLOGKS("pid", ""), PMLOGKFV("start_time", "%f", item->launchStartTime()),
            PMLOGKFV("collapse_time", "%f", elapsed_time), "");

    item->setReturnToken(token);
    m_lscall_request_list.push_back(item);
}

bool QmlAppLifeHandler::cb_return_booster_launch(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    LSMessageToken token = 0;
    std::string uid = "";
    AppLaunchingItemPtr item = NULL;

    token = LSMessageGetResponseToken(lsmsg);
    item = g_this->get_lscall_request_item_by_token(token);

    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("reason", "null_launching_item"), PMLOGKS("where", "booster_launch_cb_return"), "internal error");
        return false;
    }

    uid = item->uid();
    g_this->remove_item_from_lscall_request_list(uid);
    item->resetReturnToken();

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("reason", "parsing_fail"), PMLOGKS("where", "booster_launch_cb_return"), "internal error");
        // TODO: set proper life status for this error case
        // g_this->signal_app_life_status_changed(item->app_id(), "", RuntimeStatus::STOP);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "booster error");
        g_this->signal_launching_done(item->uid());
        return false;
    }

    if (!jmsg.hasKey("returnValue") || jmsg["returnValue"].asBool() == false) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("app_id", item->appId().c_str()),
                  PMLOGKS("reason", "invalid_return"),
                  "invalid_booster_return err: %s", jmsg.stringify().c_str());

        // TODO: set proper life status for this error case
        g_this->signal_app_life_status_changed(item->appId(), "", RuntimeStatus::STOP);

        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "booster error");
        g_this->signal_launching_done(item->uid());
        return true;
    }

    std::string app_id = jmsg.hasKey("appId") && jmsg["appId"].isString() ? jmsg["appId"].asString() : "";
    std::string pid = jmsg.hasKey("pid") && jmsg["pid"].isNumber() ? boost::lexical_cast<std::string>(jmsg["pid"].asNumber<int>()) : "";

    if (app_id.empty()) {
        LOG_WARNING(MSGID_APPLAUNCH, 2, PMLOGKS("status", "received_empty_app_id"), PMLOGKS("restoring_app_id", item->appId().c_str()), "now restore app_id from sam info");
        app_id = item->appId();
    }

    if (pid.empty()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("app_id", item->appId().c_str()),
                  PMLOGKS("reason", "received_empty_pid"),
                  "invalid_booster_return err: %s", jmsg.stringify().c_str());

        // TODO: set proper life status for this error case
        g_this->signal_app_life_status_changed(item->appId(), "", RuntimeStatus::STOP);

        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "booster error");
        g_this->signal_launching_done(item->uid());
        return true;
    }

    LOG_INFO(MSGID_APPLAUNCH, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "booster_launched_app"), "");

    item->setPid(pid);
    g_this->signal_running_app_added(app_id, pid, "");
    g_this->signal_app_life_status_changed(app_id, "", RuntimeStatus::RUNNING);

    g_this->signal_launching_done(item->uid());

    return true;
}

void QmlAppLifeHandler::close(AppCloseItemPtr item, std::string& err_text)
{
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("appId", item->getAppId());

    LSErrorSafe lserror;
    if (!LSCallOneReply(AppMgrService::instance().serviceHandle(),
                        "luna://com.webos.booster/close",
                        payload.stringify().c_str(),
                        cb_return_booster_close,
                        NULL,
                        NULL,
                        &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "lscallonereply"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", "booster_close"), "err: %s", lserror.message);
        err_text = "close request fail";
    }

    signal_app_life_status_changed(item->getAppId(), "", RuntimeStatus::CLOSING);
}

bool QmlAppLifeHandler::cb_return_booster_close(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2, PMLOGKS("reason", "parsing_fail"), PMLOGKS("where", "booster_close_cb_return"), "internal error");
        return false;
    }

    if (jmsg["returnValue"].asBool()) {
        std::string app_id = jmsg.hasKey("appId") && jmsg["appId"].isString() ? jmsg["appId"].asString() : "";
        std::string pid = jmsg.hasKey("pid") && jmsg["pid"].isNumber() ? boost::lexical_cast<std::string>(jmsg["pid"].asNumber<int>()) : "";
        LOG_INFO(MSGID_APPCLOSE, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("pid", pid.c_str()), PMLOGKS("status", "received_close_return_from_booster"), "");
    } else {
        std::string err_text = jmsg["errorText"].asString();
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2, PMLOGKS("reason", "return_false"), PMLOGKS("where", "booster_kill_cb_return"), "err: %s", err_text.c_str());
    }

    return true;
}

void QmlAppLifeHandler::pause(const std::string& app_id, const pbnjson::JValue& params, std::string& err_text, bool send_life_event)
{
    LOG_ERROR(MSGID_APPPAUSE, 1, PMLOGKS("app_id", app_id.c_str()), "no interface defined for qml booster");
    err_text = "no interface defined for qml booster";
    return;
}

bool QmlAppLifeHandler::qml_process_watcher(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    LOG_INFO(MSGID_APPCLOSE, 1, PMLOGKS("where", "booster_process_watcher"), "received closed event");

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2, PMLOGKS("reason", "parsing_fail"), PMLOGKS("where", "booster_process_watcher"), "internal error");
        return true;
    }
    if (!jmsg.hasKey("pid") || !jmsg["pid"].isNumber()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS("where", "booster_process_watcher"),
                  PMLOGJSON("payload", jmsg.stringify().c_str()),
                  "invalid pid info");
        return true;
    }

    std::string pid = boost::lexical_cast<std::string>(jmsg["pid"].asNumber<int>());
    std::string app_id = AppInfoManager::instance().getAppIdByPid(pid);
    if (app_id.empty()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("pid", pid.c_str()), "err: no app_id matched by pid");
        return true;
    }

    LOG_INFO(MSGID_APP_CLOSED, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("type", "qml"), PMLOGKS("pid", pid.c_str()), "");

    g_this->signal_app_life_status_changed(app_id, "", RuntimeStatus::STOP);
    g_this->signal_running_app_removed(app_id);

    return true;
}

AppLaunchingItemPtr QmlAppLifeHandler::get_lscall_request_item_by_token(const LSMessageToken& token)
{
    auto it = m_lscall_request_list.begin();
    auto it_end = m_lscall_request_list.end();
    for (; it != it_end; ++it) {
        if ((*it)->returnToken() == token)
            return *it;
    }
    return NULL;
}

void QmlAppLifeHandler::remove_item_from_lscall_request_list(const std::string& uid)
{
    auto it = std::find_if(m_lscall_request_list.begin(), m_lscall_request_list.end(), [&uid](AppLaunchingItemPtr item) {return (item->uid() == uid);});
    if (it == m_lscall_request_list.end())
        return;

    LOG_INFO(MSGID_APPLAUNCH, 2, PMLOGKS("app_id", (*it)->appId().c_str()), PMLOGKS("uid", uid.c_str()), "removed from checking queue");

    m_lscall_request_list.erase(it);
}

