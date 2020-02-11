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

#include "manager/PolicyManager.h"

#include "bus/client/MemoryManager.h"

PolicyManager::PolicyManager()
{
    setClassName("PolicyManager");
}

PolicyManager::~PolicyManager()
{
}

void PolicyManager::relaunch(LunaTaskPtr lunaTask)
{
    if (!lunaTask->getNextStep().empty()) {
        Logger::info(getClassName(), __FUNCTION__, lunaTask->getNextStep());
    }
    if (lunaTask->getAPICallback().empty()) {
        lunaTask->setAPICallback(boost::bind(&PolicyManager::relaunch, this, _1));
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByLunaTask(lunaTask);
    if (runningApp == nullptr) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "The app is not running");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    runningApp->launch(lunaTask);
}

void PolicyManager::launch(LunaTaskPtr lunaTask)
{
    if (!lunaTask->getNextStep().empty()) {
        Logger::info(getClassName(), __FUNCTION__, lunaTask->getNextStep());
    }
    if (lunaTask->getAPICallback().empty()) {
        lunaTask->setAPICallback(boost::bind(&PolicyManager::launch, this, _1));
    }

    RunningAppPtr runningApp = RunningAppList::getInstance().getByInstanceId(lunaTask->getInstanceId());
    if (runningApp == nullptr) {
        int displayId = lunaTask->getDisplayId();
        string instanceId = RunningApp::generateInstanceId(displayId);
        lunaTask->setInstanceId(instanceId);

        runningApp = RunningAppList::getInstance().createByLunaTask(lunaTask);
        runningApp->setLifeStatus(LifeStatus::LifeStatus_SPLASHING);

        if (!checkExecutionLock(runningApp)) {
            lunaTask->setErrCodeAndText(ErrCode_APP_LOCKED, "app is locked");
            LunaTaskList::getInstance().removeAfterReply(lunaTask);
        }
        RunningAppList::getInstance().add(runningApp);
    }

    if (lunaTask->getNextStep().empty()) {
        lunaTask->setNextStep("Launch App");
        MemoryManager::getInstance().requireMemory(lunaTask);
    } else if (lunaTask->getNextStep() == "Launch App") {
        lunaTask->setNextStep("Complete");
        if (runningApp->getLifeStatus() == LifeStatus::LifeStatus_SPLASHING)
            runningApp->setLifeStatus(LifeStatus::LifeStatus_SPLASHED);
        runningApp->launch(lunaTask);
    } else {
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
    }
}

bool PolicyManager::checkExecutionLock(RunningAppPtr runningApp)
{
    if (runningApp == nullptr) {
        return false;
    }
    if (runningApp->getLaunchPoint()->getAppDesc()->isLocked()) {
        return false;
    }
    return true;
}
