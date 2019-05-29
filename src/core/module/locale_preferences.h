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

#ifndef CORE_MODULE_LOCALE_PREFERENCES_H_
#define CORE_MODULE_LOCALE_PREFERENCES_H_

#include <string>

#include <luna-service2/lunaservice.h>
#include <boost/signals2.hpp>
#include <core/util/singleton.h>
#include <pbnjson.hpp>

class LocalePreferences: public Singleton<LocalePreferences> {
friend class Singleton<LocalePreferences> ;
public:
    void init();
    void onRestInit();

    const std::string& localeInfo() const
    {
        return m_localeInfo;
    }
    const std::string& language() const
    {
        return m_language;
    }
    const std::string& script() const
    {
        return m_script;
    }
    const std::string& region() const
    {
        return m_region;
    }

    boost::signals2::signal<void(std::string, std::string, std::string)> signalLocaleChanged;

private:
    LocalePreferences();
    virtual ~LocalePreferences();

    void onSettingServiceStatusChanaged(bool connection);
    static bool onLocaleInfoReceived(LSHandle *sh, LSMessage *message, void *user_data);

    void updateLocaleInfo(const pbnjson::JValue& j_locale);
    void setLocaleInfo(const std::string& locale);

    std::string m_localeInfo;
    std::string m_language;
    std::string m_script;
    std::string m_region;

    LSMessageToken m_localeInfoToken;
};
#endif // CORE_MODULE_LOCALE_PREFERENCES_H_

