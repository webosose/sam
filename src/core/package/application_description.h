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

#include <stdint.h>

#include <string>
#include <list>
#include <memory>
#include <tuple>

#include "core/base/utils.h"
#include "core/package/cmd_resource_handlers.h"
#include "core/package/keyword_map.h"

const unsigned int APP_VERSION_DIGIT = 3;

enum class AppType
    : int8_t {
        None = 0, Web,             // Web app
    Stub,            // StubApp (this is dummy type of apps)
    Native,          // Native app (general native apps)
    Native_Builtin,  // Native app (more special than general native app: e.g. Isis2, Chrome,...)
    Native_Mvpd,     // Native app (more special than general native app: e.g. Netflix, vudu,...)
    Native_Qml,      // Native app (stand alone qml apps, forked with qml runner)
    Native_AppShell, // for Native app (stand alone appshell apps, forked with appshell runner)
    Qml,             // Qml app (launch via booster)
};

enum class LifeHandlerType
    : int8_t {
        None = 0, Native, Web, Qml,
};

enum class AppTypeByDir
    : int8_t {
        None = 0, External_Shared,  // {usb}/sharedcryptofs/apps/usr/palm/applications
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
    ~ApplicationDescription();

    // getter
    AppType type() const
    {
        return app_type_;
    }
    AppTypeByDir getTypeByDir() const
    {
        return type_by_dir_;
    }
    LifeHandlerType handler_type() const
    {
        return handler_type_;
    }
    const std::string& folderPath() const
    {
        return folder_path_;
    }
    const std::string& id() const
    {
        return app_id_;
    }
    const std::string& title() const
    {
        return title_;
    }
    const std::string& entryPoint() const
    {
        return entry_point_;
    }
    const std::string& version() const
    {
        return version_;
    }
    const AppIntVersion& IntVersion() const
    {
        return int_version_;
    }
    const std::string& trustLevel() const
    {
        return trust_level_;
    }
    const std::string& splashBackground() const
    {
        return splash_background_;
    }
    int NativeInterfaceVersion() const
    {
        return native_interface_version_;
    }
    bool IsBuiltinBasedApp() const
    {
        return is_builtin_based_app_;
    }
    bool isRemoveFlagged() const
    {
        return flagged_for_removal_;
    }
    bool isRemovable() const
    {
        return removable_;
    }
    bool isVisible() const
    {
        return visible_;
    }
    int requiredMemory() const
    {
        return required_memory_;
    }
    const std::string& defaultWindowType() const
    {
        return default_window_type_;
    }
    bool window_group() const
    {
        return window_group_;
    }
    bool window_group_owner() const
    {
        return window_group_owner_;
    }
    bool is_child_window() const
    {
        return (window_group_ && !window_group_owner_);
    }
    const std::string& window_group_owner_id() const
    {
        return window_group_owner_id_;
    }
    bool canExecute() const
    {
        return !is_locked_for_excution_;
    }
    const pbnjson::JValue& redirection()
    {
        return redirection_;
    }
    bool isLockable() const
    {
        return lockable_;
    }
    bool splashOnLaunch() const
    {
        return splash_on_launch_;
    }
    bool spinnerOnLaunch() const
    {
        return spinner_on_launch_;
    }
    const pbnjson::JValue& getBootParams()
    {
        return boot_params_;
    }
    const pbnjson::JValue& launchParams() const
    {
        return launch_params_;
    }
    const std::string& containerJS() const
    {
        return container_js_;
    }
    const std::string& enyoVersion() const
    {
        return enyo_version_;
    }
    const pbnjson::JValue& getDeviceId() const
    {
        return m_deviceId;
    }
    const std::list<gchar*>& keywords() const
    {
        return keywords_.allKeywords();
    }
    const std::list<ResourceHandler>& mimeTypes() const;
    const std::list<RedirectHandler>& redirectTypes() const;
    pbnjson::JValue toJValue() const
    {
        return appinfo_json_;
    }
    std::string toString() const;

    // setter
    void SetIntVersion(uint16_t major, uint16_t minor, uint16_t micro)
    {
        int_version_ = {major, minor, micro};}
    void flagForRemoval(bool rf=true) {flagged_for_removal_ = rf;}
    void executionLock(bool xp=true) {is_locked_for_excution_ = xp;}
    void setLaunchParams(const pbnjson::JValue& launchParams) {
        launch_params_ = launchParams.duplicate(); appinfo_json_.put("launchParams", launch_params_);}
    void setDeviceId (const pbnjson::JValue& deviceId) {m_deviceId = deviceId;}

    // feature functions
    bool isPrivileged() const;
    bool securityChecksVerified();
    bool doesMatchKeywordExact(const gchar* keyword) const;
    bool doesMatchKeywordPartial(const gchar* keyword) const;

    void setMimeData();
    void clearMimeData();

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

    class MimeRegInfo {
    public:
        MimeRegInfo() : stream(false) {}
        MimeRegInfo& operator=(const MimeRegInfo& c) {
            if (this == &c)
            return *this;
            mimeType = c.mimeType;
            extension = c.extension;
            urlPattern = c.urlPattern;
            scheme = c.scheme;
            stream = c.stream;
            return *this;
        }
        std::string mimeType;
        std::string extension;
        std::string urlPattern;
        std::string scheme;
        bool stream;
    };

    virtual bool LoadJson(pbnjson::JValue& jdesc, const AppTypeByDir& type_by_dir);
    void LoadAsset(pbnjson::JValue& jdesc);
    static int ExtractMimeTypes(const pbnjson::JValue& jsonMimeTypeArray,
            std::vector<MimeRegInfo>& extractedMimeTypes);
    static std::string CreateRegexpFromSchema(const std::string& schema);

    AppType app_type_;
    AppTypeByDir type_by_dir_;
    LifeHandlerType handler_type_;
    std::string folder_path_;
    std::string app_id_;
    std::string title_;    //copy of default launch point's title
    std::string entry_point_;
    std::string version_;
    AppIntVersion int_version_;
    std::string trust_level_;
    std::string splash_background_;
    int native_interface_version_;
    bool is_builtin_based_app_;
    bool flagged_for_removal_;
    bool removable_;
    bool visible_;
    int required_memory_;
    std::string default_window_type_;
    bool window_group_;
    bool window_group_owner_;
    std::string window_group_owner_id_;
    bool is_locked_for_excution_;
    pbnjson::JValue redirection_;
    bool lockable_;
    bool splash_on_launch_;
    bool spinner_on_launch_;
    pbnjson::JValue boot_params_;
    pbnjson::JValue launch_params_;
    std::string container_js_;
    std::string enyo_version_;
    pbnjson::JValue m_deviceId;
    pbnjson::JValue appinfo_json_;
    KeywordMap keywords_;
    std::list<ResourceHandler> mime_types_;
    std::list<RedirectHandler> redirect_types_;
};

#endif // CORE_PACKAGE_APPLICATION_DESCRIPTION_H_

