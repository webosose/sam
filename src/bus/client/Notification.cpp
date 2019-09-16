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

#include <bus/client/Notification.h>
#include <bus/service/ApplicationManager.h>

const std::string Notification::NAME = "com.webos.notification";

Notification::Notification()
    : AbsLunaClient(NAME)
{
}

Notification::~Notification()
{
}

void Notification::onInitialze()
{

}

void Notification::onServerStatusChanged(bool isConnected)
{

}

Call Notification::createPincodePrompt(LSFilterFunc func)
{
    static string method = std::string("luna://") + getName() + std::string("/createPincodePrompt");

    JValue requestPayload = pbnjson::Object();
    requestPayload.put("promptType", "parental");

    Call call = ApplicationManager::getInstance().callOneReply(method.c_str(), requestPayload.stringify().c_str(), func, nullptr);
    return call;
}
