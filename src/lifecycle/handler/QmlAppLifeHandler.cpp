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

#include <boost/lexical_cast.hpp>
#include <boost/signals2.hpp>
#include <bus/service/ApplicationManager.h>
#include <bus/service/ApplicationManager.h>
#include <lifecycle/handler/QmlAppLifeHandler.h>
#include <lifecycle/RunningInfoManager.h>
#include <package/AppPackageManager.h>
#include <util/Time.h>
#include <util/LSUtils.h>

const string QmlAppLifeHandler::LOG_NAME = "QmlAppLifeHandler";

QmlAppLifeHandler::QmlAppLifeHandler()
{
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
        Logger::error(LOG_NAME, __FUNCTION__, "lscall", lserror.message);
    }
}

void QmlAppLifeHandler::launch(LaunchAppItemPtr item)
{
    AppPackagePtr app_desc = AppPackageManager::getInstance().getAppById(item->getAppId());
    if (app_desc == NULL) {
        Logger::error(LOG_NAME, __FUNCTION__, item->getAppId(), "null_description");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        EventLaunchingDone(item->getUid());
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("main", app_desc->getMain());
    payload.put(Logger::LOG_KEY_APPID, item->getAppId());
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
        Logger::error(LOG_NAME, __FUNCTION__, item->getAppId(), lserror.message);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        EventLaunchingDone(item->getUid());
        return;
    }

    if (item->getPreload().empty())
        EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::LAUNCHING);
    else
        EventAppLifeStatusChanged(item->getAppId(), item->getUid(), RuntimeStatus::PRELOADING);

    double current_time = Time::getCurrentTime();
    double elapsed_time = current_time - item->launchStartTime();
    Logger::info(LOG_NAME, __FUNCTION__, item->getAppId(), Logger::format("elapsed_time(%f)", elapsed_time));

    item->setReturnToken(token);
    m_LSCallRequestList.push_back(item);
}

bool QmlAppLifeHandler::onReturnBoosterLaunch(LSHandle* sh, LSMessage* message, void* context)
{
    LSMessageToken token = 0;
    std::string uid = "";
    LaunchAppItemPtr item = NULL;

    token = LSMessageGetResponseToken(message);
    item = QmlAppLifeHandler::getInstance().getLSCallRequestItemByToken(token);

    if (item == NULL) {
        Logger::error(LOG_NAME, __FUNCTION__, "null_launching_item");
        return false;
    }

    uid = item->getUid();
    QmlAppLifeHandler::getInstance().removeItemFromLSCallRequestList(uid);
    item->resetReturnToken();

    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull()) {
        Logger::error(LOG_NAME, __FUNCTION__, "parsing_fail");

        // TODO: set proper life status for this error case
        // QmlAppLifeHandler::getInstance().signal_app_life_status_changed(item->appId(), "", RuntimeStatus::STOP);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "booster error");
        QmlAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());
        return false;
    }

    if (!responsePayload.hasKey("returnValue") || responsePayload["returnValue"].asBool() == false) {
        Logger::error(LOG_NAME, __FUNCTION__, item->getAppId(), responsePayload.stringify());

        // TODO: set proper life status for this error case
        QmlAppLifeHandler::getInstance().EventAppLifeStatusChanged(item->getAppId(), "", RuntimeStatus::STOP);

        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "booster error");
        QmlAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());
        return true;
    }

    std::string appId = responsePayload.hasKey(Logger::LOG_KEY_APPID) && responsePayload[Logger::LOG_KEY_APPID].isString() ? responsePayload[Logger::LOG_KEY_APPID].asString() : "";
    std::string pid = responsePayload.hasKey("pid") && responsePayload["pid"].isNumber() ? boost::lexical_cast<std::string>(responsePayload["pid"].asNumber<int>()) : "";

    if (appId.empty()) {
        Logger::warning(LOG_NAME, __FUNCTION__, item->getAppId(), "now restore appId from sam info");
        appId = item->getAppId();
    }

    if (pid.empty()) {
        Logger::error(LOG_NAME, __FUNCTION__, item->getAppId(), responsePayload.stringify());

        // TODO: set proper life status for this error case
        QmlAppLifeHandler::getInstance().EventAppLifeStatusChanged(item->getAppId(), "", RuntimeStatus::STOP);

        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "booster error");
        QmlAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());
        return true;
    }

    Logger::error(LOG_NAME, __FUNCTION__, appId, "booster_launched_app");
    item->setPid(pid);
    QmlAppLifeHandler::getInstance().EventRunningAppAdded(appId, pid, "");
    QmlAppLifeHandler::getInstance().EventAppLifeStatusChanged(appId, "", RuntimeStatus::RUNNING);

    QmlAppLifeHandler::getInstance().EventLaunchingDone(item->getUid());

    return true;
}

void QmlAppLifeHandler::close(CloseAppItemPtr item, std::string& errorText)
{
    pbnjson::JValue payload = pbnjson::Object();
    payload.put(Logger::LOG_KEY_APPID, item->getAppId());

    LSErrorSafe lserror;
    if (!LSCallOneReply(ApplicationManager::getInstance().get(),
                        "luna://com.webos.booster/close",
                        payload.stringify().c_str(),
                        onReturnBoosterClose,
                        NULL,
                        NULL,
                        &lserror)) {
        Logger::error(LOG_NAME, __FUNCTION__, "lscallonereply", lserror.message);
        errorText = "close request fail";
    }

    EventAppLifeStatusChanged(item->getAppId(), "", RuntimeStatus::CLOSING);
}

bool QmlAppLifeHandler::onReturnBoosterClose(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull()) {
        Logger::error(LOG_NAME, __FUNCTION__, "parsing_fail");
        return false;
    }

    if (responsePayload["returnValue"].asBool()) {
        std::string appId = responsePayload.hasKey(Logger::LOG_KEY_APPID) && responsePayload[Logger::LOG_KEY_APPID].isString() ? responsePayload[Logger::LOG_KEY_APPID].asString() : "";
        std::string pid = responsePayload.hasKey("pid") && responsePayload["pid"].isNumber() ? boost::lexical_cast<std::string>(responsePayload["pid"].asNumber<int>()) : "";

        Logger::info(LOG_NAME, __FUNCTION__, appId, "received_close_return_from_booster");
    } else {
        std::string errorText = responsePayload[Logger::LOG_KEY_ERRORTEXT].asString();
        Logger::error(LOG_NAME, __FUNCTION__, errorText);
    }

    return true;
}

void QmlAppLifeHandler::pause(const std::string& appId, const pbnjson::JValue& params, std::string& errorText, bool send_life_event)
{
    errorText = "no interface defined for qml booster";
    Logger::error(LOG_NAME, __FUNCTION__, appId, errorText);
    return;
}

bool QmlAppLifeHandler::onQMLProcessWatcher(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull()) {
        Logger::error(LOG_NAME, __FUNCTION__, "parsing_fail");
        return true;
    }
    if (!responsePayload.hasKey("pid") || !responsePayload["pid"].isNumber()) {
        Logger::error(LOG_NAME, __FUNCTION__, "invalid pid info", responsePayload.stringify());
        return true;
    }

    std::string pid = boost::lexical_cast<std::string>(responsePayload["pid"].asNumber<int>());
    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfoByPid(pid);
    if (runningInfoPtr == nullptr) {
        Logger::error(LOG_NAME, __FUNCTION__, "err: no appId matched by pid");
        return true;
    }

    Logger::info(LOG_NAME, __FUNCTION__, runningInfoPtr->getAppId());
    QmlAppLifeHandler::getInstance().EventAppLifeStatusChanged(runningInfoPtr->getAppId(), "", RuntimeStatus::STOP);
    QmlAppLifeHandler::getInstance().EventRunningAppRemoved(runningInfoPtr->getAppId());

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

    Logger::info(LOG_NAME, __FUNCTION__, (*it)->getAppId(), "removed from checking queue");
    m_LSCallRequestList.erase(it);
}

