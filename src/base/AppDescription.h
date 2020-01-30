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

#ifndef BASE_APPDESCRIPTION_H_
#define BASE_APPDESCRIPTION_H_

#include <list>
#include <memory>
#include <pbnjson.hpp>
#include <stdint.h>
#include <string>
#include <tuple>

#include "interface/IClassName.h"
#include "util/JValueUtil.h"
#include "util/Logger.h"

const unsigned int APP_VERSION_DIGIT = 3;

enum class AppStatusEvent : int8_t {
    AppStatusEvent_Nothing = 0,
    AppStatusEvent_Installed,
    AppStatusEvent_Uninstalled,
    AppStatusEvent_UpdateCompleted,
};

enum class AppType : int8_t {
    AppType_None = 0,
    AppType_Web,             // Web app
    AppType_Stub,            // StubApp (this is dummy type of apps)
    AppType_Native,          // Native app (general native apps)
    AppType_Native_Builtin,  // Native app (more special than general native app: e.g. Isis2, Chrome,...)
    AppType_Native_Mvpd,     // Native app (more special than general native app: e.g. Netflix, vudu,...)
    AppType_Native_Qml,      // Native app (stand alone qml apps, forked with qml runner)
    AppType_Native_AppShell // for Native app (stand alone appshell apps, forked with appshell runner)
};

enum class AppLocation : int8_t {
    AppLocation_None = 0,
    AppLocation_AppStore_Internal,  // /media/crptofs/apps/usr/palm/applications
    AppLocation_AppStore_External,  // {usb}/cryptofs/apps/usr/palm/applications
    AppLocation_System_ReadWrite,   // /media/system/apps/usr/palm/applications
    AppLocation_System_ReadOnly,    // /usr/palm/applications;/mnt/otycabi/usr/palm/applications;/mnt/otncabi/usr/palm/applications
    AppLocation_Devmode,            // /media/developer/apps/usr/palm/applications
};

enum class LifeHandlerType : int8_t {
    LifeHandlerType_None = 0,
    LifeHandlerType_Native,
    LifeHandlerType_Web
};

class AppDescription;

typedef shared_ptr<AppDescription> AppDescriptionPtr;
typedef tuple<uint16_t, uint16_t, uint16_t> AppIntVersion;

class AppDescription {
friend class AppDescriptionList;
public:
    static string toString(const AppStatusEvent& event);

    static string toString(AppType type);
    static AppType toAppType(const string& type);

    static const char* toString(AppLocation location);
    static AppLocation toAppLocation(const string& type);

    AppDescription(const string& appId);
    virtual ~AppDescription();

    bool scan(const string& folderPath, const AppLocation& appLocation);
    bool rescan();

    void applyFolderPath(string& path);
    bool loadAppinfo();

    bool isLocked() const
    {
        return m_isLocked;
    }
    void lock()
    {
        m_isLocked = true;
    }
    void unlock()
    {
        m_isLocked = false;
    }

    JValue getJson(JValue& properties);

    JValue& getJson()
    {
        return m_appinfo;
    }

    void toJson(JValue& json)
    {
        json = m_appinfo.duplicate();
    }

    const string& getFolderPath() const
    {
        return m_folderPath;
    }
    void setFolderPath(const string& folderPath)
    {
        m_folderPath = folderPath;
    }

    AppLocation getAppLocation() const
    {
        return m_appLocation;
    }
    void setAppLocation(const AppLocation& location)
    {
        m_appLocation = location;
    }

    LifeHandlerType getLifeHandlerType() const
    {
        return m_handlerType;
    }

    // Just getter
    const string getAppDescription() const
    {
        string appDescription = "";
        JValueUtil::getValue(m_appinfo, "appDescription", appDescription);
        return appDescription;
    }

    const string& getAppId() const
    {
        return m_appId;
    }

    const string& getAbsMain() const
    {
        return m_absMain;
    }

    AppType getAppType() const
    {
        return m_appType;
    }

    const string getBgColor() const
    {
        string bgColor = "";
        JValueUtil::getValue(m_appinfo, "bgColor", bgColor);
        return bgColor;
    }

    const string getBgImage() const
    {
        string bgImage = "";
        JValueUtil::getValue(m_appinfo, "bgImage", bgImage);
        return bgImage;
    }

    const string getDefaultWindowType() const
    {
        string defaultWindowType = "";
        JValueUtil::getValue(m_appinfo, "defaultWindowType", defaultWindowType);
        return defaultWindowType;
    }

    const string getIcon() const
    {
        string icon = "";
        JValueUtil::getValue(m_appinfo, "icon", icon);
        return icon;
    }

    const AppIntVersion& getIntVersion() const
    {
        return m_intVersion;
    }

    const string getLargeIcon() const
    {
        string largeIcon = "";
        JValueUtil::getValue(m_appinfo, "largeIcon", largeIcon);
        return largeIcon;
    }

    int getNativeInterfaceVersion() const
    {
        int nativeLifeCycleInterfaceVersion = 1;
        JValueUtil::getValue(m_appinfo, "nativeLifeCycleInterfaceVersion", nativeLifeCycleInterfaceVersion);
        return nativeLifeCycleInterfaceVersion;
    }

    int getRequiredMemory() const
    {
        int requiredMemory = 0;
        JValueUtil::getValue(m_appinfo, "requiredMemory", requiredMemory);
        return requiredMemory;
    }

    const string& getSplashBackground() const
    {
        return m_absSplashBackground;
    }

    const string getTitle() const
    {
        string title = "";
        JValueUtil::getValue(m_appinfo, "title", title);
        return title;
    }

    bool isAllowedAppId()
    {
        if (m_folderPath.length() <= m_appId.length() ||
            strcmp(m_folderPath.c_str() + m_folderPath.length() - m_appId.length(), m_appId.c_str()) != 0) {
            //Logger::error(CLASS_NAME, __FUNCTION__, m_appId, "App path does not match app id");
            return false;
        }
        return true;
    }

    bool isDevmodeApp() const
    {
        if (m_appLocation == AppLocation::AppLocation_Devmode)
        return true;
        return false;
    }

    bool isNoSplashOnLaunch() const
    {
        bool noSplashOnLaunch = true;
        JValueUtil::getValue(m_appinfo, "noSplashOnLaunch", noSplashOnLaunch);
        return noSplashOnLaunch;
    }

    bool isPrivilegedAppId()
    {
        if (m_appId.find("com.palm.") == 0 ||
            m_appId.find("com.webos.") == 0 ||
            m_appId.find("com.lge.") == 0)
            return true;
        return false;
    }

    bool isRemovable() const
    {
        if (AppLocation::AppLocation_System_ReadOnly == m_appLocation ||
            AppLocation::AppLocation_System_ReadWrite == m_appLocation) {
            return false;
        } else {
            bool removable = true;
            JValueUtil::getValue(m_appinfo, "removable", removable);
            return removable;
        }
    }

    bool isSpinnerOnLaunch() const
    {
        bool spinnerOnLaunch = false;
        JValueUtil::getValue(m_appinfo, "spinnerOnLaunch", spinnerOnLaunch);
        return spinnerOnLaunch;
    }

    bool isSystemApp() const
    {
        switch(m_appLocation) {
        case AppLocation::AppLocation_System_ReadOnly:
        case AppLocation::AppLocation_System_ReadWrite:
        return true;

        default:
        return false;
        }
        return false;
    }

    bool isUnmovable() const
    {
        bool unmovable = false;
        JValueUtil::getValue(m_appinfo, "unmovable", unmovable);
        return unmovable;
    }

    bool isVisible() const
    {
        bool visible = true;
        JValueUtil::getValue(m_appinfo, "visible", visible);
        return visible;
    }

private:
    static const vector<string> PROPS_PROHIBITED;
    static const vector<string> PROPS_IMAGES;
    static const vector<string> ASSETS_SUPPORTED;
    static const string CLASS_NAME;

    AppDescription& operator=(const AppDescription& appDesc) = delete;
    AppDescription(const AppDescription& appDesc) = delete;

    bool readAppinfo();
    void readAsset();

    // Following values are given by external
    AppLocation m_appLocation;
    string m_folderPath;

    AppType m_appType;
    LifeHandlerType m_handlerType;

    string m_appId;
    string m_version;
    AppIntVersion m_intVersion;

    // from appinfo
    string m_absMain;
    string m_absSplashBackground;

    bool m_isLocked;
    JValue m_appinfo;

};

#endif // BASE_APPDESCRIPTION_H_

