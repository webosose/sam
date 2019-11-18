// Copyright (c) 2019 LG Electronics, Inc.
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

#include "Booster.h"

#include <boost/lexical_cast.hpp>

#include "base/RunningAppList.h"
#include "base/LaunchPointList.h"
#include "base/LunaTaskList.h"
#include "bus/service/ApplicationManager.h"
#include "util/Logger.h"

const std::string Booster::NAME = "com.webos.booster";

Booster::Booster()
    : AbsLunaClient(NAME)
{
    setClassName("Booster");
}

Booster::~Booster()
{

}

void Booster::onInitialze()
{
    processFinished();
}

void Booster::onServerStatusChanged(bool isConnected)
{
}

bool Booster::onProcessFinished(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    pbnjson::JValue subscriptionPayload = JDomParser::fromString(response.getPayload());
    Logger::logSubscriptionResponse(getInstance().getClassName(), __FUNCTION__, response, subscriptionPayload);

    if (subscriptionPayload.isNull())
        return true;

    int pid = -1;
    JValueUtil::getValue(subscriptionPayload, "pid", pid);
    if (pid < 0) {
        Logger::info(getInstance().getClassName(), __FUNCTION__, "No valid pid");
        return true;
    }

    std::string pidstr = boost::lexical_cast<std::string>(subscriptionPayload["pid"].asNumber<int>());
    RunningAppPtr runningApp = RunningAppList::getInstance().getByPid(pidstr);
    if (runningApp == nullptr) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "Cannot find running app");
        return true;
    }

    // booster앱을 여기서 제거해야함.
    // QmlAppLifeHandler::getInstance().EventAppLifeStatusChanged(runningApp->getInstanceId(), "", RuntimeStatus::STOP);
    // QmlAppLifeHandler::getInstance().EventRunningAppRemoved(runningApp->getInstanceId());

    return true;
}

void Booster::processFinished()
{
    JValue requestPayload = pbnjson::Object();
    requestPayload.put("category", "/booster");
    requestPayload.put("method", "processFinished");

    m_call = ApplicationManager::getInstance().callMultiReply(
        "luna://com.webos.service.bus/signal/addmatch",
        requestPayload.stringify().c_str(),
        onProcessFinished,
        nullptr
    );
}

bool Booster::onLaunch(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    LunaTaskPtr lunaTask = LunaTaskList::getInstance().getByToken(response.getResponseToken());
    if (lunaTask == NULL) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "null_launching_item");
        return false;
    }

    if (responsePayload.isNull()) {
        // TODO: set proper life status for this error case
        // QmlAppLifeHandler::getInstance().signal_app_life_status_changed(item->appId(), "", RuntimeStatus::STOP);
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "booster error");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }

    if (!responsePayload.hasKey("returnValue") || responsePayload["returnValue"].asBool() == false) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, lunaTask->getAppId(), responsePayload.stringify());

        // TODO: set proper life status for this error case
        // QmlAppLifeHandler::getInstance().EventAppLifeStatusChanged(lunaTask->getAppId(), "", RuntimeStatus::STOP);

        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "booster error");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }

    std::string appId = responsePayload.hasKey("appId") && responsePayload["appId"].isString() ? responsePayload["appId"].asString() : "";
    std::string pid = responsePayload.hasKey("pid") && responsePayload["pid"].isNumber() ? boost::lexical_cast<std::string>(responsePayload["pid"].asNumber<int>()) : "";

    if (appId.empty()) {
        Logger::warning(getInstance().getClassName(), __FUNCTION__, lunaTask->getAppId(), "now restore appId from sam info");
        appId = lunaTask->getAppId();
    }

    if (pid.empty()) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, lunaTask->getAppId(), responsePayload.stringify());

        // TODO: set proper life status for this error case
//        QmlAppLifeHandler::getInstance().EventAppLifeStatusChanged(lunaTask->getAppId(), "", RuntimeStatus::STOP);

        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "booster error");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }

    Logger::error(getInstance().getClassName(), __FUNCTION__, appId, "booster_launched_app");
    lunaTask->setPid(pid);

//    QmlAppLifeHandler::getInstance().EventRunningAppAdded(appId, pid, "");
//    QmlAppLifeHandler::getInstance().EventAppLifeStatusChanged(appId, "", RuntimeStatus::RUNNING);

    lunaTask->getResponsePayload().put("returnValue", true);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
    return true;
}

bool Booster::launch(LunaTaskPtr lunaTask)//LSFilterFunc func, const string& appId, const string& main, const JValue& params)
{
    static string method = std::string("luna://") + getName() + std::string("/launch");
    JValue requestPayload = pbnjson::Object();

    LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByAppId(lunaTask->getAppId());
    requestPayload.put("appId", lunaTask->getAppId());
    requestPayload.put("main", launchPoint->getAppDesc()->getAbsMain());
    requestPayload.put("params", lunaTask->getParams().stringify());

    LSErrorSafe error;
    bool result = true;
    result = LSCallOneReply(
        lunaTask->getHandle(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onLaunch,
        nullptr,
        nullptr,
        &error
    );
    if (!result) {
        lunaTask->setErrCodeAndText(error.error_code, error.message);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return false;
    }

//    if (lunaTask->getPreload().empty())
//        EventAppLifeStatusChanged(lunaTask->getAppId(), lunaTask->getInstanceId(), RuntimeStatus::LAUNCHING);
//    else
//        EventAppLifeStatusChanged(lunaTask->getAppId(), lunaTask->getInstanceId(), RuntimeStatus::PRELOADING);
//
//    lunaTask->stopTime();
//    Logger::info(LOG_NAME, __FUNCTION__, lunaTask->getAppId(), Logger::format("elapsed_time(%f)", lunaTask->getTotalTime()));
//
//    // xxx lunaTask->setToken(token);
    return true;
}

bool Booster::onClose(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    if (responsePayload.isNull())
        return true;

    if (responsePayload["returnValue"].asBool()) {
        std::string appId = responsePayload.hasKey("appId") && responsePayload["appId"].isString() ? responsePayload["appId"].asString() : "";
        std::string pid = responsePayload.hasKey("pid") && responsePayload["pid"].isNumber() ? boost::lexical_cast<std::string>(responsePayload["pid"].asNumber<int>()) : "";

        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "received_close_return_from_booster");
    } else {
        std::string errorText = responsePayload["errorText"].asString();
        Logger::error(getInstance().getClassName(), __FUNCTION__, errorText);
    }

    return true;
}

bool Booster::close(LunaTaskPtr lunaTask)
{
    static string method = std::string("luna://") + getName() + std::string("/close");
    JValue requestPayload = pbnjson::Object();
    LSMessageToken token = 0;

    if (!isConnected()) {
        Logger::warning(getClassName(), __FUNCTION__, "Booster is not running");
        return false;
    }

    requestPayload.put("appId", lunaTask->getAppId());

    LSErrorSafe error;
    bool result = true;
    Logger::logAPIRequest(getClassName(), __FUNCTION__, lunaTask->getRequest(), requestPayload);
    result = LSCallOneReply(
        lunaTask->getHandle(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onClose,
        nullptr,
        &token,
        &error
    );
    if (!result) {
        lunaTask->setErrCodeAndText(error.error_code, error.message);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return false;
    }
    lunaTask->setToken(token);

    // EventAppLifeStatusChanged(lunaTask->getAppId(), "", RuntimeStatus::CLOSING);
    return true;
}

bool Booster::pause(LunaTaskPtr lunaTask)
{
    lunaTask->setErrCodeAndText(ErrCode_GENERAL, "no interface defined for qml booster");
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
    return false;
}
