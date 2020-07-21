// Copyright (c) 2019-2020 LG Electronics, Inc.
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

#include "PolicyManager.h"

#include "bus/client/AbsLifeHandler.h"
#include "bus/client/WAM.h"
#include "bus/client/NativeContainer.h"
#include "bus/client/MemoryManager.h"

PolicyManager::PolicyManager()
{
    setClassName("PolicyManager");
}

PolicyManager::~PolicyManager()
{
}

void PolicyManager::launch(LunaTaskPtr lunaTask)
{
    pre(lunaTask);

    string instanceId = RunningApp::generateInstanceId(lunaTask->getDisplayId());
    lunaTask->setInstanceId(instanceId);
    RunningAppPtr runningApp = RunningAppList::getInstance().createByLunaTask(lunaTask);
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Cannot create RunningApp");
        lunaTask->error(lunaTask);
        return;
    }
    if (runningApp->getLaunchPoint()->getAppDesc()->isLocked()) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH_APP_LOCKED, "app is locked");
        lunaTask->error(lunaTask);
        return;
    }
    runningApp->setLifeStatus(LifeStatus::LifeStatus_SPLASHING);
    RunningAppList::getInstance().add(runningApp);

    lunaTask->setSuccessCallback(boost::bind(&PolicyManager::onRequireMemory, this, _1));
    MemoryManager::getInstance().requireMemory(runningApp, lunaTask);
}

void PolicyManager::pause(LunaTaskPtr lunaTask)
{
    pre(lunaTask);

    RunningAppPtr runningApp = RunningAppList::getInstance().getByInstanceId(lunaTask->getInstanceId());
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_CLOSE, lunaTask->getId() + " is not running");
        lunaTask->error(lunaTask);
        return;
    }

    if (runningApp->isRegistered() && SAMConf::getInstance().isAppHandlingSupported()) {
        JValue payload = pbnjson::Object();
        runningApp->toEventJson(payload, lunaTask, "pause");
        if (runningApp->sendEvent(payload)) {
            lunaTask->success(lunaTask);
            return;
        }
    }

    AbsLifeHandler::getLifeHandler(runningApp).pause(runningApp, lunaTask);
}

void PolicyManager::close(LunaTaskPtr lunaTask)
{
    pre(lunaTask);

    RunningAppPtr runningApp = RunningAppList::getInstance().getByInstanceId(lunaTask->getInstanceId());
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_CLOSE, lunaTask->getId() + " is not running");
        lunaTask->error(lunaTask);
        return;
    }

    if (runningApp->isRegistered() && SAMConf::getInstance().isAppHandlingSupported()) {
        JValue payload = pbnjson::Object();
        runningApp->toEventJson(payload, lunaTask, "close");
        if (runningApp->sendEvent(payload)) {
            // The application should be closed within transition timeout
            // If it isn't, SAM kills the application directly.
            runningApp->setLifeStatus(LifeStatus::LifeStatus_CLOSING);
            lunaTask->success(lunaTask);
            return;
        }
    }

    AbsLifeHandler::getLifeHandler(runningApp).close(runningApp, lunaTask);
}

void PolicyManager::relaunch(LunaTaskPtr lunaTask)
{
    pre(lunaTask);

    RunningAppPtr runningApp = RunningAppList::getInstance().getByInstanceId(lunaTask->getInstanceId());
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_PAUSE, lunaTask->getId() + " is not running");
        lunaTask->error(lunaTask);
        return;
    }

    runningApp->setLifeStatus(LifeStatus::LifeStatus_LAUNCHING);
    if (runningApp->isRegistered() && SAMConf::getInstance().isAppHandlingSupported()) {
        JValue payload = pbnjson::Object();
        runningApp->toEventJson(payload, lunaTask, "relaunch");
        if (runningApp->sendEvent(payload)) {
            lunaTask->success(lunaTask);
            return;
        }
    }

    if (runningApp->getLaunchPoint()->getAppDesc()->getAppType() == AppType::AppType_Web) {
        AbsLifeHandler::getLifeHandler(runningApp).launch(runningApp, lunaTask);
    } else {
        lunaTask->setSuccessCallback(boost::bind(&PolicyManager::launch, this, _1));
        close(lunaTask);
    }
}

void PolicyManager::removeLaunchPoint(LunaTaskPtr lunaTask)
{
    pre(lunaTask);
    if (RunningAppList::getInstance().getByLunaTask(lunaTask) != nullptr) {
        lunaTask->setSuccessCallback(boost::bind(&PolicyManager::onCloseForRemove, this, _1));
        close(lunaTask);
        return;
    }
    onCloseForRemove(lunaTask);
}

void PolicyManager::onCloseForRemove(LunaTaskPtr lunaTask)
{
    lunaTask->setSuccessCallback(boost::bind(&PolicyManager::onSuccess, this, _1));
    LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByLaunchPointId(lunaTask->getLaunchPointId());
    switch(launchPoint->getType()) {
    case LaunchPointType::LaunchPoint_DEFAULT:
        // TODO launchPoint
//        if (AppLocation::AppLocation_System_ReadOnly != launchPoint->getAppDesc()->getAppLocation()) {
//            Call call = AppInstallService::getInstance().remove(launchPoint->getAppDesc()->getAppId());
//            Logger::info(getClassName(), __FUNCTION__, launchPoint->getAppDesc()->getAppId(), "requested_to_appinstalld");
//        }
//
//        if (!launchPoint->getAppDesc()->isVisible()) {
//            AppDescriptionList::getInstance().removeByObject(launchPoint->getAppDesc());
//            // removeApp(appId, false, AppStatusEvent::AppStatusEvent_Uninstalled);
//        } else {
//            // Call call = SettingService::getInstance().checkParentalLock(onCheckParentalLock, appId);
//        }
        break;

    case LaunchPointType::LaunchPoint_BOOKMARK:
        LaunchPointList::getInstance().remove(launchPoint);
        break;

    default:
        lunaTask->setErrCodeAndText(ErrCode_GENERAL, "Invalid launch point type");
        lunaTask->error(lunaTask);
        return;
    }
    lunaTask->success(lunaTask);
}

void PolicyManager::pre(LunaTaskPtr lunaTask)
{
    if (!lunaTask->hasSuccessCallback()) {
        lunaTask->setSuccessCallback(boost::bind(&PolicyManager::onSuccess, this, _1));
    }
    if (!lunaTask->hasErrorCallback()) {
        lunaTask->setErrorCallback(boost::bind(&PolicyManager::onError, this, _1));
    }
}

void PolicyManager::onRequireMemory(LunaTaskPtr lunaTask)
{
    lunaTask->setSuccessCallback(boost::bind(&PolicyManager::onSuccess, this, _1));
    RunningAppPtr runningApp = RunningAppList::getInstance().getByInstanceId(lunaTask->getInstanceId());
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_PAUSE, lunaTask->getId() + " is not running");
        lunaTask->error(lunaTask);
        return;
    }

    runningApp->setLifeStatus(LifeStatus::LifeStatus_SPLASHED);
    AbsLifeHandler::getLifeHandler(runningApp).launch(runningApp, lunaTask);
}

void PolicyManager::onError(LunaTaskPtr lunaTask)
{
    LunaTaskList::getInstance().removeAfterReply(lunaTask, true);
}

void PolicyManager::onSuccess(LunaTaskPtr lunaTask)
{
    LunaTaskList::getInstance().removeAfterReply(lunaTask, true);
}
