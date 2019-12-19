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

#include "MemoryManager.h"

MemoryManager::MemoryManager()
    : AbsLunaClient("com.webos.service.memorymanager")
{
    setClassName("MemoryManager");
}

MemoryManager::~MemoryManager()
{
}

void MemoryManager::onInitialzed()
{

}

void MemoryManager::onFinalized()
{

}

void MemoryManager::onServerStatusChanged(bool isConnected)
{
}

bool MemoryManager::onRequireMemory(LSHandle* sh, LSMessage* message, void* context)
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

    int errorCode = 0;
    string errorText = "";
    bool returnValue = true;

    JValueUtil::getValue(responsePayload, "errorCode", errorCode);
    JValueUtil::getValue(responsePayload, "errorText", errorText);
    JValueUtil::getValue(responsePayload, "returnValue", returnValue);

    if (!returnValue) {
        lunaTask->setErrCodeAndText(errorCode, errorText);
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return true;
    }

    if (!lunaTask->getAPICallback().empty()) {
        lunaTask->getAPICallback()(lunaTask);
    }
    return true;
}

void MemoryManager::requireMemory(LunaTaskPtr lunaTask)
{
    static string method = string("luna://") + getName() + string("/requireMemory");
    JValue requestPayload = pbnjson::Object();

    if (!isConnected()) {
        Logger::warning(getClassName(), __FUNCTION__, "MemoryManager is not running. Skip memory reclaiming");
        if (!lunaTask->getAPICallback().empty()) {
            lunaTask->getAPICallback()(lunaTask);
        }
        return;
    }

    // TODO : 150MB should be extended based on appinfo.json
    requestPayload.put("requiredMemory", 150);

    LSErrorSafe error;
    bool result = true;
    LSMessageToken token = 0;
    Logger::logCallRequest(getClassName(), __FUNCTION__, method, requestPayload);
    result = LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onRequireMemory,
        nullptr,
        &token,
        &error
    );
    if (!result) {
        lunaTask->setErrCodeAndText(ErrCode_LAUNCH, "Failed to call memorymanager");
        LunaTaskList::getInstance().removeAfterReply(lunaTask);
        return;
    }
    lunaTask->setToken(token);
}
