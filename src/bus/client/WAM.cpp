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

const string INVALID_PROCESS_ID = "-1";
const string PROCESS_ID_ZERO = "0";

bool WAM::onListRunningApps(LSHandle* sh, LSMessage* message, void* context)
{
    static const string STATUS = "status";

    Message response(message);
    pbnjson::JValue subscriptionPayload = JDomParser::fromString(response.getPayload());
    Logger::logSubscriptionResponse(getInstance().getClassName(), __FUNCTION__, response, subscriptionPayload);

    if (subscriptionPayload.isNull())
        return true;

    static pbnjson::JValue prevRunning = pbnjson::Array();
    pbnjson::JValue currRunning = pbnjson::Array();
    JValueUtil::getValue(subscriptionPayload, "running", currRunning);

    int prevSize = prevRunning.arraySize();
    for (int i = 0; i < prevSize; i++) {
        prevRunning[i].put(STATUS, "stop");
    }

    int curSize = currRunning.arraySize();
    for (int i = 0; i < curSize; i++) {
        for (int j = 0; j < prevSize; j++) {
            if (currRunning[i]["id"].asString() == prevRunning[j]["id"].asString()) {
                prevRunning[j].put(STATUS, "running");
                currRunning[i].put(STATUS, "running");
            }
        }
        if (!currRunning[i].hasKey(STATUS)) {
            currRunning[i].put(STATUS, "start");
        }
    }

    string status;
    string instanceId;
    string launchPointId;
    string appId;
    string webprocessid;

    for (int i = 0; i < prevSize; i++) {
        JValueUtil::getValue(prevRunning[i], "instanceId", instanceId);
        JValueUtil::getValue(prevRunning[i], "launchPointId", launchPointId);
        JValueUtil::getValue(prevRunning[i], "id", appId);

        JValueUtil::getValue(prevRunning[i], STATUS, status);
        JValueUtil::getValue(prevRunning[i], "webprocessid", webprocessid);

        RunningAppPtr runningApp = RunningAppList::getInstance().getByIds(instanceId, launchPointId, appId);
        if (runningApp == nullptr) {
            Logger::error(getInstance().getClassName(), __FUNCTION__, appId, "Cannot find RunningApp");
            continue;
        }
        Logger::debug(getInstance().getClassName(), __FUNCTION__, appId,
                      Logger::format("Previous: %s(%s) webprocessid(%s)", STATUS.c_str(), status.c_str(), webprocessid.c_str()));
        if (status == "running") {
            runningApp->setWebprocid(webprocessid);
        } else {
            RunningAppList::getInstance().removeByObject(runningApp);
        }
    }

    for (int i = 0; i < curSize; i++) {
        JValueUtil::getValue(currRunning[i], "instanceId", instanceId);
        JValueUtil::getValue(currRunning[i], "launchPointId", launchPointId);
        JValueUtil::getValue(currRunning[i], "id", appId);

        JValueUtil::getValue(currRunning[i], STATUS, status);
        JValueUtil::getValue(currRunning[i], "webprocessid", webprocessid);

        RunningAppPtr runningApp = RunningAppList::getInstance().getByIds(instanceId, launchPointId, appId);
        Logger::debug(getInstance().getClassName(), __FUNCTION__, appId,
                      Logger::format("Current: %s(%s) webprocessid(%s)", STATUS.c_str(), status.c_str(), webprocessid.c_str()));
        if (status == "start") {
            // SAM might be restarted (respwaned)
            if (runningApp == nullptr) {
                Logger::warning(getInstance().getClassName(), __FUNCTION__, "SAM might be restarted. RunningApp is created by WAM");
                runningApp = RunningAppList::getInstance().createByAppId(appId);
                runningApp->setLifeStatus(LifeStatus::LifeStatus_BACKGROUND);
                RunningAppList::getInstance().add(runningApp);
            }
        }
        runningApp->setWebprocid(webprocessid);
    }

    prevRunning = currRunning.duplicate();
    return true;
}

WAM::WAM()
    : AbsLunaClient("com.palm.webappmanager")
{
    setClassName("WAM");
}

WAM::~WAM()
{
}

void WAM::onInitialze()
{

}

void WAM::onServerStatusChanged(bool isConnected)
{
    static string method = string("luna://") + getName() + string("/listRunningApps");
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

        LunaTaskList::getInstance().removeAboutWAM();
    }
}

bool WAM::onDiscardCodeCache(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    if (responsePayload.isNull())
        return true;

    WAM::getInstance().m_discardCodeCacheCall.cancel();
    return true;
}

void WAM::discardCodeCache()
{
    static string method = string("luna://") + getName() + string("/discardCodeCache");

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
        Logger::error(getInstance().getClassName(), __FUNCTION__, "Cannot find lunaTask");
        return false;
    }

    string instanceId = "";
    string launchPointId = "";
    string appId = "";
    string procId = "";
    bool returnValue = true;

    JValueUtil::getValue(responsePayload, "instanceId", instanceId);
    JValueUtil::getValue(responsePayload, "launchPointId", launchPointId);
    JValueUtil::getValue(responsePayload, "appId", appId);
    JValueUtil::getValue(responsePayload, "procId", procId);
    JValueUtil::getValue(responsePayload, "returnValue", returnValue);

    if (!returnValue) {
        RunningAppList::getInstance().removeByIds(instanceId, launchPointId, appId);
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Failed to launch webapp in WAM");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByIds(instanceId, launchPointId, appId);
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Cannot find RunningApp");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }
    runningApp->setProcessId(procId);

    lunaTask->getResponsePayload().put("returnValue", true);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
    Logger::info(getInstance().getClassName(), __FUNCTION__, appId, Logger::format("Launched TotalTime(%f)", lunaTask->getTimeStampMS()));
    return true;
}

bool WAM::launchApp(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    static string method = string("luna://") + getName() + string("/launchApp");
    JValue requestPayload = pbnjson::Object();
    LSMessageToken token = 0;

    if (!isConnected()) {
        Logger::info(getClassName(), __FUNCTION__, "WAM is not running. Push LSCall to queue...");
    }

    JValue appDesc = pbnjson::Object();
    runningApp.getLaunchPoint()->toJson(appDesc);

    requestPayload.put("appDesc", appDesc);
    requestPayload.put("reason", lunaTask->getReason());
    requestPayload.put("parameters", lunaTask->getParams());
    requestPayload.put("launchingAppId", lunaTask->getCaller(true));
    requestPayload.put("launchingProcId", "");

    if (runningApp.isKeepAlive()) {
        requestPayload.put("keepAlive", true);
    }
    if (!runningApp.getPreload().empty()) {
        requestPayload.put("preload", runningApp.getPreload());
    }

    LSErrorSafe error;
    bool result = true;
    Logger::logAPIRequest(getClassName(), __FUNCTION__, lunaTask->getRequest(), requestPayload);
    result = LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onLaunchApp,
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
    return true;
}

bool WAM::close(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    string sender = lunaTask->getCaller(true);
    if (sender != "com.webos.service.memorymanager" &&
        sender != "com.webos.lunasend" &&
        runningApp.isKeepAlive()) {
        return pauseApp(runningApp, lunaTask);
    } else {
        return killApp(runningApp, lunaTask);
    }
}

bool WAM::onPauseApp(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    LSMessageToken token = LSMessageGetResponseToken(message);
    LunaTaskPtr lunaTask = LunaTaskList::getInstance().getByToken(token);
    if (lunaTask == nullptr) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "Failed to get lunaTask");
        return false;
    }

    string instanceId = "";
    string launchPointId = "";
    string appId = "";
    bool returnValue = true;

    JValueUtil::getValue(responsePayload, "instanceId", instanceId);
    JValueUtil::getValue(responsePayload, "launchPointId", launchPointId);
    JValueUtil::getValue(responsePayload, "appId", appId);
    JValueUtil::getValue(responsePayload, "returnValue", returnValue);

    if (!returnValue) {
        RunningAppList::getInstance().removeByIds(instanceId, launchPointId, appId);
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Failed to pause webapp in WAM");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByIds(instanceId, launchPointId, appId);
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Cannot find RunningApp");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }

    lunaTask->getResponsePayload().put("returnValue", true);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
    return true;
}

bool WAM::pauseApp(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    static string method = string("luna://") + getName() + string("/pauseApp");

    if (!isConnected()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "WAM is not running. The app is not exist");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }

    runningApp.setLifeStatus(LifeStatus::LifeStatus_PAUSING);
    JValue requestPayload = pbnjson::Object();
    requestPayload.put("appId", runningApp.getAppId());
    requestPayload.put("reason", "pause");
    requestPayload.put("parameters", lunaTask->getParams());

    LSErrorSafe error;
    bool result = true;
    LSMessageToken token = 0;
    Logger::logAPIRequest(getClassName(), __FUNCTION__, lunaTask->getRequest(), requestPayload);
    result = LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onPauseApp,
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
        Logger::error(getInstance().getClassName(), __FUNCTION__, "Failed to get lunaTask");
        return false;
    }

    string procId = "";
    bool returnValue = true;

    JValueUtil::getValue(responsePayload, "processId", procId);
    JValueUtil::getValue(responsePayload, "returnValue", returnValue);

    if (!returnValue) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Failed to killApp in WAM");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }

    // How could we get the previous status? We should restore it.
    RunningAppPtr runningApp = RunningAppList::getInstance().getByLunaTask(lunaTask);
    if (runningApp == nullptr) {
        Logger::info(getInstance().getClassName(), __FUNCTION__, "The RunningApp is already removed in 'listRunning'");
    } else {
        RunningAppList::getInstance().removeByObject(runningApp);
    }
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
    return true;
}

bool WAM::killApp(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    static string method = string("luna://") + getName() + string("/killApp");
    JValue requestPayload = pbnjson::Object();
    LSMessageToken token = 0;

    if (!isConnected()) {
        Logger::warning(getClassName(), __FUNCTION__, "WAM is not running. The app is not exist");
        lunaTask->getResponsePayload().put("returnValue", true);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }

    runningApp.setLifeStatus(LifeStatus::LifeStatus_CLOSING);
    requestPayload.put("instanceId", runningApp.getInstanceId());
    requestPayload.put("launchPointId", runningApp.getLaunchPointId());
    requestPayload.put("appId", runningApp.getAppId());
    requestPayload.put("reason", lunaTask->getReason());

    LSErrorSafe error;
    bool result = true;
    Logger::logAPIRequest(getClassName(), __FUNCTION__, lunaTask->getRequest(), requestPayload);
    result = LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onKillApp,
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
    return true;
}
