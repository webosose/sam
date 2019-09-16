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

#ifndef __Settings_h__
#define __Settings_h__

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <glib.h>
#include <package/AppPackage.h>
#include <package/AppPackageScanner.h>
#include "interface/IClassName.h"
#include "interface/ISingleton.h"
#include <pbnjson.hpp>

class Settings : public ISingleton<Settings>,
                 public IClassName {
friend class ISingleton<Settings> ;
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
    bool loadSAMConf();
    void onRestLoad();

    // lifecycle related
    void setKeepAliveApps(const pbnjson::JValue& apps);
    void setSupportQMLBooster(bool value);
    bool isKeepAliveApp(const std::string&) const;
    bool isInNoJailApps(const std::string&) const;
    void setLifeCycleReason(const pbnjson::JValue& data);
    void addLaunchReason(const std::string& caller_id, const std::string& reason, const std::string& launch_reason);
    std::string getLaunchReason(const std::string& caller_id, const std::string& reason);
    void addCloseReason(const std::string& caller_id, const std::string& reason, const std::string& close_reason);
    std::string getCloseReason(const std::string& caller_id, const std::string& reason);
    void addCRIUSupportApp(const std::string& appId);
    bool supportCRIU(const std::string& appId) const;
    unsigned long long int getLaunchExpiredTimeout() const
    {
        return m_launchExpiredTimeout;
    }
    unsigned long long int getLoadingExpiredTimeout() const
    {
        return m_loadingExpiredTimeout;
    }
    guint getLastLoadingAppTimeout() const
    {
        return m_lastLoadingAppTimeout;
    }

    // package related
    void addBaseAppDirPath(const std::string& path, AppTypeByDir type);
    const BaseScanPaths& getBaseAppDirs() const
    {
        return m_baseAppDirs;
    }
    std::string getAppInstallBase(bool verified = true) const;
    void setAssetFallbackKeys(const pbnjson::JValue& keys);
    void deletedSystemAppsToStringVector(std::vector<std::string>& apps);
    void setSystemAppAsRemoved(const std::string& appId);
    bool isDeletedSystemApp(const std::string& appId) const;
    bool isInHostAppsList(const std::string& appId) const;
    const std::vector<std::string>& getBootTimeApps() const
    {
        return m_bootTimeApps;
    }
    void addBootTimeApp(const std::string& appId);
    bool isBootTimeApp(const std::string& appId);

    // common settings
    std::string m_appMgrPreferenceDir;      // /var/preferences/com.webos.applicationManager/
    std::string m_deletedSystemAppListPath; // /var/preferences/com.webos.applicationManager/deletedSystemAppList.json
    std::string m_devModePath;              // /var/luna/preferences/devmode_enabled
    std::string m_localeInfoPath;           // /var/luna/preferences/localeInfo
    std::string m_schemaPath;               // /etc/palm/schemas/sam/
    std::string m_respawnedPath;            // /tmp/sam-respawned
    std::string m_jailModePath;             // /var/luna/preferences/jailer_disabled
    std::string m_logPath;
    bool m_isRespawned;
    bool m_isDevMode;
    bool m_isJailMode;
    bool m_isRestLoaded;

    // lifecycle related
    std::vector<std::string> m_fullscreenWindowTypes;
    std::vector<std::string> m_keepAliveApps;
    std::vector<std::string> m_noJailApps;
    std::vector<std::string> m_hostAppsForAlias;
    std::string m_appStoreId;
    std::string m_qmlRunnerPath; // /usr/bin/qml-runner
    std::string m_appshellRunnerPath; // /usr/bin/app-shell/run_appshell
    std::string m_jailerPath;    // /usr/bin/jailer
    bool m_useQmlBooster;
    std::map<std::string, std::string> m_launchEventReasonMap;
    std::map<std::string, std::string> m_closeEventReasonMap;
    unsigned long long int m_launchExpiredTimeout;
    unsigned long long int m_loadingExpiredTimeout;
    guint m_lastLoadingAppTimeout;

    // package related
    std::string m_appInstallBase;       // /media/cryptofs/apps
    std::string m_appInstallRelative;   // /usr/palm/applications
    std::string m_devAppsBasePath;      // /media/developer/apps
    std::string m_tempAliasAppBasePath; // /tmp/alias/apps
    std::string m_aliasAppBasePath;     // /media/alias/apps
    pbnjson::JValue m_deletedSystemApps;
    std::vector<std::string> m_assetFallbackPrecedence;
    std::string m_packageAssetVariant;
    bool m_usePartialKeywordAppSearch;
    std::string m_lunaCmdHandlerSavedPath;  // TODO: make it deprecated or restructured

    pbnjson::JValue m_launchPointDbkind;
    pbnjson::JValue m_launchPointPermissions;

private:
    Settings();
    ~Settings();

    bool loadStaticConfig(const char* filePath);
    void loadConfigOnRWFilesystemReady();

    std::string m_confPath;
    BaseScanPaths m_baseAppDirs;
    std::vector<std::string> m_devAppsPaths;
    std::vector<std::string> m_bootTimeApps;
    std::vector<std::string> m_criuSupportApps;

};

typedef ISingleton<Settings> SettingsImpl;

#endif // Settings

