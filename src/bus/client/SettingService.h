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

#ifndef BUS_CLIENT_SETTINGSERVICE_H_
#define BUS_CLIENT_SETTINGSERVICE_H_

#include "AbsLunaClient.h"
#include "interface/ISingleton.h"
#include "interface/IClassName.h"
#include <luna-service2/lunaservice.hpp>
#include <boost/signals2.hpp>
#include <pbnjson.hpp>
#include "util/Logger.h"

using namespace LS;
using namespace pbnjson;

class SettingService : public ISingleton<SettingService>,
                       public AbsLunaClient,
                       public IClassName {
friend class ISingleton<SettingService>;
public:
    virtual ~SettingService();

    // AbsLunaClient
    virtual void onInitialze();
    virtual void onServerStatusChanged(bool isConnected);

    void onRestInit();

    // API
    Call batch(LSFilterFunc func, const string& appId);

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

    boost::signals2::signal<void(std::string, std::string, std::string)> EventLocaleChanged;

private:
    static const string NAME;
    static bool onGetSystemSettings(LSHandle* sh, LSMessage* message, void* context);

    SettingService();

    void updateLocaleInfo(const pbnjson::JValue& j_locale);
    void setLocaleInfo(const std::string& locale);

    std::string m_localeInfo;
    std::string m_language;
    std::string m_script;
    std::string m_region;

    Call m_getSystemSettingsCall;

};
#endif // BUS_CLIENT_SETTINGSERVICE_H_

