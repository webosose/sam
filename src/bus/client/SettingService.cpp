// Copyright (c) 2012-2019 LG Electronics, Inc.
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

#include <bus/client/SettingService.h>
#include <bus/ResBundleAdaptor.h>
#include <bus/service/ApplicationManager.h>
#include <stdlib.h>
#include <unicode/locid.h>

#include <pbnjson.hpp>
#include <setting/Settings.h>

#include <util/LSUtils.h>

const string SettingService::NAME = "com.webos.settingsservice";

SettingService::SettingService()
    : AbsLunaClient(NAME)
{
    setClassName(NAME);
}

SettingService::~SettingService()
{
}

void SettingService::onInitialze()
{

}

void SettingService::onServerStatusChanged(bool isConnected)
{
    static std::string method = std::string("luna://") + getName() + std::string("/getSystemSettings");

    if (isConnected) {
        JValue requestPayload = pbnjson::Object();
        requestPayload.put("subscribe", "true");
        requestPayload.put("key", "localeInfo");

        m_getSystemSettingsCall = ApplicationManager::getInstance().callMultiReply(
            method.c_str(),
            requestPayload.stringify().c_str(),
            onGetSystemSettings,
            nullptr
        );
    } else {
        m_getSystemSettingsCall.cancel();
    }
}

void SettingService::onRestInit()
{
    pbnjson::JValue localeInfo = JDomParser::fromFile(SettingsImpl::getInstance().m_localeInfoPath.c_str());
    updateLocaleInfo(localeInfo);
}

Call SettingService::batch(LSFilterFunc func, const string& appId)
{
    static string method = std::string("luna://") + getName() + std::string("/batch");
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

bool SettingService::onGetSystemSettings(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue json = JDomParser::fromString(LSMessageGetPayload(message));

    Logger::debug(getInstance().getClassName(), __FUNCTION__, "subscription-client", json.stringify());

    if (json.isNull() || !json["returnValue"].asBool() || !json["settings"].isObject()) {
        Logger::warning(getInstance().getClassName(), __FUNCTION__, "lscall", "received invaild message from settings service");
        return true;
    }

    SettingService::getInstance().updateLocaleInfo(json["settings"]);

    return true;
}

void SettingService::updateLocaleInfo(const pbnjson::JValue& j_locale)
{
    std::string ui_locale_info;

    // load language info
    if (j_locale.isNull() || j_locale.isObject() == false)
        return;

    if (j_locale.hasKey("localeInfo") &&
        j_locale["localeInfo"].hasKey("locales") &&
        j_locale["localeInfo"]["locales"].hasKey("UI")) {
        ui_locale_info = j_locale["localeInfo"]["locales"]["UI"].asString();
    }

    if (!ui_locale_info.empty() && ui_locale_info != m_localeInfo) {
        m_localeInfo = ui_locale_info;
        setLocaleInfo(m_localeInfo);
    }
}

/// separate the substrings of BCP-47 (langauge-Script-COUNTRY)
void SettingService::setLocaleInfo(const std::string& locale)
{
    std::string language;
    std::string script;
    std::string region;

    if (locale.empty()) {
        language = "";
        script = "";
        region = "";
    } else {
        icu::Locale icu_UI_locale = icu::Locale::createFromName(locale.c_str());

        language = icu_UI_locale.getLanguage();
        script = icu_UI_locale.getScript();
        region = icu_UI_locale.getCountry();
    }

    if (language == m_language && script == m_script && region == m_region) {
        return;
    }

    m_language = language;
    m_script = script;
    m_region = region;

    Logger::info(getClassName(), __FUNCTION__, "lscall",
                 Logger::format("locale(%s) language(%s) script(%s) region(%s)", locale.c_str(), m_language.c_str(), m_script.c_str(), m_region.c_str()));

    ResBundleAdaptor::getInstance().setLocale(locale);
    EventLocaleChanged(m_language, m_script, m_region);
}
