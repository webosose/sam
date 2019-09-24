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

#include "AbsLunaClient.h"

JValue& AbsLunaClient::getEmptyPayload()
{
    static JValue empty;

    if (empty.isNull()) {
        empty = pbnjson::Object();
    }
    return empty;
}

JValue& AbsLunaClient::getSubscriptionPayload()
{
    static JValue subscription;
    if (subscription.isNull()) {
        subscription = pbnjson::Object();
        subscription.put("subscribe", true);
    }
    return subscription;
}

bool AbsLunaClient::_onServerStatus(LSHandle* sh, LSMessage* message, void* context)
{
    AbsLunaClient* client = static_cast<AbsLunaClient*>(context);
    pbnjson::JValue subscriptionPayload = JDomParser::fromString(LSMessageGetPayload(message));

    bool connected = false;
    if (!JValueUtil::getValue(subscriptionPayload, "connected", connected)) {
        return true;
    }

    client->m_isConnected = connected;
    client->EventServiceStatusChanged(connected);
    client->onServerStatusChanged(connected);
    return true;
}

AbsLunaClient::AbsLunaClient(const string& name)
    : m_name(name),
      m_isConnected(false)
{
    setClassName("AbsLunaClient");
}

AbsLunaClient::~AbsLunaClient()
{
}

// This API should be called after bus registration
void AbsLunaClient::initialize()
{
    JValue requestPayload = pbnjson::Object();
    requestPayload.put("serviceName", getName());
    m_statusCall = ApplicationManager::getInstance().callMultiReply(
        "luna://com.webos.service.bus/signal/registerServerStatus",
        requestPayload.stringify().c_str(),
        _onServerStatus,
        this
    );

    onInitialze();
}
