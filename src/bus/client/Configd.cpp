// Copyright (c) 2017-2019 LG Electronics, Inc.
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

#include <bus/client/Configd.h>
#include <pbnjson.hpp>

Configd::Configd()
    : AbsLunaClient("com.webos.service.config")
{
    setClassName("Configd");

    configNames = pbnjson::Array();
    configNames.append("system.sysAssetFallbackPrecedence");
    configNames.append("com.webos.applicationManager.keepAliveApps");
    configNames.append("com.webos.applicationManager.supportQmlBooster");
    configNames.append("com.webos.applicationManager.lifeCycle");
}

Configd::~Configd()
{
}

void Configd::onInitialze()
{

}

void Configd::onServerStatusChanged(bool isConnected)
{
    static std::string method = std::string("luna://") + getName() + std::string("/getConfigs");

    if (isConnected) {
        if (m_getConfigsCall.isActive())
            return;

        pbnjson::JValue requestPayload = pbnjson::Object();
        requestPayload.put("subscribe", true);
        requestPayload.put("configNames", configNames);

        m_getConfigsCall = ApplicationManager::getInstance().callMultiReply(
            method.c_str(),
            requestPayload.stringify().c_str(),
            onGetConfigs,
            nullptr
        );
    } else {
        m_getConfigsCall.cancel();
    }
}

bool Configd::onGetConfigs(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    pbnjson::JValue subscriptionPayload = JDomParser::fromString(response.getPayload());
    Logger::logSubscriptionResponse(getInstance().getClassName(), __FUNCTION__, response, subscriptionPayload);

    if (subscriptionPayload.isNull())
        return false;

    Configd::getInstance().EventGetConfigs(subscriptionPayload);
    return true;
}
