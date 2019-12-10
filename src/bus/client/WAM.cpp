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

bool WAM::onListRunningApps(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue subscriptionPayload = JDomParser::fromString(response.getPayload());
    Logger::logSubscriptionResponse(getInstance().getClassName(), __FUNCTION__, response, subscriptionPayload);

    if (subscriptionPayload.isNull())
        return true;

    JValue running = pbnjson::Array();
    string instanceId = "";
    string appId = "";
    string webprocessid = "";

    RunningAppList::getInstance().setConext(AppType::AppType_Web, CONTEXT_STOP);
    JValueUtil::getValue(subscriptionPayload, "running", running);
    int size = running.arraySize();
    for (int i = 0; i < size; i++) {
        JValueUtil::getValue(running[i], "id", appId);
        JValueUtil::getValue(running[i], "instanceid", instanceId);
        JValueUtil::getValue(running[i], "webprocessid", webprocessid);

        RunningAppPtr runningApp = RunningAppList::getInstance().getByIds(instanceId, "", appId);
        if (runningApp == nullptr) {
            Logger::warning(getInstance().getClassName(), __FUNCTION__,
                            Logger::format("SAM might be restarted. RunningApp is created by WAM: appId(%s) instanceId(%d)", appId.c_str(), instanceId.c_str()));
            runningApp = RunningAppList::getInstance().createByAppId(appId);
            runningApp->setLifeStatus(LifeStatus::LifeStatus_BACKGROUND);
            runningApp->setInstanceId(instanceId);
            RunningAppList::getInstance().add(runningApp);
        }
        runningApp->setWebprocid(webprocessid);
        runningApp->setContext(CONTEXT_RUNNING);
    }
    RunningAppList::getInstance().removeAllByConext(AppType::AppType_Web, CONTEXT_STOP);
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

void WAM::onInitialzed()
{

}

void WAM::onFinalized()
{
    m_listRunningAppsCall.cancel();
    m_discardCodeCacheCall.cancel();
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

        RunningAppList::getInstance().removeAllByType(AppType::AppType_Web);
    }
}

bool WAM::onDiscardCodeCache(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = JDomParser::fromString(response.getPayload());
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
        Logger::warning(getInstance().getClassName(), __FUNCTION__, "Cannot find lunaTask about launch request");
        return false;
    }

    string instanceId = "";
    string appId = "";
    bool returnValue = true;

    JValueUtil::getValue(responsePayload, "instanceId", instanceId);
    JValueUtil::getValue(responsePayload, "appId", appId);
    JValueUtil::getValue(responsePayload, "returnValue", returnValue);

    if (!returnValue) {
        RunningAppList::getInstance().removeByIds(instanceId, "", appId);
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Failed to launch webapp in WAM");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByIds(instanceId, "", appId);
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Cannot find RunningApp");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }
    if (runningApp->isFirstLaunch()) {
        if (!runningApp->getPreload().empty()) {
            runningApp->setLifeStatus(LifeStatus::LifeStatus_PRELOADED);
        } else if (runningApp->isLaunchedHidden()) {
            runningApp->setLifeStatus(LifeStatus::LifeStatus_BACKGROUND);
        }
    }
    runningApp->setFirstLaunch(false);

    lunaTask->getResponsePayload().put("returnValue", true);
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
    Logger::info(getInstance().getClassName(), __FUNCTION__, appId, Logger::format("Launch Time: %f seconds", lunaTask->getTimeStamp()));
    return true;
}

bool WAM::launchApp(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    static string method = string("luna://") + getName() + string("/launchApp");
    JValue requestPayload = pbnjson::Object();

    if (!isConnected()) {
        Logger::info(getClassName(), __FUNCTION__, "WAM is not running. Waiting for WAM wakes up...");
    }

    JValue appDesc = pbnjson::Object();
    runningApp.getLaunchPoint()->toJson(appDesc);

    requestPayload.put("appDesc", appDesc);
    requestPayload.put("appId", runningApp.getAppId());
    requestPayload.put("instanceId", runningApp.getInstanceId());
    requestPayload.put("reason", lunaTask->getReason());
    requestPayload.put("launchingAppId", lunaTask->getCaller());
    requestPayload.put("launchingProcId", "");

    if (lunaTask->getParams().objectSize() != 0) {
        requestPayload.put("parameters", lunaTask->getParams());
    } else {
        requestPayload.put("parameters", runningApp.getLaunchPoint()->getParams());
    }

    if (runningApp.isKeepAlive()) {
        requestPayload.put("keepAlive", true);
    }

    if (runningApp.isFirstLaunch()) {
        // TODO This is temp solution about displayId
        // When home app support peropery. Please detete following code block
        if (lunaTask->getCaller() == "com.webos.app.home") {
            runningApp.setDisplayId(lunaTask->getDisplayAffinity());
            requestPayload["parameters"].put("displayAffinity", runningApp.getDisplayId());
        }

        if (!runningApp.getPreload().empty()) {
            runningApp.setLifeStatus(LifeStatus::LifeStatus_PRELOADING);
            requestPayload.put("preload", runningApp.getPreload());
        } else {
            runningApp.setLifeStatus(LifeStatus::LifeStatus_LAUNCHING);
        }
    } else {
        runningApp.setLifeStatus(LifeStatus::LifeStatus_LAUNCHING);
    }

    LSErrorSafe error;
    bool result = true;
    LSMessageToken token = 0;
    Logger::logCallRequest(getClassName(), __FUNCTION__, method, requestPayload);
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
    runningApp.setToken(token);
    return true;
}

bool WAM::close(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    string sender = lunaTask->getCaller();
    /* TODO This should be enabled after WAM supports 'keepAlive' feature
    if (sender != "com.webos.service.memorymanager" && runningApp.isKeepAlive()) {
        return pauseApp(runningApp, lunaTask);
    } else {
        return killApp(runningApp, lunaTask);
    }
    */
    return killApp(runningApp, lunaTask);
}

bool WAM::onPauseApp(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    LSMessageToken token = LSMessageGetResponseToken(message);
    LunaTaskPtr lunaTask = LunaTaskList::getInstance().getByToken(token);
    RunningAppPtr runningApp = RunningAppList::getInstance().getByToken(token);
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

    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Cannot find RunningApp");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }
    runningApp->setLifeStatus(LifeStatus::LifeStatus_PAUSED);
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
    requestPayload.put("instanceId", runningApp.getInstanceId());
    requestPayload.put("reason", "pause");
    requestPayload.put("parameters", lunaTask->getParams());

    LSErrorSafe error;
    bool result = true;
    LSMessageToken token = 0;
    Logger::logCallRequest(getClassName(), __FUNCTION__, method, requestPayload);
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
    runningApp.setToken(token);
    return true;
}

bool WAM::onKillApp(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    LSMessageToken token = LSMessageGetResponseToken(message);
    LunaTaskPtr lunaTask = LunaTaskList::getInstance().getByToken(token);
    RunningAppPtr runningApp = RunningAppList::getInstance().getByToken(token);

    string procId = "";
    bool returnValue = true;

    JValueUtil::getValue(responsePayload, "processId", procId);
    JValueUtil::getValue(responsePayload, "returnValue", returnValue);

    if (!returnValue) {
        if (lunaTask) {
            lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Failed to killApp in WAM");
            LunaTaskList::getInstance().removeAfterReply(lunaTask);
        }
        return true;
    }
    if (runningApp == nullptr) {
        Logger::info(getInstance().getClassName(), __FUNCTION__, "The RunningApp is already removed");
    } else {
        RunningAppList::getInstance().removeByObject(runningApp);
    }

    lunaTask->getResponsePayload().put("appId", runningApp->getAppId());
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
    return true;
}

bool WAM::killApp(RunningApp& runningApp, LunaTaskPtr lunaTask)
{
    static string method = string("luna://") + getName() + string("/killApp");
    JValue requestPayload = pbnjson::Object();

    if (!isConnected()) {
        Logger::warning(getClassName(), __FUNCTION__, "WAM is not running. The app is not exist");

        if (lunaTask) {
            lunaTask->getResponsePayload().put("returnValue", true);
            LunaTaskList::getInstance().removeAfterReply(lunaTask);
        }
        return true;
    }

    runningApp.setLifeStatus(LifeStatus::LifeStatus_CLOSING);
    requestPayload.put("instanceId", runningApp.getInstanceId());
    requestPayload.put("launchPointId", runningApp.getLaunchPointId());
    requestPayload.put("appId", runningApp.getAppId());
    if (lunaTask && !lunaTask->getReason().empty()) {
        requestPayload.put("reason", lunaTask->getReason());
    }

    LSErrorSafe error;
    bool result = true;
    LSMessageToken token = 0;
    Logger::logCallRequest(getClassName(), __FUNCTION__, method, requestPayload);
    result = LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onKillApp,
        nullptr,
        &token,
        &error
    );
    runningApp.setToken(token);
    if (!lunaTask)
        return true;

    if (!result) {
        lunaTask->setErrCodeAndText(error.error_code, error.message);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return false;
    }
    lunaTask->setToken(token);
    return true;
}
