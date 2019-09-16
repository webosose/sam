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
#include <bus/service/ApplicationManager.h>
#include <pbnjson.hpp>
#include <util/LSUtils.h>

const string Configd::NAME = "com.webos.service.config";

Configd::Configd()
    : AbsLunaClient(NAME)
{
    setClassName(NAME);
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

        if (m_configNames.empty())
            return;

        pbnjson::JValue requestPayload = pbnjson::Object();
        pbnjson::JValue configNames = pbnjson::Array();

        requestPayload.put("subscribe", true);
        for (auto& key : m_configNames) {
            configNames.append(key);
        }
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
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull())
        return false;

    Configd::getInstance().EventConfigInfo(responsePayload);
    return true;
}
