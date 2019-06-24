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

#ifndef PACKAGE_APPDESCRIPTION_H_
#define PACKAGE_APPDESCRIPTION_H_

#include <list>
#include <memory>
#include <pbnjson.hpp>
#include <stdint.h>
#include <util/Utils.h>
#include <string>
#include <tuple>

const unsigned int APP_VERSION_DIGIT = 3;

enum class AppType : int8_t {
    AppType_None = 0,
    AppType_Web,             // Web app
    AppType_Stub,            // StubApp (this is dummy type of apps)
    AppType_Native,          // Native app (general native apps)
    AppType_Native_Builtin,  // Native app (more special than general native app: e.g. Isis2, Chrome,...)
    AppType_Native_Mvpd,     // Native app (more special than general native app: e.g. Netflix, vudu,...)
    AppType_Native_Qml,      // Native app (stand alone qml apps, forked with qml runner)
    AppType_Native_AppShell, // for Native app (stand alone appshell apps, forked with appshell runner)
    AppType_Qml,             // Qml app (launch via booster)
};

enum class LifeHandlerType : int8_t {
    LifeHandlerType_None = 0,
    LifeHandlerType_Native,
    LifeHandlerType_Web,
    LifeHandlerType_Qml,
};

enum class AppTypeByDir : int8_t {
    AppTypeByDir_None = 0,
    AppTypeByDir_Store,            // /media/crptofs/apps/usr/palm/applications
    AppTypeByDir_External_Store,   // {usb}/cryptofs/apps/usr/palm/applications
    AppTypeByDir_System_Updatable, // /media/system/apps/usr/palm/applications
    AppTypeByDir_System_BuiltIn,   // /usr/palm/applications;/mnt/otycabi/usr/palm/applications;/mnt/otncabi/usr/palm/applications
    AppTypeByDir_Dev,              // /media/developer/apps/usr/palm/applications
};

class AppDescription;
typedef std::shared_ptr<AppDescription> AppDescPtr;
typedef std::tuple<uint16_t, uint16_t, uint16_t> AppIntVersion;

class AppDescription {
public:
    static std::string toString(AppType type);
    static bool getSelectedPropsFromAppInfo(const pbnjson::JValue& appinfo, const pbnjson::JValue& wanted_props, pbnjson::JValue& result);
    static bool getSelectedPropsFromApps(const pbnjson::JValue& apps, const pbnjson::JValue& wanted_props, pbnjson::JValue& result);

    static bool isHigherVersionThanMe(AppDescPtr me, AppDescPtr another);
    static bool isSame(AppDescPtr me, AppDescPtr another);

    AppDescription();
    virtual ~AppDescription();

    // getter
    AppType type() const
    {
        return m_appType;
    }
    AppTypeByDir getTypeByDir() const
    {
        return m_typeByDir;
    }
    LifeHandlerType handlerType() const
    {
        return m_handlerType;
    }
    const std::string& folderPath() const
    {
        return m_folderPath;
    }
    const std::string& id() const
    {
        return m_appId;
    }
    const std::string& title() const
    {
        return m_title;
    }
    const std::string& entryPoint() const
    {
        return m_entryPoint;
    }
    const std::string& version() const
    {
        return m_version;
    }

    const AppIntVersion& intVersion() const
    {
        return m_intVersion;
    }
    void setIntVersion(uint16_t major, uint16_t minor, uint16_t micro)
    {
        m_intVersion = {major, minor, micro};
    }
    const std::string& trustLevel() const
    {
        return m_trustLevel;
    }
    const std::string& splashBackground() const
    {
        return m_splashBackground;
    }
    int nativeInterfaceVersion() const
    {
        return m_nativeInterfaceVersion_;
    }
    bool isBuiltinBasedApp() const
    {
        return m_isBuiltinBasedApp;
    }
    bool isRemoveFlagged() const
    {
        return m_flaggedForRemoval;
    }
    bool isRemovable() const
    {
        return m_removable;
    }
    bool isVisible() const
    {
        return m_visible;
    }
    int requiredMemory() const
    {
        return m_requiredMemory;
    }
    const std::string& defaultWindowType() const
    {
        return m_defaultWindowType;
    }
    bool windowGroup() const
    {
        return m_windowGroup;
    }
    bool windowGroupOwner() const
    {
        return m_windowGroupOwner;
    }
    bool isChildWindow() const
    {
        return (m_windowGroup && !m_windowGroupOwner);
    }
    const std::string& windowGroupOwnerId() const
    {
        return m_windowGroupOwnerId;
    }
    bool canExecute() const
    {
        return !m_isLockedForExcution;
    }
    const pbnjson::JValue& redirection()
    {
        return m_redirection;
    }
    bool splashOnLaunch() const
    {
        return m_splashOnLaunch;
    }
    bool spinnerOnLaunch() const
    {
        return m_spinnerOnLaunch;
    }
    const pbnjson::JValue& launchParams() const
    {
        return launch_params_;
    }
    const std::string& containerJS() const
    {
        return m_containerJS;
    }
    const std::string& enyoVersion() const
    {
        return m_enyoVersion;
    }
    const pbnjson::JValue& getDeviceId() const
    {
        return m_deviceId;
    }
    pbnjson::JValue toJValue() const
    {
        return m_appinfoJson;
    }

    // feature functions
    bool isPrivileged() const;
    bool securityChecksVerified();
    bool doesMatchKeywordExact(const gchar* keyword) const;
    bool doesMatchKeywordPartial(const gchar* keyword) const;

    virtual bool loadJson(pbnjson::JValue& jdesc, const AppTypeByDir& type_by_dir);

protected:
    // make this class on-copyable
    AppDescription& operator=(const AppDescription& appDesc) = delete;
    AppDescription(const AppDescription& appDesc) = delete;

    void loadAsset(pbnjson::JValue& jdesc);

    AppType m_appType;
    AppTypeByDir m_typeByDir;
    LifeHandlerType m_handlerType;
    std::string m_folderPath;
    std::string m_appId;
    std::string m_title;    //copy of default launch point's title
    std::string m_entryPoint;
    std::string m_version;
    AppIntVersion m_intVersion;
    std::string m_trustLevel;
    std::string m_splashBackground;
    int m_nativeInterfaceVersion_;
    bool m_isBuiltinBasedApp;
    bool m_flaggedForRemoval;
    bool m_removable;
    bool m_visible;
    int m_requiredMemory;
    std::string m_defaultWindowType;
    bool m_windowGroup;
    bool m_windowGroupOwner;
    std::string m_windowGroupOwnerId;
    bool m_isLockedForExcution;
    pbnjson::JValue m_redirection;
    bool m_splashOnLaunch;
    bool m_spinnerOnLaunch;
    pbnjson::JValue launch_params_;
    std::string m_containerJS;
    std::string m_enyoVersion;
    pbnjson::JValue m_deviceId;
    pbnjson::JValue m_appinfoJson;

};

#endif // PACKAGE_APPDESCRIPTION_H_

