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

#include "bus/client/WAM.h"

#include "base/LaunchPointList.h"
#include "base/LunaTaskList.h"
#include "base/RunningAppList.h"

const std::string INVALID_PROCESS_ID = "-1";
const std::string PROCESS_ID_ZERO = "0";

bool WAM::onListRunningApps(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    pbnjson::JValue subscriptionPayload = JDomParser::fromString(response.getPayload());
    Logger::logSubscriptionResponse(getInstance().getClassName(), __FUNCTION__, response, subscriptionPayload);

    if (subscriptionPayload.isNull())
        return true;

    pbnjson::JValue curRunning = pbnjson::Array();
    JValueUtil::getValue(subscriptionPayload, "running", curRunning);

    int prevLenth = getInstance().m_prevRunning.arraySize();
    for (int i = 0; i < prevLenth; i++) {
        getInstance().m_prevRunning[i].put("lifeStatue", "stop");
    }

    int curLenth = curRunning.arraySize();
    for (int i = 0; i < curLenth; i++) {
        for (int j = 0; j < prevLenth; j++) {
            if (curRunning[i]["id"].asString() == getInstance().m_prevRunning[j]["id"].asString()) {
                if (curRunning[i]["webprocessid"].asString() == getInstance().m_prevRunning[j]["webprocessid"].asString()) {
                    getInstance().m_prevRunning[j].put("lifeStatue", "running");
                    curRunning[i].put("lifeStatue", "running");
                } else {
                    getInstance().m_prevRunning[j].put("lifeStatue", "restart");
                    curRunning[i].put("lifeStatue", "restart");
                }
            }
        }
        if (!curRunning[i].hasKey("lifeStatue")) {
            curRunning[i].put("lifeStatue", "start");
        }
    }


    string lifeStatue;
    string appId;
    string webprocessid;
    for (int i = 0; i < prevLenth; i++) {
        JValueUtil::getValue(getInstance().m_prevRunning[i], "lifeStatue", lifeStatue);
        JValueUtil::getValue(getInstance().m_prevRunning[i], "id", appId);
        JValueUtil::getValue(getInstance().m_prevRunning[i], "webprocessid", webprocessid);
        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, Logger::format("lifeStatue(%s) webprocessid(%s)", lifeStatue.c_str(), webprocessid.c_str()));

        if (lifeStatue == "stop") {
            // EventAppLifeStatusChanged(oldAppId, "", RuntimeStatus::STOP);
        } else if (lifeStatue == "restart") {
            // if ((oldWebprocessId != INVALID_PROCESS_ID) && (oldWebprocessId != PROCESS_ID_ZERO))
            // EventRunningAppRemoved(oldAppId);
        }
    }

    for (int i = 0; i < curLenth; i++) {
        JValueUtil::getValue(curRunning[i], "lifeStatue", lifeStatue);
        JValueUtil::getValue(curRunning[i], "id", appId);
        JValueUtil::getValue(curRunning[i], "webprocessid", webprocessid);
        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, Logger::format("lifeStatue(%s) webprocessid(%s)", lifeStatue.c_str(), webprocessid.c_str()));

        if (lifeStatue == "start") {
            RunningAppPtr running = RunningAppList::getInstance().getByAppId(appId, true);
            // EventRunningAppAdded(newAppId, newWebprocessId, newWebprocessId);
            // removeLoadingApp(newAppId);
        } else if (lifeStatue == "restart") {
            // EventAppLifeStatusChanged(newAppId, "", RuntimeStatus::RUNNING);
        }
    }

    getInstance().m_prevRunning = curRunning;
    return true;
}

WAM::WAM()
    : AbsLunaClient("com.palm.webappmanager")
{
    setClassName("WAM");
    m_prevRunning = pbnjson::Array();
}

WAM::~WAM()
{
}

void WAM::onInitialze()
{

}

void WAM::onServerStatusChanged(bool isConnected)
{
    static string method = std::string("luna://") + getName() + std::string("/listRunningApps");
    if (isConnected) {
        JValue requestPayload = pbnjson::Object();
        requestPayload.put("includeSysApps", true);
        requestPayload.put("subscribe", true);

        m_listRunningAppsCall = ApplicationManager::getInstance().callMultiReply(
            method.c_str(),
            AbsLunaClient::getSubscriptionPayload().stringify().c_str(),
            onListRunningApps,
            this
        );
    } else {
        if (m_listRunningAppsCall.isActive())
            m_listRunningAppsCall.cancel();
    }
}

bool WAM::onDiscardCodeCache(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    WAM::getInstance().m_discardCodeCacheCall.cancel();
    return true;
}

void WAM::discardCodeCache()
{
    static string method = std::string("luna://") + getName() + std::string("/discardCodeCache");

    if (m_discardCodeCacheCall.isActive())
        return;

    m_discardCodeCacheCall = ApplicationManager::getInstance().callOneReply(
        method.c_str(),
        "{\"force\":true}",
        onDiscardCodeCache,
        nullptr
    );
}

bool WAM::onLaunchApp(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    LSMessageToken token = LSMessageGetResponseToken(message);
    LunaTaskPtr lunaTask = LunaTaskList::getInstance().getByToken(token);

    if (lunaTask == nullptr) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "null_launching_item");
        return false;
    }

    string appId = lunaTask->getAppId();
    string procId = "";
    bool returnValue = true;

    JValueUtil::getValue(responsePayload, "appId", appId);
    JValueUtil::getValue(responsePayload, "procId", procId);
    JValueUtil::getValue(responsePayload, "returnValue", returnValue);

    if (!returnValue) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, appId, "Failed to launch webapp");
        lunaTask->reply(APP_LAUNCH_ERR_GENERAL, "WebAppMgr's launchApp is failed");
        LunaTaskList::getInstance().remove(lunaTask);
        // WebAppLifeHandler::getInstance().EventAppLifeStatusChanged(appId, "", RuntimeStatus::STOP);
        return true;
    }

    Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "received_launch_return_from_wam");
    lunaTask->setPid(procId);
    lunaTask->reply();
    LunaTaskList::getInstance().remove(lunaTask);
    return true;
}

bool WAM::launchApp(LunaTaskPtr lunaTask)
{
    static string method = std::string("luna://") + getName() + std::string("/launchApp");
    JValue requestPayload = pbnjson::Object();
    LSMessageToken token = 0;

    if (!isConnected()) {
        Logger::warning(getClassName(), __FUNCTION__, "WAM is not running");
        return false;
    }

    JValue appinfo = pbnjson::Object();
    LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByAppId(lunaTask->getAppId());
    launchPoint->toJson(appinfo);

    requestPayload.put("appDesc", appinfo);
    requestPayload.put("reason", lunaTask->getReason());
    requestPayload.put("parameters", lunaTask->getParams());
    requestPayload.put("launchingAppId", lunaTask->getCallerId());
    requestPayload.put("launchingProcId", lunaTask->getCallerPid());
    requestPayload.put("keepAlive", lunaTask->isKeepAlive());

    if (!lunaTask->getPreload().empty())
        requestPayload.put("preload", lunaTask->getPreload());

    Logger::logAPIRequest(getClassName(), __FUNCTION__, lunaTask->getRequest(), requestPayload);
    if (!LSCallOneReply(
        lunaTask->getHandle(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onLaunchApp,
        nullptr,
        &token,
        nullptr)
    ) {
        return false;
    }
    lunaTask->setToken(token);
//    if (lunaTask->getPreload().empty())
//        EventAppLifeStatusChanged(lunaTask->getAppId(), lunaTask->getInstanceId(), RuntimeStatus::LAUNCHING);
//    else
//        EventAppLifeStatusChanged(lunaTask->getAppId(), lunaTask->getInstanceId(), RuntimeStatus::PRELOADING);
//
//    lunaTask->stopTime();
//    Logger::info(getClassName(), __FUNCTION__, lunaTask->getAppId(), Logger::format("elapsed_time(%f)", lunaTask->getTotalTime()));
    return true;
}

bool WAM::onKillApp(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    LSMessageToken token = LSMessageGetResponseToken(message);
    LunaTaskPtr lunaTask = LunaTaskList::getInstance().getByToken(token);

    if (lunaTask == nullptr) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "null_launching_item");
        return false;
    }

    string appId = lunaTask->getAppId();
    string procId = "";
    bool returnValue = true;

    JValueUtil::getValue(responsePayload, "appId", appId);
    JValueUtil::getValue(responsePayload, "processId", procId);
    JValueUtil::getValue(responsePayload, "returnValue", returnValue);

    if (!returnValue) {
        string errorText = "Unknown Error";
        JValueUtil::getValue(responsePayload, "errorText", errorText);
        lunaTask->reply(Logger::ERRCODE_GENERAL, errorText);
        return true;
    }

    Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "received_close_return_from_wam");
    // How could we get the previous status? We should restore it.
    // WebAppLifeHandler::getInstance().signal_app_life_status_changed(appId(), "", RuntimeStatus::STOP);
    lunaTask->reply();
    return true;
}

bool WAM::killApp(LunaTaskPtr lunaTask)
{
    static string method = std::string("luna://") + getName() + std::string("/killApp");
    JValue requestPayload = pbnjson::Object();
    LSMessageToken token = 0;

    if (!isConnected()) {
        Logger::warning(getClassName(), __FUNCTION__, "WAM is not running");
        return false;
    }

    requestPayload.put("appId", lunaTask->getAppId());
    requestPayload.put("reason", lunaTask->getReason());

    LSErrorSafe error;
    bool result = true;
    Logger::logAPIRequest(getClassName(), __FUNCTION__, lunaTask->getRequest(), requestPayload);
    result = LSCallOneReply(
        lunaTask->getHandle(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onKillApp,
        nullptr,
        &token,
        &error
    );
    if (!result) {
        lunaTask->reply(error.error_code, error.message);
        return false;
    }
    lunaTask->setToken(token);
    //EventAppLifeStatusChanged(lunaTask->getAppId(), "", RuntimeStatus::CLOSING);
    return true;
}
