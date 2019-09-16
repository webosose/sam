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

#include <bus/client/Bootd.h>
#include <bus/service/ApplicationManager.h>
#include <pbnjson.hpp>
#include <util/LSUtils.h>

const string Bootd::NAME = "com.webos.bootManager";

Bootd::Bootd()
    : AbsLunaClient(NAME)
{
    setClassName(NAME);
}

Bootd::~Bootd()
{
}

void Bootd::onInitialze()
{

}

void Bootd::onServerStatusChanged(bool isConnected)
{
    static std::string method = std::string("luna://") + getName() + std::string("/getBootStatus");

    if (isConnected) {
        if (m_getBootStatus.isActive())
            return;

        m_getBootStatus = ApplicationManager::getInstance().callMultiReply(
            method.c_str(),
            AbsLunaClient::getSubscriptionPayload().stringify().c_str(),
            onGetBootStatus,
            this
        );
    } else {
        m_getBootStatus.cancel();
    }
}

bool Bootd::onGetBootStatus(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull())
        return false;

    if (responsePayload.hasKey("bootStatus")) {
        // factory, normal, firstUse
        Bootd::getInstance().m_bootStatusStr = responsePayload["bootStatus"].asString();
    }

    Bootd::getInstance().EventBootStatusChanged(responsePayload);
    return true;
}
