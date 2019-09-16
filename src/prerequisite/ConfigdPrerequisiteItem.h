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

#ifndef PREREQUISITE_CONFIGDPREREQUISITEITEM_H_
#define PREREQUISITE_CONFIGDPREREQUISITEITEM_H_

#include <iostream>
#include <pbnjson.hpp>
#include <boost/bind.hpp>
#include <boost/signals2.hpp>
#include <bus/client/Configd.h>
#include <prerequisite/PrerequisiteItem.h>
#include "setting/Settings.h"

class ConfigdPrerequisiteItem: public PrerequisiteItem {
public:
    ConfigdPrerequisiteItem()
    {
    }

    virtual ~ConfigdPrerequisiteItem()
    {
        m_eventConnection.disconnect();
    }

    virtual void start()
    {
        Configd::getInstance().addRequiredKey(CONFIGD_KEY_FALLBACK_PRECEDENCE);
        Configd::getInstance().addRequiredKey(CONFIGD_KEY_KEEPALIVE_APPS);
        Configd::getInstance().addRequiredKey(CONFIGD_KEY_SUPPORT_QML_BOOSTER);
        Configd::getInstance().addRequiredKey(CONFIGD_KEY_LIFECYCLE_REASON);

        m_eventConnection = Configd::getInstance().EventConfigInfo.connect(boost::bind(&ConfigdPrerequisiteItem::onResponse, this, _1));
        setStatus(PrerequisiteItemStatus::DOING);
    }

    void onResponse(const pbnjson::JValue& responsePayload)
    {
        Logger::info("ConfigdPrerequisiteItem", __FUNCTION__, "received_configd_msg", responsePayload.duplicate().stringify());

        if (!responsePayload.hasKey("configs") || !responsePayload["configs"].isObject()) {
            Logger::warning("ConfigdPrerequisiteItem", __FUNCTION__, "Internal Error");
        }

        if (responsePayload["configs"].hasKey(CONFIGD_KEY_FALLBACK_PRECEDENCE) &&
            responsePayload["configs"][CONFIGD_KEY_FALLBACK_PRECEDENCE].isArray())
            SettingsImpl::getInstance().setAssetFallbackKeys(responsePayload["configs"][CONFIGD_KEY_FALLBACK_PRECEDENCE]);

        if (responsePayload["configs"].hasKey(CONFIGD_KEY_KEEPALIVE_APPS) &&
            responsePayload["configs"][CONFIGD_KEY_KEEPALIVE_APPS].isArray())
            SettingsImpl::getInstance().setKeepAliveApps(responsePayload["configs"][CONFIGD_KEY_KEEPALIVE_APPS]);

        if (responsePayload["configs"].hasKey(CONFIGD_KEY_SUPPORT_QML_BOOSTER) &&
            responsePayload["configs"][CONFIGD_KEY_SUPPORT_QML_BOOSTER].isBoolean())
            SettingsImpl::getInstance().setSupportQMLBooster(responsePayload["configs"][CONFIGD_KEY_SUPPORT_QML_BOOSTER].asBool());

        if (responsePayload["configs"].hasKey(CONFIGD_KEY_LIFECYCLE_REASON) &&
            responsePayload["configs"][CONFIGD_KEY_LIFECYCLE_REASON].isObject())
            SettingsImpl::getInstance().setLifeCycleReason(responsePayload["configs"][CONFIGD_KEY_LIFECYCLE_REASON]);

        if (responsePayload.hasKey("missingConfigs") &&
            responsePayload["missingConfigs"].isArray() &&
            responsePayload["missingConfigs"].arraySize() > 0) {
            Logger::warning("ConfigdPrerequisiteItem", __FUNCTION__, "missing_core_config_info", responsePayload["missingConfigs"].stringify());
        }

        m_eventConnection.disconnect();
        setStatus(PrerequisiteItemStatus::PASSED);
    }

private:
    const std::string CONFIGD_KEY_FALLBACK_PRECEDENCE = "system.sysAssetFallbackPrecedence";
    const std::string CONFIGD_KEY_KEEPALIVE_APPS = "com.webos.applicationManager.keepAliveApps";
    const std::string CONFIGD_KEY_SUPPORT_QML_BOOSTER = "com.webos.applicationManager.supportQmlBooster";
    const std::string CONFIGD_KEY_LIFECYCLE_REASON = "com.webos.applicationManager.lifeCycle";

    boost::signals2::connection m_eventConnection;
};

#endif /* PREREQUISITE_CONFIGDPREREQUISITEITEM_H_ */
