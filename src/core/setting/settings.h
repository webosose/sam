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

#ifndef __Settings_h__
#define __Settings_h__

#include <core/util/singleton.h>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <glib.h>
#include <pbnjson.hpp>

#include "core/package/application_description.h"
#include "core/package/app_scanner.h"

class Settings {
public:
    const std::string& getConfPath()
    {
        return m_confPath;
    }
    void setConfPath(const char* path)
    {
        m_confPath = path;
    }
    bool load(const char* filePath);
    void onRestLoad();

    // lifecycle related
    void setKeepAliveApps(const pbnjson::JValue& apps);
    void set_support_qml_booster(bool value);
    bool IsKeepAliveApp(const std::string&) const;
    bool checkAppAgainstNoJailAppList(const std::string&) const;
    void SetLifeCycleReason(const pbnjson::JValue& data);
    void AddLaunchReason(const std::string& caller_id, const std::string& reason, const std::string& launch_reason);
    std::string GetLaunchReason(const std::string& caller_id, const std::string& reason);
    void AddCloseReason(const std::string& caller_id, const std::string& reason, const std::string& close_reason);
    std::string GetCloseReason(const std::string& caller_id, const std::string& reason);
    void AddCRIUSupportApp(const std::string& app_id);
    bool SupportCRIU(const std::string& app_id) const;
    unsigned long long int GetLaunchExpiredTimeout() const
    {
        return launch_expired_timeout_;
    }
    unsigned long long int GetLoadingExpiredTimeout() const
    {
        return loading_expired_timeout_;
    }
    guint GetLastLoadingAppTimeout() const
    {
        return last_loading_app_timeout_;
    }

    // package related
    void AddBaseAppDirPath(const std::string& path, AppTypeByDir type);
    const BaseScanPaths& GetBaseAppDirs() const
    {
        return base_app_dirs_;
    }
    std::string getAppInstallBase(bool verified = true) const;
    void setAssetFallbackKeys(const pbnjson::JValue& keys);
    void deletedSystemAppsToStringVector(std::vector<std::string>& apps);
    void setSystemAppAsRemoved(const std::string& appId);
    bool isDeletedSystemApp(const std::string& appId) const;
    bool is_in_host_apps_list(const std::string& app_id) const;
    const std::vector<std::string>& GetBootTimeApps() const
    {
        return boot_time_apps_;
    }
    void AddBootTimeApp(const std::string& app_id);
    bool IsBootTimeApp(const std::string& app_id);

    // common settings
    std::string appMgrPreferenceDir;      // /var/preferences/com.webos.applicationManager/
    std::string deletedSystemAppListPath; // /var/preferences/com.webos.applicationManager/deletedSystemAppList.json
    std::string devModePath;              // /var/luna/preferences/devmode_enabled
    std::string localeInfoPath;           // /var/luna/preferences/localeInfo
    std::string schemaPath;               // /etc/palm/schemas/sam/
    std::string respawnedPath;            // /tmp/sam-respawned
    std::string jailModePath;             // /var/luna/preferences/jailer_disabled
    std::string log_path_;
    bool isRespawned;
    bool isDevMode;
    bool isJailMode;
    bool isRestLoaded;

    // lifecycle related
    std::vector<std::string> fullscreen_window_types;
    std::vector<std::string> keepAliveApps;
    std::vector<std::string> noJailApps;
    std::vector<std::string> host_apps_for_alias;
    std::string app_store_id_;
    std::string qmlRunnerPath; // /usr/bin/qml-runner
    std::string appshellRunnerPath; // /usr/bin/app-shell/run_appshell
    std::string jailerPath;    // /usr/bin/jailer
    bool use_qml_booster_;
    std::map<std::string, std::string> launch_event_reason_map_;
    std::map<std::string, std::string> close_event_reason_map_;
    unsigned long long int launch_expired_timeout_;
    unsigned long long int loading_expired_timeout_;
    guint last_loading_app_timeout_;

    // package related
    std::string appInstallBase;       // /media/cryptofs/apps
    std::string appInstallRelative;   // /usr/palm/applications
    std::string devAppsBasePath;      // /media/developer/apps
    std::string tempAliasAppBasePath; // /tmp/alias/apps
    std::string aliasAppBasePath;     // /media/alias/apps
    pbnjson::JValue m_deletedSystemApps;
    std::vector<std::string> m_assetFallbackPrecedence;
    std::string package_asset_variant_;
    bool usePartialKeywordAppSearch;
    std::string lunaCmdHandlerSavedPath;  // TODO: make it deprecated or restructured

private:
    friend class Singleton<Settings> ;

    Settings();
    ~Settings();

    bool LoadStaticConfig(const char* filePath);
    void LoadConfigOnRWFilesystemReady();

    std::string m_confPath;
    BaseScanPaths base_app_dirs_;
    std::vector<std::string> devAppsPaths;
    std::vector<std::string> boot_time_apps_;
    std::vector<std::string> criu_support_apps_;
};

typedef Singleton<Settings> SettingsImpl;

#endif // Settings

