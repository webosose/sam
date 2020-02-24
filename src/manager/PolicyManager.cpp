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
    if (lunaTask->getAPICallback().empty()) {
        lunaTask->setAPICallback(boost::bind(&PolicyManager::launch, this, _1));
    } else {
        Logger::info(getClassName(), __FUNCTION__, lunaTask->getNextStep());
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByInstanceId(lunaTask->getInstanceId());
    if (runningApp == nullptr) {
        string instanceId = RunningApp::generateInstanceId(lunaTask->getDisplayId());

        lunaTask->setInstanceId(instanceId);
        runningApp = RunningAppList::getInstance().createByLunaTask(lunaTask);
        if (runningApp == nullptr) {
            LunaTaskList::getInstance().removeAfterReply(lunaTask, ErrCode_LAUNCH, "Cannot create RunningApp");
            return;
        }
        if (runningApp->getLaunchPoint()->getAppDesc()->isLocked()) {
            LunaTaskList::getInstance().removeAfterReply(lunaTask, ErrCode_LAUNCH_APP_LOCKED, "app is locked");
            return;
        }
        runningApp->setLifeStatus(LifeStatus::LifeStatus_SPLASHING);
        RunningAppList::getInstance().add(runningApp);
    }

    if (lunaTask->getNextStep().empty()) {
        lunaTask->setNextStep("Launch App");
        MemoryManager::getInstance().requireMemory(runningApp, lunaTask);
    } else if (lunaTask->getNextStep() == "Launch App") {
        lunaTask->setNextStep("Complete");
        runningApp->setLifeStatus(LifeStatus::LifeStatus_SPLASHED);
        AbsLifeHandler::getLifeHandler(runningApp).launch(runningApp, lunaTask);
    } else if (lunaTask->getNextStep() == "Complete") {
        runningApp->toJson(lunaTask->getResponsePayload());
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
    } else {
        LunaTaskList::getInstance().removeAfterReply(lunaTask, ErrCode_LAUNCH, "Unknown Error");
        RunningAppList::getInstance().removeByObject(runningApp);
    }
}

void PolicyManager::pause(LunaTaskPtr lunaTask)
{
    if (lunaTask->getAPICallback().empty()) {
        lunaTask->setAPICallback(boost::bind(&PolicyManager::pause, this, _1));
    } else {
        Logger::info(getClassName(), __FUNCTION__, lunaTask->getNextStep());
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByInstanceId(lunaTask->getInstanceId());
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_PAUSE, lunaTask->getId() + " is not running");
        goto Error;
    }

    if (runningApp->isRegistered() && SAMConf::getInstance().isAppHandlingSupported()) {
        JValue payload = pbnjson::Object();
        runningApp->toEventJson(payload, lunaTask, "pause");
        if (!runningApp->sendEvent(payload)) {
            goto Error;
        }

        runningApp->toJson(lunaTask->getResponsePayload());
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    if (lunaTask->getNextStep().empty()) {
        lunaTask->setNextStep("Complete");
        AbsLifeHandler::getLifeHandler(runningApp).pause(runningApp, lunaTask);
    } else if (lunaTask->getNextStep() == "Complete") {
        runningApp->toJson(lunaTask->getResponsePayload());
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
    }
    return;

Error:
    if (runningApp) {
        runningApp->toJson(lunaTask->getResponsePayload());
        RunningAppList::getInstance().removeByInstanceId(runningApp->getInstanceId());
    }
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void PolicyManager::close(LunaTaskPtr lunaTask)
{
    if (lunaTask->getAPICallback().empty()) {
        lunaTask->setAPICallback(boost::bind(&PolicyManager::close, this, _1));
    } else {
        Logger::info(getClassName(), __FUNCTION__, lunaTask->getNextStep());
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByInstanceId(lunaTask->getInstanceId());
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_CLOSE, lunaTask->getId() + " is not running");
        goto Error;
    }

    if (runningApp->isRegistered() && SAMConf::getInstance().isAppHandlingSupported()) {
        JValue payload = pbnjson::Object();
        runningApp->toEventJson(payload, lunaTask, "close");
        if (!runningApp->sendEvent(payload)) {
            goto Error;
        }
        // Close has different logic with pause
    }

    if (lunaTask->getNextStep().empty()) {
        lunaTask->setNextStep("Complete");
        AbsLifeHandler::getLifeHandler(runningApp).close(runningApp, lunaTask);
    } else if (lunaTask->getNextStep() == "Complete") {
        runningApp->toJson(lunaTask->getResponsePayload());
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
    }
    return;

Error:
    if (runningApp) {
        runningApp->toJson(lunaTask->getResponsePayload());
        RunningAppList::getInstance().removeByInstanceId(runningApp->getInstanceId());
    }
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}

void PolicyManager::relaunch(LunaTaskPtr lunaTask)
{
    if (lunaTask->getAPICallback().empty()) {
        lunaTask->setAPICallback(boost::bind(&PolicyManager::relaunch, this, _1));
    } else {
        Logger::info(getClassName(), __FUNCTION__, lunaTask->getNextStep());
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByInstanceId(lunaTask->getInstanceId());
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_RELAUNCH, lunaTask->getId() + " is not running");
        goto Error;
    }

    if (runningApp->isRegistered() && SAMConf::getInstance().isAppHandlingSupported()) {
        JValue payload = pbnjson::Object();
        runningApp->toEventJson(payload, lunaTask, "relaunch");
        if (!runningApp->sendEvent(payload)) {
            goto Error;
        }

        runningApp->toJson(lunaTask->getResponsePayload());
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }

    if (AppType::AppType_Web == runningApp->getLaunchPoint()->getAppDesc()->getAppType()) {
        lunaTask->setNextStep("Complete");
        // SAM just calls WAM in case of webapp
        WAM::getInstance().launch(runningApp, lunaTask);
    } else if (lunaTask->getNextStep().empty()) {
        lunaTask->setNextStep("Launching Native App Again");
        // In case of native app, it should be closed first.
        NativeContainer::getInstance().close(runningApp, lunaTask);
    } else if (lunaTask->getNextStep() == "Launching Native App Again") {
        lunaTask->setNextStep("Complete");
        // SAM launches native app again.
        NativeContainer::getInstance().launch(runningApp, lunaTask);
    } else if (lunaTask->getNextStep() == "Complete") {
        runningApp->toJson(lunaTask->getResponsePayload());
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
    }
    return;

Error:
    if (runningApp) {
        runningApp->toJson(lunaTask->getResponsePayload());
        RunningAppList::getInstance().removeByInstanceId(runningApp->getInstanceId());
    }
    LunaTaskList::getInstance().removeAfterReply(lunaTask);
}
