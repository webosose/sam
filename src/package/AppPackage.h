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

#ifndef PACKAGE_APPPACKAGE_H_
#define PACKAGE_APPPACKAGE_H_

#include <list>
#include <memory>
#include <pbnjson.hpp>
#include <stdint.h>
#include "interface/IClassName.h"
#include <string>
#include <tuple>
#include "util/Logger.h"

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

class AppPackage;

typedef std::shared_ptr<AppPackage> AppPackagePtr;
typedef std::tuple<uint16_t, uint16_t, uint16_t> AppIntVersion;

class AppPackage : public IClassName {
public:
    static std::string toString(AppType type);
    static bool getSelectedPropsFromAppInfo(const pbnjson::JValue& appinfo, const pbnjson::JValue& wanted_props, pbnjson::JValue& result);
    static bool getSelectedPropsFromApps(const pbnjson::JValue& apps, const pbnjson::JValue& wanted_props, pbnjson::JValue& result);

    static bool isHigherVersionThanMe(AppPackagePtr me, AppPackagePtr another);
    static bool isSame(AppPackagePtr me, AppPackagePtr another);

    AppPackage();
    virtual ~AppPackage();

    // getter
    AppType getAppType() const
    {
        return m_appType;
    }
    AppTypeByDir getTypeByDir() const
    {
        return m_typeByDir;
    }
    LifeHandlerType getHandlerType() const
    {
        return m_handlerType;
    }
    const std::string& getFolderPath() const
    {
        return m_folderPath;
    }
    const std::string& getAppId() const
    {
        return m_appId;
    }
    const std::string& getTitle() const
    {
        return m_title;
    }
    const std::string& getMain() const
    {
        return m_main;
    }
    const std::string& getVersion() const
    {
        return m_version;
    }

    void setIntVersion(uint16_t major, uint16_t minor, uint16_t micro)
    {
        m_intVersion = {major, minor, micro};
    }
    const AppIntVersion& getIntVersion() const
    {
        return m_intVersion;
    }

    const std::string& getSplashBackground() const
    {
        return m_splashBackground;
    }
    int getNativeInterfaceVersion() const
    {
        return m_nativeInterfaceVersion;
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
    int getRequiredMemory() const
    {
        return m_requiredMemory;
    }
    const std::string& getDefaultWindowType() const
    {
        return m_defaultWindowType;
    }
    bool getWindowGroup() const
    {
        return m_isWindowGroup;
    }
    bool getWindowGroupOwner() const
    {
        return m_isWindowGroupOwner;
    }
    bool isChildWindow() const
    {
        return (m_isWindowGroup && !m_isWindowGroupOwner);
    }
    bool getSplashOnLaunch() const
    {
        return m_splashOnLaunch;
    }
    bool getSpinnerOnLaunch() const
    {
        return m_spinnerOnLaunch;
    }

    void lock()
    {
        m_isLocked = true;
    }
    void unlock()
    {
        m_isLocked = false;
    }
    bool isLocked() const
    {
        return m_isLocked;
    }
    pbnjson::JValue toJValue() const
    {
        return m_appinfo;
    }

    // feature functions
    bool isPrivileged() const;
    bool securityChecksVerified();

    virtual bool loadJson(pbnjson::JValue& appinfo, const AppTypeByDir& type_by_dir);

protected:
    // make this class on-copyable
    AppPackage& operator=(const AppPackage& appDesc) = delete;
    AppPackage(const AppPackage& appDesc) = delete;

    void loadAsset(pbnjson::JValue& appinfo);

    AppType m_appType;
    AppTypeByDir m_typeByDir;
    LifeHandlerType m_handlerType;

    std::string m_appId;
    std::string m_folderPath;
    std::string m_title;
    std::string m_main;
    std::string m_version;
    std::string m_splashBackground;

    AppIntVersion m_intVersion;
    int m_nativeInterfaceVersion;

    bool m_isBuiltinBasedApp;
    bool m_flaggedForRemoval;
    bool m_removable;
    bool m_visible;
    int m_requiredMemory;
    std::string m_defaultWindowType;
    bool m_isWindowGroup;
    bool m_isWindowGroupOwner;
    bool m_splashOnLaunch;
    bool m_spinnerOnLaunch;
    bool m_isLocked;

    pbnjson::JValue m_appinfo;

};

#endif // PACKAGE_APPPACKAGE_H_

