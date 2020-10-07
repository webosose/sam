// Copyright (c) 2012-2020 LG Electronics, Inc.
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

    if (response.isHubError()) {
        return false;
    }

    JValue running = pbnjson::Array();
    string appId = "";
    string instanceId = "";
    string webprocessid = "";
    int displayId = -1;

    RunningAppList::getInstance().setConext(AppType::AppType_Web, CONTEXT_STOP);
    JValueUtil::getValue(subscriptionPayload, "running", running);
    int size = running.arraySize();
    for (int i = 0; i < size; i++) {
        JValueUtil::getValue(running[i], "id", appId);
        JValueUtil::getValue(running[i], "webprocessid", webprocessid);

        // TODO fire a bug to WAM.
        if (JValueUtil::getValue(running[i], "instanceid", instanceId)) {
            displayId = RunningApp::getDisplayId(instanceId);
        }
        if (JValueUtil::getValue(running[i], "instanceId", instanceId)) {
            displayId = RunningApp::getDisplayId(instanceId);
        }

        RunningAppPtr runningApp = RunningAppList::getInstance().getByIds(instanceId, appId);
        if (runningApp == nullptr) {
            Logger::warning(getInstance().getClassName(), __FUNCTION__,
                            Logger::format("SAM might be restarted. RunningApp is created by WAM: appId(%s) instanceId(%s)", appId.c_str(), instanceId.c_str()));
            runningApp = RunningAppList::getInstance().createByAppId(appId);
            if (runningApp == nullptr)
                continue; // Cannot find launchPoint
            runningApp->setLifeStatus(LifeStatus::LifeStatus_BACKGROUND);
            runningApp->setWebprocid(webprocessid);
            runningApp->setInstanceId(instanceId);
            runningApp->setDisplayId(displayId);
            RunningAppList::getInstance().add(runningApp);
        } else {
            if (runningApp->getWindowId() != webprocessid)
                runningApp->setWebprocid(webprocessid);
            if (displayId != -1)
                runningApp->setDisplayId(displayId);
            ApplicationManager::getInstance().postRunning(runningApp);
        }
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
        m_listRunningAppsCall.cancel();

        // SAM is running before WAM
        // Sometimes, app launching request is already in LS2 queue before WAM running
        if (m_serverStatusCount > 1) {
            RunningAppList::getInstance().removeAllByType(AppType::AppType_Web);
        }
    }
}

bool WAM::onLaunchApp(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    if (response.isHubError()) {
        return false;
    }

    LSMessageToken token = LSMessageGetResponseToken(message);
    LunaTaskPtr lunaTask = LunaTaskList::getInstance().getByToken(token);
    if (lunaTask == nullptr) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "Cannot find lunaTask about launch request");
        return false;
    }

    string instanceId = "";
    string appId = "";
    bool returnValue = true;

    JValueUtil::getValue(responsePayload, "instanceId", instanceId);
    JValueUtil::getValue(responsePayload, "appId", appId);
    JValueUtil::getValue(responsePayload, "returnValue", returnValue);

    if (!returnValue) {
        RunningAppList::getInstance().removeByInstanceId(lunaTask->getInstanceId());
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Failed to launch webapp in WAM");
        lunaTask->error(lunaTask);
        return true;
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByInstanceId(lunaTask->getInstanceId());
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Cannot find RunningApp");
        lunaTask->error(lunaTask);
        return true;
    }
    if (runningApp->isFirstLaunch()) {
        if (!runningApp->getPreload().empty()) {
            runningApp->setLifeStatus(LifeStatus::LifeStatus_PRELOADED);
        } else if (runningApp->isLaunchedHidden()) {
            runningApp->setLifeStatus(LifeStatus::LifeStatus_BACKGROUND);
        }
    }

    lunaTask->success(lunaTask);
    Logger::info(getInstance().getClassName(), __FUNCTION__, runningApp->getAppId(), Logger::format("Launch Time: %lld ms", runningApp->getTimeStamp()));
    return true;
}

void WAM::launch(RunningAppPtr runningApp, LunaTaskPtr lunaTask)
{
    static string method = string("luna://") + getName() + string("/launchApp");
    JValue requestPayload = pbnjson::Object();

    if (!isConnected()) {
        Logger::info(getClassName(), __FUNCTION__, "WAM is not running. Waiting for WAM wakes up...");
    }

    // We don't need to launch again if it requires 'LaunchedHidden'
    if (!runningApp->isFirstLaunch() && lunaTask->isLaunchedHidden()) {
        lunaTask->success(lunaTask);
        return;
    }

    JValue appDesc = pbnjson::Object();
    runningApp->getLaunchPoint()->toJson(appDesc);

    requestPayload.put("appDesc", appDesc);
    requestPayload.put("appId", runningApp->getAppId());
    requestPayload.put("instanceId", runningApp->getInstanceId());
    requestPayload.put("reason", lunaTask->getReason());
    requestPayload.put("launchingAppId", lunaTask->getCaller());
    requestPayload.put("launchingProcId", "");
    requestPayload.put("parameters", lunaTask->getParams());

    if (runningApp->isKeepAlive()) {
        requestPayload.put("keepAlive", true);
    }

    if (runningApp->isFirstLaunch() && !runningApp->getPreload().empty()) {
        runningApp->setLifeStatus(LifeStatus::LifeStatus_PRELOADING);
        requestPayload.put("preload", runningApp->getPreload());
    } else {
        runningApp->setLifeStatus(LifeStatus::LifeStatus_LAUNCHING);
    }

    LSErrorSafe error;
    LSMessageToken token = 0;
    Logger::logCallRequest(getClassName(), __FUNCTION__, method, requestPayload);
    if (!LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onLaunchApp,
        nullptr,
        &token,
        &error
    )) {
        RunningAppList::getInstance().removeByObject(runningApp);
        lunaTask->setErrCodeAndText(error.error_code, error.message);
        lunaTask->error(lunaTask);
        return;
    }
    lunaTask->setToken(token);
    runningApp->setToken(token);
}

void WAM::close(RunningAppPtr runningApp, LunaTaskPtr lunaTask)
{
    string sender = lunaTask->getCaller();
    if (sender == "com.webos.service.memorymanager") {
        killApp(runningApp, lunaTask);
        return;
    }

    if (runningApp->isKeepAlive()) {
        pause(runningApp, lunaTask);
    } else {
        killApp(runningApp, lunaTask);
    }
}

void WAM::kill(RunningAppPtr runningApp)
{
    killApp(runningApp, nullptr);
}

bool WAM::onPauseApp(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    if (response.isHubError()) {
        return false;
    }

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
        RunningAppList::getInstance().removeByInstanceId(lunaTask->getInstanceId());
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Failed to pause webapp in WAM");
        lunaTask->error(lunaTask);
        return true;
    }

    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Cannot find RunningApp");
        lunaTask->error(lunaTask);
        return true;
    }
    runningApp->setLifeStatus(LifeStatus::LifeStatus_PAUSED);
    lunaTask->success(lunaTask);
    return true;
}

void WAM::pause(RunningAppPtr runningApp, LunaTaskPtr lunaTask)
{
    static string method = string("luna://") + getName() + string("/pauseApp");

    if (!isConnected()) {
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "WAM is not running. The app is not exist");
        lunaTask->error(lunaTask);
        return;
    }

    runningApp->setLifeStatus(LifeStatus::LifeStatus_PAUSING);
    JValue requestPayload = pbnjson::Object();
    requestPayload.put("appId", runningApp->getAppId());
    requestPayload.put("instanceId", runningApp->getInstanceId());
    requestPayload.put("reason", "pause");
    requestPayload.put("parameters", lunaTask->getParams());

    LSErrorSafe error;
    LSMessageToken token = 0;
    Logger::logCallRequest(getClassName(), __FUNCTION__, method, requestPayload);
    if (!LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onPauseApp,
        nullptr,
        &token,
        &error
    )) {
        lunaTask->setErrCodeAndText(error.error_code, error.message);
        lunaTask->error(lunaTask);
        return;
    }
    lunaTask->setToken(token);
    runningApp->setToken(token);
}

bool WAM::onKillApp(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    if (response.isHubError()) {
        return false;
    }

    LSMessageToken token = LSMessageGetResponseToken(message);
    LunaTaskPtr lunaTask = LunaTaskList::getInstance().getByToken(token);
    RunningAppPtr runningApp = RunningAppList::getInstance().getByToken(token);

    string procId = "";
    bool returnValue = true;

    JValueUtil::getValue(responsePayload, "processId", procId);
    JValueUtil::getValue(responsePayload, "returnValue", returnValue);

    if (!returnValue && lunaTask) {
        Logger::warning(getInstance().getClassName(), __FUNCTION__, "Failed to kill app. WAM might be restarted");
        if (lunaTask) {
            lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Failed to killApp in WAM");
            lunaTask->error(lunaTask);
        }
        return true;
    }

    // RunningApp was already removed in onListRunningApp subscription
    if (runningApp) {
        RunningAppList::getInstance().removeByObject(runningApp);
    }

    // Should not access RunningApp. It can be nullptr.
    if (lunaTask) {
        lunaTask->fillIds(lunaTask->getResponsePayload());
        lunaTask->success(lunaTask);
    }

    return true;
}

void WAM::killApp(RunningAppPtr runningApp, LunaTaskPtr lunaTask)
{
    static string method = string("luna://") + getName() + string("/killApp");
    JValue requestPayload = pbnjson::Object();

    if (!isConnected()) {
        Logger::warning(getClassName(), __FUNCTION__, "WAM is not running. The app is not exist");
        lunaTask->success(lunaTask);
        return;
    }

    runningApp->setLifeStatus(LifeStatus::LifeStatus_CLOSING);
    requestPayload.put("instanceId", runningApp->getInstanceId());
    requestPayload.put("launchPointId", runningApp->getLaunchPointId());
    requestPayload.put("appId", runningApp->getAppId());
    if (lunaTask) {
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
    runningApp->setToken(token);

    if (lunaTask) {
        if (!result) {
            lunaTask->setErrCodeAndText(error.error_code, error.message);
            lunaTask->error(lunaTask);
        } else {
            lunaTask->setToken(token);
        }
    }
}
