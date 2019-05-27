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

#include "core/module/locale_preferences.h"

#include <stdlib.h>
#include <unicode/locid.h>

#include <pbnjson.hpp>

#include "core/base/jutil.h"
#include "core/base/logging.h"
#include "core/base/lsutils.h"
#include "core/bus/appmgr_service.h"
#include "core/module/notification.h"
#include "core/module/service_observer.h"
#include "core/setting/settings.h"

LocalePreferences::LocalePreferences() :
        locale_info_token_(0)
{
}

LocalePreferences::~LocalePreferences()
{
}

void LocalePreferences::Init()
{
}

void LocalePreferences::OnRestInit()
{

    pbnjson::JValue locale_info = JUtil::parseFile(SettingsImpl::instance().localeInfoPath, std::string(""));
    UpdateLocaleInfo(locale_info);

    ServiceObserver::instance().Add(WEBOS_SERVICE_SETTINGS, std::bind(&LocalePreferences::OnSettingServiceStatusChanaged, this, std::placeholders::_1));
}

void LocalePreferences::OnSettingServiceStatusChanaged(bool connection)
{
    if (connection) {
        if (locale_info_token_ == 0) {
            std::string payload = "{\"subscribe\":true,\"key\":\"localeInfo\"}";
            if (!LSCall(AppMgrService::instance().serviceHandle(), "luna://com.webos.settingsservice/getSystemSettings", payload.c_str(), OnLocaleInfoReceived, this, &locale_info_token_, NULL)) {
                LOG_WARNING(MSGID_LSCALL_ERR, 2, PMLOGKS("type", "lscall"), PMLOGKS("where", __FUNCTION__), "failed_subscribing_settings");
            }
        }
    } else {
        if (0 != locale_info_token_) {
            (void) LSCallCancel(AppMgrService::instance().serviceHandle(), locale_info_token_, NULL);
            locale_info_token_ = 0;
        }
    }
}

bool LocalePreferences::OnLocaleInfoReceived(LSHandle* sh, LSMessage* message, void* user_data)
{
    LocalePreferences* s_this = static_cast<LocalePreferences*>(user_data);
    if (s_this == NULL)
        return false;

    pbnjson::JValue json = JUtil::parse(LSMessageGetPayload(message), std::string(""));

    LOG_DEBUG("[RESPONSE-SUBSCRIPTION]: %s", json.stringify().c_str());

    if (json.isNull() || !json["returnValue"].asBool() || !json["settings"].isObject()) {
        LOG_WARNING(MSGID_RECEIVED_INVALID_SETTINGS, 1, PMLOGKS("reason", "invalid_return"), "received invaild message from settings service");
        return true;
    }

    s_this->UpdateLocaleInfo(json["settings"]);

    return true;
}

void LocalePreferences::UpdateLocaleInfo(const pbnjson::JValue& j_locale)
{

    std::string ui_locale_info;

    // load language info
    if (j_locale.isNull() || j_locale.isObject() == false)
        return;

    if (j_locale.hasKey("localeInfo") && j_locale["localeInfo"].hasKey("locales") && j_locale["localeInfo"]["locales"].hasKey("UI")) {
        ui_locale_info = j_locale["localeInfo"]["locales"]["UI"].asString();
    }

    if (!ui_locale_info.empty() && ui_locale_info != locale_info_) {
        locale_info_ = ui_locale_info;
        SetLocaleInfo(locale_info_);
    }

}

/// separate the substrings of BCP-47 (langauge-Script-COUNTRY)
void LocalePreferences::SetLocaleInfo(const std::string& locale)
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

    if (language == language_ && script == script_ && region == region_) {
        return;
    }

    language_ = language;
    script_ = script;
    region_ = region;

    LOG_INFO(MSGID_LANGUAGE_SET_CHANGE, 4, PMLOGKS("locale", locale.c_str()), PMLOGKS("language", language_.c_str()), PMLOGKS("script", script_.c_str()), PMLOGKS("region", region_.c_str()), "");

    ResBundleAdaptor::instance().setLocale(locale);
    signalLocaleChanged(language_, script_, region_);
}
