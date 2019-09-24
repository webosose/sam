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
#include <pbnjson.hpp>

#include "base/AppDescription.h"
#include "interface/IClassName.h"
#include "interface/ISingleton.h"
#include "setting/SettingsConf.h"

class Settings : public ISingleton<Settings>,
                 public IClassName {
friend class ISingleton<Settings> ;
public:
    virtual ~Settings();

    void setLifeCycleReason(const pbnjson::JValue& data);
    void addLaunchReason(const std::string& caller_id, const std::string& reason, const std::string& launch_reason);
    std::string getLaunchReason(const std::string& caller_id, const std::string& reason);
    void addCloseReason(const std::string& caller_id, const std::string& reason, const std::string& close_reason);
    std::string getCloseReason(const std::string& caller_id, const std::string& reason);

    // package related
    void addAppDir(std::string path, AppLocation type);
    const map<std::string, AppLocation>& getAppDirs() const
    {
        return m_appDirs;
    }

    string getAppShellRunnerPath()
    {
        string AppShellRunnerPath = "/usr/bin/app-shell/run_appshell";
        JValueUtil::getValue(m_readOnlyDatabase, "AppShellRunnerPath", AppShellRunnerPath);
        return AppShellRunnerPath;
    }

    string getDevModePath()
    {
        string DevModePath = "/var/luna/preferences/devmode_enabled";
        JValueUtil::getValue(m_readOnlyDatabase, "DevModePath", DevModePath);
        return DevModePath;
    }

    string getRespawnedPath()
    {
        string RespawnedPath = "/tmp/sam-respawned";
        JValueUtil::getValue(m_readOnlyDatabase, "RespawnedPath", RespawnedPath);
        return RespawnedPath;
    }

    string getJailModePath()
    {
        string JailModePath = "/var/luna/preferences/jailer_disabled";
        JValueUtil::getValue(m_readOnlyDatabase, "JailModePath", JailModePath);
        return JailModePath;
    }

    string getJailerPath()
    {
        string JailerPath = "/usr/bin/jailer";
        JValueUtil::getValue(m_readOnlyDatabase, "JailerPath", JailerPath);
        return JailerPath;
    }

    string getQmlRunnerPath()
    {
        string QmlRunnerPath = "/usr/bin/qml-runner";
        JValueUtil::getValue(m_readOnlyDatabase, "QmlRunnerPath", QmlRunnerPath);
        return QmlRunnerPath;
    }

    bool isNoJailApp(const std::string& appId) const
    {
        JValue NoJailApps;
        if (!JValueUtil::getValue(m_readOnlyDatabase, "NoJailApps", NoJailApps) || !NoJailApps.isArray()) {
            return false;
        }

        int size = NoJailApps.arraySize();
        for (int i = 0; i < size; ++i) {
            if (NoJailApps[i].asString() == appId) {
                return true;
            }
        }
        return false;
    }

    bool isFullscreenWindowTypes(string type)
    {
        JValue FullscreenWindowType;
        if (!JValueUtil::getValue(m_readOnlyDatabase, "FullscreenWindowType", FullscreenWindowType) || !FullscreenWindowType.isArray()) {
            return false;
        }
        int size = FullscreenWindowType.arraySize();
        for (int i = 0; i < size; ++i) {
            if (FullscreenWindowType[i].asString() == type) {
                return true;
            }
        }
        return false;
    }

    JValue getDBSchema() const
    {
        JValue LaunchPointDBKind = pbnjson::Object();
        JValueUtil::getValue(m_readOnlyDatabase, "LaunchPointDBKind", LaunchPointDBKind);
        return LaunchPointDBKind;
    }

    JValue getDBPermission() const
    {
        JValue LaunchPointDBPermissions = pbnjson::Object();
        JValueUtil::getValue(m_readOnlyDatabase, "LaunchPointDBPermissions", LaunchPointDBPermissions);
        return LaunchPointDBPermissions;
    }

    void setKeepAliveApps(const pbnjson::JValue& array)
    {
        if (!array.isArray())
            return;

        JValue keepAliveApps;
        if (JValueUtil::getValue(m_readWriteDatabase, "keepAliveApps", keepAliveApps) && keepAliveApps == array)
            return;

        m_readWriteDatabase.put("keepAliveApps", array);
        saveReadWriteConf();
    }

    JValue getSysAssetFallbackPrecedence() const
    {
        JValue sysAssetFallbackPrecedence = pbnjson::Array();
        JValueUtil::getValue(m_readWriteDatabase, "sysAssetFallbackPrecedence", sysAssetFallbackPrecedence);
        return sysAssetFallbackPrecedence;
    }

    void setSysAssetFallbackPrecedence(const pbnjson::JValue& array)
    {
        if (!array.isArray())
            return;

        JValue sysAssetFallbackPrecedence;
        if (JValueUtil::getValue(m_readWriteDatabase, "sysAssetFallbackPrecedence", sysAssetFallbackPrecedence) && sysAssetFallbackPrecedence == array)
            return;

        m_readWriteDatabase.put("sysAssetFallbackPrecedence", array);
        saveReadWriteConf();
    }

    bool isKeepAliveApp(const std::string& appId) const
    {
        JValue keepAliveApps;
        if (!JValueUtil::getValue(m_readWriteDatabase, "keepAliveApps", keepAliveApps) || !keepAliveApps.isArray()) {
            return false;
        }

        int size = keepAliveApps.arraySize();
        for (int i = 0; i < size; ++i) {
            if (keepAliveApps[i].asString() == appId) {
                return true;
            }
        }
        return false;
    }

    void appendDeletedSystemApp(const std::string& appId)
    {
        if (isDeletedSystemApp(appId)) {
            return;
        }

        JValue deletedSystemApps;
        if (!JValueUtil::getValue(m_readWriteDatabase, "deletedSystemApps", deletedSystemApps) || !deletedSystemApps.isArray()) {
            m_readWriteDatabase.put("deletedSystemApps", pbnjson::Array());
        }
        m_readWriteDatabase["deletedSystemApps"].append(appId);
        saveReadWriteConf();
    }

    bool isDeletedSystemApp(const std::string& appId) const
    {
        JValue deletedSystemApps;
        if (!JValueUtil::getValue(m_readWriteDatabase, "deletedSystemApps", deletedSystemApps) || !deletedSystemApps.isArray()) {
            return false;
        }

        int size = deletedSystemApps.arraySize();
        for (int i = 0; i < size; ++i) {
            if (deletedSystemApps[i].asString() == appId) {
                return true;
            }
        }
        return false;
    }

    void setSupportQMLBooster(bool value)
    {
        bool isSupportQmlBooster = false;
        if (JValueUtil::getValue(m_readWriteDatabase, "isSupportQmlBooster", isSupportQmlBooster) && isSupportQmlBooster == value)
            return;
        m_readWriteDatabase.put("isSupportQmlBooster", value);
        saveReadWriteConf();
    }

    const std::string getLanguage() const
    {
        string language = "";
        JValueUtil::getValue(m_readWriteDatabase, "language", language);
        return language;
    }

    const std::string getScript() const
    {
        string script = "";
        JValueUtil::getValue(m_readWriteDatabase, "script", script);
        return script;
    }

    const std::string getRegion() const
    {
        string region = "";
        JValueUtil::getValue(m_readWriteDatabase, "region", region);
        return region;
    }

    void setLocale(const string& language, const string& script, const string& region)
    {
        if (language == getLanguage() && script == getScript() && region == getRegion())
            return;
        m_readWriteDatabase.put("language", language);
        m_readWriteDatabase.put("script", script);
        m_readWriteDatabase.put("region", region);
        saveReadWriteConf();
    }

    bool isSupportQmlBooster()
    {
        bool isSupportQmlBooster = false;
        JValueUtil::getValue(m_readWriteDatabase, "isSupportQmlBooster", isSupportQmlBooster);
        return isSupportQmlBooster;
    }

    bool isRespawned()
    {
        return m_isRespawned;
    }

    bool isDevmodeEnabled()
    {
        return m_isDevmodeEnabled;
    }

    bool isJailerDisabled()
    {
        return m_isJailerDisabled;
    }

private:
    Settings();

    bool loadReadOnlyConf();
    void loadReadWriteConf();
    void saveReadWriteConf();

    map<std::string, AppLocation> m_appDirs;
    vector<std::string> m_devAppsPaths;

    JValue m_readOnlyDatabase;
    JValue m_readWriteDatabase;

    bool m_isRespawned;
    bool m_isDevmodeEnabled;
    bool m_isJailerDisabled;
};

typedef ISingleton<Settings> SettingsImpl;

#endif // Settings

