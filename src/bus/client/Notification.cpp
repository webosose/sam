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

#include "bus/client/Notification.h"

#include "util/Logger.h"
#include "util/JValueUtil.h"

Notification::Notification()
    : AbsLunaClient("com.webos.notification")
{
    setClassName("Notification");
}

Notification::~Notification()
{
}

void Notification::onInitialzed()
{

}

void Notification::onFinalized()
{

}

void Notification::onServerStatusChanged(bool isConnected)
{

}

bool Notification::onCreatePincodePrompt(LSHandle* sh, LSMessage* message, void* context)
{
    JValue responsePayload = pbnjson::JDomParser::fromString(LSMessageGetPayload(message));
    bool returnValue = false;
    bool matched = false;

    if (!JValueUtil::getValue(responsePayload, "returnValue", returnValue) || !JValueUtil::getValue(responsePayload, "matched", matched)) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "Failed to get required params");
        return true;
    }


    if (matched == false) {
        Logger::info(getInstance().getClassName(), __FUNCTION__, "uninstallation is canceled because of invalid pincode");
    } else {
        // TODO: appId should be passed
        // AppPackageManager::getInstance().removeApp(appId, false, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
    }
    return true;
}

Call Notification::createPincodePrompt(LSFilterFunc func)
{
    static string method = string("luna://") + getName() + string("/createPincodePrompt");

    JValue requestPayload = pbnjson::Object();
    requestPayload.put("promptType", "parental");

    Call call = ApplicationManager::getInstance().callOneReply(method.c_str(), requestPayload.stringify().c_str(), func, nullptr);
    return call;
}
