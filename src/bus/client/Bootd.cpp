// Copyright (c) 2017-2020 LG Electronics, Inc.
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

#include "Bootd.h"

Bootd::Bootd()
    : AbsLunaClient("com.webos.bootManager")
{
    setClassName("Bootd");
}

Bootd::~Bootd()
{
}

void Bootd::onInitialzed()
{

}

void Bootd::onFinalized()
{
    m_getBootStatusCall.cancel();
}

void Bootd::onServerStatusChanged(bool isConnected)
{
    static string method = string("luna://") + getName() + string("/getBootStatus");

    if (isConnected) {
        m_getBootStatusCall = ApplicationManager::getInstance().callMultiReply(
            method.c_str(),
            AbsLunaClient::getSubscriptionPayload().stringify().c_str(),
            onGetBootStatus,
            this
        );
        Logger::logSubscriptionRequest(getClassName(), __FUNCTION__, method, AbsLunaClient::getSubscriptionPayload());
    } else {
        m_getBootStatusCall.cancel();
    }
}

bool Bootd::onGetBootStatus(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue subscriptionPayload = JDomParser::fromString(response.getPayload());
    Logger::logSubscriptionResponse(getInstance().getClassName(), __FUNCTION__, response, subscriptionPayload);

    if (subscriptionPayload.isNull())
        return true;

    Bootd::getInstance().EventGetBootStatus(subscriptionPayload);
    return true;
}
