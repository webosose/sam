// Copyright (c) 2012-2020 LG Electronics, Inc.
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

#include "SettingService.h"

#include <stdlib.h>
#include <unicode/locid.h>

#include "base/AppDescriptionList.h"
#include "bus/service/ApplicationManager.h"
#include "conf/SAMConf.h"
#include "util/JValueUtil.h"

SettingService::SettingService()
    : AbsLunaClient("com.webos.settingsservice")
{
    setClassName("SettingService");

    JValue localeInfo = JDomParser::fromFile(PATH_LOCALE_INFO);
    updateLocaleInfo(localeInfo);
}

SettingService::~SettingService()
{
}

void SettingService::onInitialzed()
{

}

void SettingService::onFinalized()
{
    m_getSystemSettingsCall.cancel();
}

void SettingService::onServerStatusChanged(bool isConnected)
{
    static string method = string("luna://") + getName() + string("/getSystemSettings");

    if (isConnected) {
        JValue requestPayload = pbnjson::Object();
        requestPayload.put("subscribe", true);
        requestPayload.put("key", "localeInfo");

        m_getSystemSettingsCall = ApplicationManager::getInstance().callMultiReply(
            method.c_str(),
            requestPayload.stringify().c_str(),
            onLocaleChanged,
            nullptr
        );
    } else {
        m_getSystemSettingsCall.cancel();
    }
}

bool SettingService::onCheckParentalLock(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    if (responsePayload.isNull())
        return true;

    bool returnValue = false;
    string errorText;

    bool isParentalControlValid = false;
    bool parentalControl = false;
    bool isApplockPerAppValid = false;
    bool applockPerApp = false;

    JValueUtil::getValue(responsePayload, "returnValue", returnValue);
    JValueUtil::getValue(responsePayload, "errorText", errorText);

    if (!responsePayload.hasKey("results") || !responsePayload["results"].isArray()) {
        Logger::debug(getInstance().getClassName(), __FUNCTION__, Logger::format("result fail: %s", responsePayload.stringify().c_str()));
        goto Done;
    }

    for (int i = 0; i < responsePayload["results"].arraySize(); ++i) {
        if (!responsePayload["results"][i].hasKey("settings") || !responsePayload["results"][i]["settings"].isObject())
            continue;

        if (responsePayload["results"][i]["settings"].hasKey("parentalControl") && responsePayload["results"][i]["settings"]["parentalControl"].isBoolean()) {
            parentalControl = responsePayload["results"][i]["settings"]["parentalControl"].asBool();
            isParentalControlValid = true;
        }

        if (responsePayload["results"][i]["settings"].hasKey("applockPerApp") && responsePayload["results"][i]["settings"]["applockPerApp"].isBoolean()) {
            applockPerApp = responsePayload["results"][i]["settings"]["applockPerApp"].asBool();
            isApplockPerAppValid = true;
        }
    }

    if (!isParentalControlValid || !isApplockPerAppValid) {
        Logger::debug("CheckAppLockStatus", __FUNCTION__, Logger::format("receiving valid result fail: %s", responsePayload.stringify().c_str()));
        goto Done;
    }

    if (parentalControl && applockPerApp) {
        // Notification::getInstance().createPincodePrompt(onCreatePincodePrompt);
        return true;
    }

Done:
    // TODO: appId should be passed
    // AppPackageManager::getInstance().removeApp(appId, false, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
    return true;
}

Call SettingService::checkParentalLock(LSFilterFunc func, const string& appId)
{
    static string method = string("luna://") + getName() + string("/batch");
    JValue requestPayload = pbnjson::Object();
    JValue operations = pbnjson::Array();

    JValue parentalControl = pbnjson::Object();
    parentalControl.put("method", "getSystemSettings");
    parentalControl.put("params", pbnjson::Object());
    parentalControl["params"].put("category", "lock");
    parentalControl["params"].put("key", "parentalControl");
    operations.append(parentalControl);

    JValue applockPerApp = pbnjson::Object();
    applockPerApp.put("method", "getSystemSettings");
    applockPerApp.put("params", pbnjson::Object());
    applockPerApp["params"].put("category", "lock");
    applockPerApp["params"].put("key", "applockPerApp");
    applockPerApp["params"].put("appId", appId);
    operations.append(applockPerApp);

    requestPayload.put("operations", operations);

    Call call = ApplicationManager::getInstance().callOneReply(method.c_str(), requestPayload.stringify().c_str(), func, nullptr);
    return call;
}

bool SettingService::onLocaleChanged(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue subscriptionPayload = JDomParser::fromString(response.getPayload());
    Logger::logSubscriptionResponse(getInstance().getClassName(), __FUNCTION__, response, subscriptionPayload);

    if (subscriptionPayload.isNull())
        return true;

    bool returnValue = true;
    if (!JValueUtil::getValue(subscriptionPayload, "returnValue", returnValue) || !returnValue) {
        Logger::warning(getInstance().getClassName(), __FUNCTION__, "received invaild message from settings service");
        return true;
    }

    if (!subscriptionPayload["settings"].isObject()) {
        return true;
    }

    SettingService::getInstance().updateLocaleInfo(subscriptionPayload["settings"]);
    return true;
}

void SettingService::updateLocaleInfo(const JValue& settings)
{
    string localeInfo;

    if (settings.isNull() || !settings.isObject()) {
        return;
    }

    if (!JValueUtil::getValue(settings, "localeInfo", "locales", "UI", localeInfo)) {
        return;
    }

    if (localeInfo.empty() || localeInfo == m_localeInfo) {
        return;
    }

    m_localeInfo = std::move(localeInfo);
    string language;
    string script;
    string region;

    if (m_localeInfo.empty()) {
        language = "";
        script = "";
        region = "";
    } else {
        icu::Locale icu_UI_locale = icu::Locale::createFromName(m_localeInfo.c_str());
        language = icu_UI_locale.getLanguage();
        script = icu_UI_locale.getScript();
        region = icu_UI_locale.getCountry();
    }

    if (language == SAMConf::getInstance().getLanguage() &&
        script == SAMConf::getInstance().getScript() &&
        region == SAMConf::getInstance().getRegion()) {
        Logger::info(getClassName(), __FUNCTION__, "Same localization info");
        return;
    }

    Logger::info(getClassName(), __FUNCTION__, "Changed Locale",
                 Logger::format("language(%s=>%s) script(%s=>%s) region(%s=>%s)",
                 SAMConf::getInstance().getLanguage().c_str(), language.c_str(),
                 SAMConf::getInstance().getScript().c_str(), script.c_str(),
                 SAMConf::getInstance().getRegion().c_str(), region.c_str()));

    SAMConf::getInstance().setLocale(language, script, region);
    AppDescriptionList::getInstance().changeLocale();

}
