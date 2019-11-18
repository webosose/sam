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

void PolicyManager::launch(LunaTaskPtr lunaTask)
{
    printf("Step = %s\n", lunaTask->getNextStep().c_str());
    if (lunaTask->getAPICallback().empty()) {
        lunaTask->setAPICallback(boost::bind(&PolicyManager::launch, this, _1));
    }

    if (lunaTask->getNextStep().empty()) {
        lunaTask->setNextStep("Check Memory Status");
        checkExecutionLock(lunaTask);
    } else if (lunaTask->getNextStep() == "Check Memory Status") {
        lunaTask->setNextStep("Launch App");
        RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(lunaTask->getAppId(), true);
        // TODO
        //LifecycleManager::getInstance().generateLifeCycleEvent(lunaTask->getAppId(), lunaTask->getInstanceId(), LifeEvent::SPLASH);
        MemoryManager::getInstance().requireMemory(lunaTask);
    } else if (lunaTask->getNextStep() == "Launch App") {
        lunaTask->setNextStep("Complete");

        RunningAppPtr runningApp = RunningAppList::getInstance().getByAppId(lunaTask->getAppId());
        runningApp->launch(lunaTask);
    } else {
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
    }
}

void PolicyManager::removeLaunchPoint(LunaTaskPtr lunaTask)
{

}

void PolicyManager::checkExecutionLock(LunaTaskPtr lunaTask)
{
    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getById(lunaTask->getAppId());
    if (appDesc == nullptr || appDesc->isLocked()) {
        Logger::error("PrelauncherStage", __FUNCTION__, lunaTask->getAppId(), "app is locked");
        lunaTask->setErrCodeAndText(ErrCode_APP_LOCKED, "app is locked");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        // TODO reply error
        // return false;
    }

    lunaTask->getAPICallback()(lunaTask);
}
