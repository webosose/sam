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

#ifndef __CONF_SAM_FONF_H__
#define __CONF_SAM_FONF_H__

#include <string>
#include <pbnjson.hpp>

#include "Environment.h"
#include "interface/IClassName.h"
#include "interface/ISingleton.h"
#include "util/JValueUtil.h"
#include "util/Logger.h"
#include "util/File.h"

class SAMConf : public ISingleton<SAMConf>,
                public IClassName {
friend class ISingleton<SAMConf> ;
public:
    virtual ~SAMConf();

    void initialize();

    /** READ ONLY CONFIGS **/

    JValue getApplicationPaths()
    {
        JValue ApplicationPaths = pbnjson::Array();
        JValueUtil::getValue(m_readOnlyDatabase, "ApplicationPaths", ApplicationPaths);
        return ApplicationPaths;
    }

    const string& getAppShellRunnerPath()
    {
        static string AppShellRunnerPath = "/usr/bin/app-shell/run_app_shell";
        JValueUtil::getValue(m_readOnlyDatabase, "AppShellRunnerPath", AppShellRunnerPath);
        return AppShellRunnerPath;
    }

    JValue getDBPermission() const
    {
        JValue LaunchPointDBPermissions = pbnjson::Object();
        JValueUtil::getValue(m_readOnlyDatabase, "LaunchPointDBPermissions", LaunchPointDBPermissions);
        return LaunchPointDBPermissions;
    }

    const JValue& getDBSchema() const
    {
        static JValue LaunchPointDBKind = pbnjson::Object();
        JValueUtil::getValue(m_readOnlyDatabase, "LaunchPointDBKind", LaunchPointDBKind);
        return LaunchPointDBKind;
    }

    const string& getDevModePath()
    {
        static string DevModePath = "/var/luna/preferences/devmode_enabled";
        JValueUtil::getValue(m_readOnlyDatabase, "DevModePath", DevModePath);
        return DevModePath;
    }

    const string& getJailerPath()
    {
        static string JailerPath = "/usr/bin/jailer";
        JValueUtil::getValue(m_readOnlyDatabase, "JailerPath", JailerPath);
        return JailerPath;
    }

    const string& getJailModePath()
    {
        static string JailModePath = "/var/luna/preferences/jailer_disabled";
        JValueUtil::getValue(m_readOnlyDatabase, "JailModePath", JailModePath);
        return JailModePath;
    }

    const string& getQmlRunnerPath()
    {
        static string QmlRunnerPath = "/usr/bin/qml-runner";
        JValueUtil::getValue(m_readOnlyDatabase, "QmlRunnerPath", QmlRunnerPath);
        return QmlRunnerPath;
    }

    const string& getRespawnedPath()
    {
        static string RespawnedPath = "/tmp/sam-respawned";
        JValueUtil::getValue(m_readOnlyDatabase, "RespawnedPath", RespawnedPath);
        return RespawnedPath;
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

    bool isNoJailApp(const string& appId) const
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

    /** READ WRIETE CONFIGS **/

    bool isKeepAliveApp(const string& appId) const
    {
        JValue keepAliveApps;

        if (JValueUtil::getValue(m_readOnlyDatabase, "keepAliveApps", keepAliveApps) && keepAliveApps.isArray()) {
            int size = keepAliveApps.arraySize();
            for (int i = 0; i < size; ++i) {
                if (keepAliveApps[i].asString() == appId) {
                    return true;
                }
            }
        }

        if (!JValueUtil::getValue(m_readWriteDatabase, "keepAliveApps", keepAliveApps) && keepAliveApps.isArray()) {
            int size = keepAliveApps.arraySize();
            for (int i = 0; i < size; ++i) {
                if (keepAliveApps[i].asString() == appId) {
                    return true;
                }
            }
        }

        return false;
    }

    void setKeepAliveApps(const JValue& array)
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

    void setSysAssetFallbackPrecedence(const JValue& array)
    {
        if (!array.isArray())
            return;

        JValue sysAssetFallbackPrecedence;
        if (JValueUtil::getValue(m_readWriteDatabase, "sysAssetFallbackPrecedence", sysAssetFallbackPrecedence) && sysAssetFallbackPrecedence == array)
            return;

        m_readWriteDatabase.put("sysAssetFallbackPrecedence", array);
        saveReadWriteConf();
    }

    bool isDeletedSystemApp(const string& appId) const
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

    void appendDeletedSystemApp(const string& appId)
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

    const string& getLanguage() const
    {
        static string language = "";
        JValueUtil::getValue(m_readWriteDatabase, "language", language);
        return language;
    }

    const string& getScript() const
    {
        static string script = "";
        JValueUtil::getValue(m_readWriteDatabase, "script", script);
        return script;
    }

    const string& getRegion() const
    {
        static string region = "";
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

    bool isAppRelaunchSupported()
    {
        // TODO This should be moved in configuration file in the future.
        return false;
    }

private:
    SAMConf();

    void loadReadOnlyConf();
    void loadReadWriteConf();
    void saveReadWriteConf();

    JValue m_readOnlyDatabase;
    JValue m_readWriteDatabase;

    bool m_isRespawned;
    bool m_isDevmodeEnabled;
    bool m_isJailerDisabled;
};

#endif // __CONF_SAM_FONF_H__

