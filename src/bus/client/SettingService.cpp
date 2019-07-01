// Copyright (c) 2012-2018 LG Electronics, Inc.
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

#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/LSUtils.h>

SettingService::SettingService()
    : AbsLunaClient("com.webos.settingsservice"),
      m_localeInfoToken(0)
{
}

SettingService::~SettingService()
{
}

void SettingService::onInitialze()
{

}

void SettingService::onServerStatusChanged(bool isConnected)
{

}

void SettingService::onRestInit()
{

    pbnjson::JValue locale_info = JUtil::parseFile(SettingsImpl::getInstance().m_localeInfoPath, std::string(""));
    updateLocaleInfo(locale_info);
}

void SettingService::onSettingServiceStatusChanaged(bool connection)
{
    if (connection) {
        if (m_localeInfoToken == 0) {
            std::string payload = "{\"subscribe\":true,\"key\":\"localeInfo\"}";
            if (!LSCall(ApplicationManager::getInstance().get(),
                        "luna://com.webos.settingsservice/getSystemSettings",
                        payload.c_str(),
                        onLocaleInfoReceived,
                        this,
                        &m_localeInfoToken,
                        NULL)) {
                LOG_WARNING(MSGID_LSCALL_ERR, 2,
                            PMLOGKS(LOG_KEY_TYPE, "lscall"),
                            PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                            "failed_subscribing_settings");
            }
        }
    } else {
        if (0 != m_localeInfoToken) {
            (void) LSCallCancel(ApplicationManager::getInstance().get(), m_localeInfoToken, NULL);
            m_localeInfoToken = 0;
        }
    }
}

bool SettingService::onLocaleInfoReceived(LSHandle* sh, LSMessage* message, void* user_data)
{
    SettingService* s_this = static_cast<SettingService*>(user_data);
    if (s_this == NULL)
        return false;

    pbnjson::JValue json = JUtil::parse(LSMessageGetPayload(message), std::string(""));

    LOG_DEBUG("[RESPONSE-SUBSCRIPTION]: %s", json.stringify().c_str());

    if (json.isNull() || !json["returnValue"].asBool() || !json["settings"].isObject()) {
        LOG_WARNING(MSGID_RECEIVED_INVALID_SETTINGS, 1,
                    PMLOGKS(LOG_KEY_REASON, "invalid_return"),
                    "received invaild message from settings service");
        return true;
    }

    s_this->updateLocaleInfo(json["settings"]);

    return true;
}

void SettingService::updateLocaleInfo(const pbnjson::JValue& j_locale)
{

    std::string ui_locale_info;

    // load language info
    if (j_locale.isNull() || j_locale.isObject() == false)
        return;

    if (j_locale.hasKey("localeInfo") && j_locale["localeInfo"].hasKey("locales") && j_locale["localeInfo"]["locales"].hasKey("UI")) {
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

    LOG_INFO(MSGID_LANGUAGE_SET_CHANGE, 4,
             PMLOGKS("locale", locale.c_str()),
             PMLOGKS("language", m_language.c_str()),
             PMLOGKS("script", m_script.c_str()),
             PMLOGKS("region", m_region.c_str()), "");

    ResBundleAdaptor::getInstance().setLocale(locale);
    EventLocaleChanged(m_language, m_script, m_region);
}
