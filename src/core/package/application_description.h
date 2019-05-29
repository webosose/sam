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

#ifndef CORE_PACKAGE_APPLICATION_DESCRIPTION_H_
#define CORE_PACKAGE_APPLICATION_DESCRIPTION_H_

#include <core/util/utils.h>
#include <stdint.h>

#include <string>
#include <list>
#include <memory>
#include <tuple>
#include <pbnjson.hpp>

//#include "core/package/keyword_map.h"

const unsigned int APP_VERSION_DIGIT = 3;

enum class AppType : int8_t {
    None = 0,
    Web,             // Web app
    Stub,            // StubApp (this is dummy type of apps)
    Native,          // Native app (general native apps)
    Native_Builtin,  // Native app (more special than general native app: e.g. Isis2, Chrome,...)
    Native_Mvpd,     // Native app (more special than general native app: e.g. Netflix, vudu,...)
    Native_Qml,      // Native app (stand alone qml apps, forked with qml runner)
    Native_AppShell, // for Native app (stand alone appshell apps, forked with appshell runner)
    Qml,             // Qml app (launch via booster)
};

enum class LifeHandlerType : int8_t {
    None = 0,
    Native,
    Web,
    Qml,
};

enum class AppTypeByDir : int8_t {
    None = 0,
    External_Shared,  // {usb}/sharedcryptofs/apps/usr/palm/applications
    Store,            // /media/crptofs/apps/usr/palm/applications
    External_Store,   // {usb}/cryptofs/apps/usr/palm/applications
    System_Updatable, // /media/system/apps/usr/palm/applications
    System_BuiltIn,   // /usr/palm/applications;/mnt/otycabi/usr/palm/applications;/mnt/otncabi/usr/palm/applications
    Dev,              // /media/developer/apps/usr/palm/applications
    Alias,            // /media/alias/apps/usr/palm/applications
    TempAlias         // /tmp/alias/apps/usr/palm/applications
};

class ApplicationDescription;
typedef std::shared_ptr<ApplicationDescription> AppDescPtr;
typedef std::tuple<uint16_t, uint16_t, uint16_t> AppIntVersion;

class ApplicationDescription {
public:
    ApplicationDescription();
    virtual ~ApplicationDescription();

    // getter
    AppType type() const
    {
        return m_appType;
    }
    AppTypeByDir getTypeByDir() const
    {
        return m_typeByDir;
    }
    LifeHandlerType handler_type() const
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
    const AppIntVersion& IntVersion() const
    {
        return m_intVersion;
    }
    const std::string& trustLevel() const
    {
        return m_trustLevel;
    }
    const std::string& splashBackground() const
    {
        return m_splashBackground;
    }
    int NativeInterfaceVersion() const
    {
        return m_nativeInterfaceVersion_;
    }
    bool IsBuiltinBasedApp() const
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
    bool window_group() const
    {
        return m_windowGroup;
    }
    bool window_group_owner() const
    {
        return m_windowGroupOwner;
    }
    bool is_child_window() const
    {
        return (m_windowGroup && !m_windowGroupOwner);
    }
    const std::string& window_group_owner_id() const
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
    bool isLockable() const
    {
        return m_lockable;
    }
    bool splashOnLaunch() const
    {
        return m_splashOnLaunch;
    }
    bool spinnerOnLaunch() const
    {
        return m_spinnerOnLaunch;
    }
    const pbnjson::JValue& getBootParams()
    {
        return m_bootParams;
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

    // setter
    void SetIntVersion(uint16_t major, uint16_t minor, uint16_t micro)
    {
        m_intVersion = {major, minor, micro};}
    void flagForRemoval(bool rf=true) {m_flaggedForRemoval = rf;}
    void executionLock(bool xp=true) {m_isLockedForExcution = xp;}
    void setLaunchParams(const pbnjson::JValue& launchParams)
    {
        launch_params_ = launchParams.duplicate();
        m_appinfoJson.put("launchParams", launch_params_);
    }
    void setDeviceId(const pbnjson::JValue& deviceId) {m_deviceId = deviceId;}

    // feature functions
    bool isPrivileged() const;
    bool securityChecksVerified();
    bool doesMatchKeywordExact(const gchar* keyword) const;
    bool doesMatchKeywordPartial(const gchar* keyword) const;

    static std::string appTypeToString(AppType type);
    static bool getSelectedPropsFromAppInfo(const pbnjson::JValue& appinfo,
            const pbnjson::JValue& wanted_props, pbnjson::JValue& result);
    static bool getSelectedPropsFromApps(const pbnjson::JValue& apps,
            const pbnjson::JValue& wanted_props, pbnjson::JValue& result);
    static AppVersion GetAppVersionStruct(std::string strVersion);

    static bool IsHigherVersionThanMe(AppDescPtr me, AppDescPtr another);
    static bool IsSame(AppDescPtr me, AppDescPtr another);

protected:
    // make this class on-copyable
    ApplicationDescription& operator=(const ApplicationDescription& appDesc) = delete;
    ApplicationDescription(const ApplicationDescription& appDesc) = delete;

    virtual bool LoadJson(pbnjson::JValue& jdesc, const AppTypeByDir& type_by_dir);
    void LoadAsset(pbnjson::JValue& jdesc);

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
    bool m_lockable;
    bool m_splashOnLaunch;
    bool m_spinnerOnLaunch;
    pbnjson::JValue m_bootParams;
    pbnjson::JValue launch_params_;
    std::string m_containerJS;
    std::string m_enyoVersion;
    pbnjson::JValue m_deviceId;
    pbnjson::JValue m_appinfoJson;

};

#endif // CORE_PACKAGE_APPLICATION_DESCRIPTION_H_

