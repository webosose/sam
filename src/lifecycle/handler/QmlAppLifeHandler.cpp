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

#include <boost/lexical_cast.hpp>
#include <boost/signals2.hpp>
#include <bus/service/ApplicationManager.h>
#include <bus/service/ApplicationManager.h>
#include <lifecycle/handler/QmlAppLifeHandler.h>
#include <lifecycle/RunningInfoManager.h>
#include <package/AppPackageManager.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/LSUtils.h>
#include <util/Utils.h>

static QmlAppLifeHandler* g_this = NULL;

QmlAppLifeHandler::QmlAppLifeHandler()
{
    g_this = this;
    ApplicationManager::getInstance().EventServiceReady.connect(boost::bind(&QmlAppLifeHandler::onServiceReady, this));
}

QmlAppLifeHandler::~QmlAppLifeHandler()
{
}

void QmlAppLifeHandler::onServiceReady()
{
    LSErrorSafe lserror;
    if (!LSCall(ApplicationManager::getInstance().get(),
                "luna://com.webos.service.bus/signal/addmatch",
                R"({"category":"/booster","method":"processFinished"})",
                onQMLProcessWatcher,
                this,
                NULL,
                &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS(LOG_KEY_TYPE, "lscall"),
                  PMLOGJSON(LOG_KEY_PAYLOAD, R"({"category":"/booster","method":"processFinished"})"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "err: %s", lserror.message);
    }
}

void QmlAppLifeHandler::launch(LaunchAppItemPtr item)
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
    payload.put("main", app_desc->getMain());
    payload.put(LOG_KEY_APPID, item->getAppId());
    payload.put("params", item->getParams().duplicate().stringify());

    LSMessageToken token = 0;
    LSErrorSafe lserror;
    if (!LSCallOneReply(ApplicationManager::getInstance().get(),
                        "luna://com.webos.booster/launch",
                        payload.stringify().c_str(),
                        onReturnBoosterLaunch,
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

    if (item->getPreload().empty())
        EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::LAUNCHING);
    else
        EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::PRELOADING);

    double current_time = getCurrentTime();
    double elapsed_time = current_time - item->launchStartTime();
    LOG_INFO(MSGID_APP_LAUNCHED, 5,
             PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
             PMLOGKS(LOG_KEY_TYPE, "qml"), PMLOGKS("pid", ""),
             PMLOGKFV("start_time", "%f", item->launchStartTime()),
             PMLOGKFV("collapse_time", "%f", elapsed_time), "");

    item->setReturnToken(token);
    m_LSCallRequestList.push_back(item);
}

bool QmlAppLifeHandler::onReturnBoosterLaunch(LSHandle* handle, LSMessage* lsmsg, void* user_data)
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
        // TODO: set proper life status for this error case
        // g_this->signal_app_life_status_changed(item->app_id(), "", RuntimeStatus::STOP);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "booster error");
        g_this->EventLaunchingDone(item->getUid());
        return false;
    }

    if (!jmsg.hasKey("returnValue") || jmsg["returnValue"].asBool() == false) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                  PMLOGKS(LOG_KEY_REASON, "invalid_return"),
                  "invalid_booster_return err: %s", jmsg.stringify().c_str());

        // TODO: set proper life status for this error case
        g_this->EventAppLifeStatusChanged(item->getAppId(), "", RuntimeStatus::STOP);

        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "booster error");
        g_this->EventLaunchingDone(item->getUid());
        return true;
    }

    std::string app_id = jmsg.hasKey(LOG_KEY_APPID) && jmsg[LOG_KEY_APPID].isString() ? jmsg[LOG_KEY_APPID].asString() : "";
    std::string pid = jmsg.hasKey("pid") && jmsg["pid"].isNumber() ? boost::lexical_cast<std::string>(jmsg["pid"].asNumber<int>()) : "";

    if (app_id.empty()) {
        LOG_WARNING(MSGID_APPLAUNCH, 2,
                    PMLOGKS("status", "received_empty_app_id"),
                    PMLOGKS("restoring_app_id", item->getAppId().c_str()), "now restore app_id from sam info");
        app_id = item->getAppId();
    }

    if (pid.empty()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS(LOG_KEY_APPID, item->getAppId().c_str()),
                  PMLOGKS(LOG_KEY_REASON, "received_empty_pid"),
                  "invalid_booster_return err: %s", jmsg.stringify().c_str());

        // TODO: set proper life status for this error case
        g_this->EventAppLifeStatusChanged(item->getAppId(), "", RuntimeStatus::STOP);

        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "booster error");
        g_this->EventLaunchingDone(item->getUid());
        return true;
    }

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
             PMLOGKS("status", "booster_launched_app"), "");

    item->setPid(pid);
    g_this->EventRunningAppAdded(app_id, pid, "");
    g_this->EventAppLifeStatusChanged(app_id, "", RuntimeStatus::RUNNING);

    g_this->EventLaunchingDone(item->getUid());

    return true;
}

void QmlAppLifeHandler::close(CloseAppItemPtr item, std::string& err_text)
{
    pbnjson::JValue payload = pbnjson::Object();
    payload.put(LOG_KEY_APPID, item->getAppId());

    LSErrorSafe lserror;
    if (!LSCallOneReply(ApplicationManager::getInstance().get(),
                        "luna://com.webos.booster/close",
                        payload.stringify().c_str(),
                        onReturnBoosterClose,
                        NULL,
                        NULL,
                        &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS(LOG_KEY_TYPE, "lscallonereply"),
                  PMLOGJSON(LOG_KEY_PAYLOAD, payload.stringify().c_str()),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "err: %s", lserror.message);
        err_text = "close request fail";
    }

    EventAppLifeStatusChanged(item->getAppId(), "", RuntimeStatus::CLOSING);
}

bool QmlAppLifeHandler::onReturnBoosterClose(LSHandle* handle, LSMessage* lsmsg, void* user_data)
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
        std::string app_id = jmsg.hasKey(LOG_KEY_APPID) && jmsg[LOG_KEY_APPID].isString() ? jmsg[LOG_KEY_APPID].asString() : "";
        std::string pid = jmsg.hasKey("pid") && jmsg["pid"].isNumber() ? boost::lexical_cast<std::string>(jmsg["pid"].asNumber<int>()) : "";
        LOG_INFO(MSGID_APPCLOSE, 3,
                 PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
                 PMLOGKS("pid", pid.c_str()),
                 PMLOGKS("status", "received_close_return_from_booster"), "");
    } else {
        std::string err_text = jmsg[LOG_KEY_ERRORTEXT].asString();
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "return_false"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "err: %s", err_text.c_str());
    }

    return true;
}

void QmlAppLifeHandler::pause(const std::string& appId, const pbnjson::JValue& params, std::string& err_text, bool send_life_event)
{
    LOG_ERROR(MSGID_APPPAUSE, 1,
              PMLOGKS(LOG_KEY_APPID, appId.c_str()),
              "no interface defined for qml booster");
    err_text = "no interface defined for qml booster";
    return;
}

bool QmlAppLifeHandler::onQMLProcessWatcher(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    LOG_INFO(MSGID_APPCLOSE, 1,
             PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
             "received closed event");

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS(LOG_KEY_REASON, "parsing_fail"),
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  "internal error");
        return true;
    }
    if (!jmsg.hasKey("pid") || !jmsg["pid"].isNumber()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                  PMLOGJSON(LOG_KEY_PAYLOAD, jmsg.stringify().c_str()),
                  "invalid pid info");
        return true;
    }

    std::string pid = boost::lexical_cast<std::string>(jmsg["pid"].asNumber<int>());
    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfoByPid(pid);
    if (runningInfoPtr == nullptr) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS(LOG_KEY_APPID, "empty"),
                  PMLOGKS("pid", pid.c_str()), "err: no app_id matched by pid");
        return true;
    }

    LOG_INFO(MSGID_APP_CLOSED, 3,
             PMLOGKS(LOG_KEY_APPID, runningInfoPtr->getAppId().c_str()),
             PMLOGKS(LOG_KEY_TYPE, "qml"),
             PMLOGKS("pid", pid.c_str()), "");

    g_this->EventAppLifeStatusChanged(runningInfoPtr->getAppId(), "", RuntimeStatus::STOP);
    g_this->EventRunningAppRemoved(runningInfoPtr->getAppId());

    return true;
}

LaunchAppItemPtr QmlAppLifeHandler::getLSCallRequestItemByToken(const LSMessageToken& token)
{
    auto it = m_LSCallRequestList.begin();
    auto it_end = m_LSCallRequestList.end();
    for (; it != it_end; ++it) {
        if ((*it)->returnToken() == token)
            return *it;
    }
    return NULL;
}

void QmlAppLifeHandler::removeItemFromLSCallRequestList(const std::string& uid)
{
    auto it = std::find_if(m_LSCallRequestList.begin(), m_LSCallRequestList.end(), [&uid](LaunchAppItemPtr item) {return (item->getUid() == uid);});
    if (it == m_LSCallRequestList.end())
        return;

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS(LOG_KEY_APPID, (*it)->getAppId().c_str()),
             PMLOGKS("uid", uid.c_str()),
             "removed from checking queue");

    m_LSCallRequestList.erase(it);
}

