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

#include <errno.h>
#include <lifecycle/AppInfoManager.h>
#include <setting/SettingsConf.h>
#include <setting/Settings.h>
#include <unistd.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/Utils.h>
#include <string>

const std::string DELETED_LIST_KEY = "deletedList";

Settings::Settings() :
        m_appMgrPreferenceDir(kAppMgrPreferenceDir), m_deletedSystemAppListPath(kDeletedSystemAppListPath), m_devModePath("/var/luna/preferences/devmode_enabled"), m_localeInfoPath(kLocaleInfoFile), m_schemaPath(
                kSchemaPath), m_respawnedPath("/tmp/sam-respawned"), m_jailModePath("/var/luna/preferences/jailer_disabled"), m_logPath(kLogBasePath), m_isRespawned(false), m_isDevMode(false), m_isJailMode(
                false), m_isRestLoaded(false), m_qmlRunnerPath("/usr/bin/qml-runner"), m_appshellRunnerPath("/usr/bin/app-shell/run_appshell"), m_jailerPath("/usr/bin/jailer"), m_useQmlBooster(false), m_launchExpiredTimeout(
                120000000000ULL), // 120sec
        m_loadingExpiredTimeout(30000000000ULL), // 30sec
        m_lastLoadingAppTimeout(30000), // 30sec
        m_appInstallBase(kAppInstallBase), m_appInstallRelative("usr/palm/applications"), m_devAppsBasePath("/media/developer/apps"), m_tempAliasAppBasePath("/tmp/alias/apps"), m_aliasAppBasePath(
                "/media/alias/apps"), m_deletedSystemApps(pbnjson::Object()), m_usePartialKeywordAppSearch(true)
{
    m_launchPointDbkind = pbnjson::Object();
    m_launchPointPermissions = pbnjson::Object();
}

Settings::~Settings()
{
    m_baseAppDirs.clear();
}

bool Settings::load(const char* filePath)
{
    // TODO: move this to configd
    // launch reason
    addLaunchReason("com.webos.surfacemanager", "launcher", "launcher");
    addLaunchReason("com.webos.surfacemanager", "recent", "recent");
    addLaunchReason("com.webos.surfacemanager", "autoReload", "autoReload");
    addLaunchReason("com.webos.surfacemanager", "manualLastApp", "manualLastApp");
    addLaunchReason("com.webos.surfacemanager", "autoLastApp", "autoLastApp");
    addLaunchReason("com.webos.surfacemanager", "hotKey", "hotKey");
    addLaunchReason("com.webos.surfacemanager", "wedge", "wedge");

    addLaunchReason("com.webos.surfacemanager.inputpicker", "", "inputPicker");
    addLaunchReason("com.webos.surfacemanager.quicksettings", "", "quickSetting");
    addLaunchReason("com.webos.service.preloadmanager", "", "preload");
    addLaunchReason("com.webos.keyfilter.magicnumber", "", "magicNumber");
    addLaunchReason("com.webos.app.voiceagent", "", "voiceCommand");
    addLaunchReason("com.webos.app.voiceview", "", "searchResult");
    addLaunchReason("com.webos.app.voice", "", "searchResult");
    addLaunchReason("com.webos.app.favshows", "", "myContents");
    addLaunchReason("com.webos.app.livemenu", "", "directAccess");
    addLaunchReason("com.webos.service.dial", "", "dial");
    addLaunchReason("com.webos.bootManager", "", "boot");
    addLaunchReason("com.webos.app.cpcover", "", "boot");

    // close reason
    addCloseReason("com.webos.surfacemanager.windowext", "windowPolicy", "windowPolicy");
    addCloseReason("com.webos.surfacemanager.windowext", "recent", "recent");
    addCloseReason("com.webos.surfacemanager.windowext", "systemUI", "systemUI");

    addCloseReason("com.webos.memorymanager", "", "memoryReclaim");
    addCloseReason("com.webos.service.tvpower", "", "powerSuspend");
    addCloseReason("com.webos.service.dial", "", "dial");

    return loadStaticConfig(filePath);
}

void Settings::onRestLoad()
{
    m_isRestLoaded = true;
    loadConfigOnRWFilesystemReady();
}

void Settings::loadConfigOnRWFilesystemReady()
{
    // Add base app dir path for dev apps
    if (0 == access(m_devModePath.c_str(), F_OK)) {
        m_isDevMode = true;
        for (auto it : m_devAppsPaths) {
            addBaseAppDirPath(it, AppTypeByDir::AppTypeByDir_Dev);
        }
    }

    LOG_INFO(MSGID_SETTING_INFO, 1, PMLOGKS("key", "devmode"), "val: %s", (m_isDevMode ? "on" : "off"));

    if (0 == access(m_respawnedPath.c_str(), F_OK)) {
        m_isRespawned = true;
    } else {
        //create respawnded file
        if (!writeFile(m_respawnedPath.c_str(), "")) {
            LOG_WARNING(MSGID_SETTINGS_ERR, 1, PMLOGKS("respawnedPath", m_respawnedPath.c_str()), "cannot create respawned file");
        }
    }

    LOG_INFO(MSGID_SAM_LOADING_SEQ, 2, PMLOGKS("status", "analyze_loading_reason"), PMLOGKS("loading_status", (m_isRespawned ? "respawned" : "fresh_start")), "");

    if (0 != g_mkdir_with_parents(m_appMgrPreferenceDir.c_str(), 0700)) {
        LOG_WARNING(MSGID_SETTINGS_ERR, 1, PMLOGKS("PreferencesDir", m_appMgrPreferenceDir.c_str()), "cannot create PreferenceDir");
    }

    // Load Deleted SystemApp list
    pbnjson::JValue json = JUtil::parseFile(m_deletedSystemAppListPath, "deletedSystemAppList", NULL);
    if (json.isNull()) {
        LOG_DEBUG("[DELETED SYSTEM APP LIST] fail to launch list");
        m_deletedSystemApps.put(DELETED_LIST_KEY, pbnjson::Array());
    } else {
        m_deletedSystemApps = json;
    }

    LOG_DEBUG("[Load deletedList] %s", m_deletedSystemApps.stringify().c_str());
}

bool Settings::loadStaticConfig(const char* filePath)
{
    std::string conf_path = "";
    if (NULL == filePath)
        conf_path = m_confPath;
    if (conf_path.empty())
        conf_path = kSettingsFile;

    LOG_DEBUG("LoadConf : %s", conf_path.c_str());

    JUtil::Error error;
    pbnjson::JValue root = JUtil::parseFile(conf_path, "sam-conf", &error);
    if (root.isNull()) {
        LOG_WARNING(MSGID_SETTINGS_ERR, 1, PMLOGKS("FILE", conf_path.c_str()), "");
        return false;
    }

    if (root["DevModePath"].isString())
        m_devModePath = root["DevModePath"].asString();

    if (root["JailModePath"].isString())
        m_jailModePath = root["JailModePath"].asString();

    if (root["JailerPath"].isString())
        m_jailerPath = root["JailerPath"].asString();

    if (root["QmlRunnerPath"].isString())
        m_qmlRunnerPath = root["QmlRunnerPath"].asString();

    if (root["AppShellRunnerPath"].isString())
        m_appshellRunnerPath = root["AppShellRunnerPath"].asString();

    if (root["RespawnedPath"].isString())
        m_respawnedPath = root["RespawnedPath"].asBool();

    if (root["ApplicationPaths"].isArray()) {
        pbnjson::JValue applicationPaths = root["ApplicationPaths"];
        int arraySize = applicationPaths.arraySize();

        for (int i = 0; i < arraySize; i++) {
            std::string path, type;
            AppTypeByDir typeByDir;
            if (applicationPaths[i]["typeByDir"].asString(type) != CONV_OK)
                continue;
            if (applicationPaths[i]["path"].asString(path) != CONV_OK)
                continue;

            if ("system_builtin" == type)
                typeByDir = AppTypeByDir::AppTypeByDir_System_BuiltIn;
            else if ("system_updatable" == type)
                typeByDir = AppTypeByDir::AppTypeByDir_System_Updatable;
            else if ("store" == type)
                typeByDir = AppTypeByDir::AppTypeByDir_Store;
            else if ("dev" == type) {
                m_devAppsPaths.push_back(path);
                continue;
            }
            else
                continue;

            LOG_DEBUG("PathType : %s, Path : %s", type.c_str(), path.c_str());

            addBaseAppDirPath(path, typeByDir);
        }

        if (m_baseAppDirs.size() < 1) {
            LOG_WARNING(MSGID_NO_APP_PATHS, 1, PMLOGKS("FILE", conf_path.c_str()), "");
            return false;
        }
    }

    if (root["UsePartialKeywordMatchForAppSearch"].isBoolean()) {
        m_usePartialKeywordAppSearch = root["UsePartialKeywordMatchForAppSearch"].asBool();
    }

    if (root.hasKey("FullscreenWindowType") && root["FullscreenWindowType"].isArray()) {
        int array_size = root["FullscreenWindowType"].arraySize();
        for (int i = 0; i < array_size; ++i) {
            std::string value;
            if (root["FullscreenWindowType"][i].asString(value) != CONV_OK)
                continue;
            m_fullscreenWindowTypes.push_back(value);
        }
    }

    if (root.hasKey("HostAppsForAlias") && root["HostAppsForAlias"].isArray()) {
        for (int i = 0; i < root["HostAppsForAlias"].arraySize(); ++i) {
            std::string app_id = "";
            if (!root["HostAppsForAlias"][i].isString() || root["HostAppsForAlias"][i].asString(app_id) != CONV_OK)
                continue;

            LOG_INFO("HOSTAPPS", 1, PMLOGKS("app_id", app_id.c_str()), "");
            m_hostAppsForAlias.push_back(app_id);
        }
    }

    if (root["NoJailApps"].isArray()) {
        pbnjson::JValue appList = root["NoJailApps"];
        int arraySize = appList.arraySize();
        for (int i = 0; i < arraySize; ++i) {
            std::string str;
            if (appList[i].asString(str) != CONV_OK)
                continue;
            m_noJailApps.push_back(str);
            LOG_DEBUG("NoJailApp: %s", str.c_str());
        }
    }

    if (root.hasKey("BootTimeApps") && root["BootTimeApps"].isArray()) {
        for (auto it : root["BootTimeApps"].items()) {
            if (!it.isString())
                continue;
            addBootTimeApp(it.asString());
        }
    }

    if (root["StoreApp"].isString()) {
        m_appStoreId = root["StoreApp"].asString();
    }
    return true;
}

bool Settings::loadSAMConf()
{
    std::string conf_path = kBasicSettingsFile;

    LOG_DEBUG("Base LoadConf : %s", conf_path.c_str());

    JUtil::Error error;
    pbnjson::JValue root = JUtil::parseFile(conf_path, "", &error);
    if (root.isNull()) {
        LOG_WARNING(MSGID_SETTINGS_ERR, 1, PMLOGKS("FILE", conf_path.c_str()), "");
        return false;
    }

    if (root.hasKey("BootTimeApps") && root["BootTimeApps"].isArray()) {
        for (auto it : root["BootTimeApps"].items()) {
            if (!it.isString())
                continue;
            SettingsImpl::instance().addBootTimeApp(it.asString());
        }
    }

    if (root.hasKey("CRIUSupportApps") && root["CRIUSupportApps"].isArray()) {
        for (auto it : root["CRIUSupportApps"].items()) {
            if (!it.isString())
                continue;
            SettingsImpl::instance().addCRIUSupportApp(it.asString());
        }
    }

    if (root.hasKey("LaunchPointDBKind"))
        m_launchPointDbkind = root["LaunchPointDBKind"];

    if (root.hasKey("LaunchPointDBPermissions"))
        m_launchPointPermissions = root["LaunchPointDBPermissions"];

    return true;
}

void Settings::addBaseAppDirPath(const std::string& path, AppTypeByDir type)
{
    m_baseAppDirs.push_back( { path, type });
}

void Settings::setKeepAliveApps(const pbnjson::JValue& apps)
{
    if (!apps.isArray())
        return;
    m_keepAliveApps.clear();

    LOG_INFO(MSGID_CONFIGD_INIT, 1,
             PMLOGKS("CONFIG_TYPE", "keep_alive_apps"),
             "%s", apps.duplicate().stringify().c_str());

    int apps_num = apps.arraySize();
    for (int i = 0; i < apps_num; ++i) {
        std::string app_id = "";
        if (!apps[i].isString() || apps[i].asString(app_id) != CONV_OK)
            continue;

        LOG_INFO(MSGID_CONFIGD_INIT, 2, PMLOGKS("CONFIG_TYPE", "keep_alive_apps"), PMLOGKS("app_id", app_id.c_str()), "");
        m_keepAliveApps.push_back(app_id);
    }
}

void Settings::setSupportQMLBooster(bool value)
{
    m_useQmlBooster = value;
    LOG_INFO(MSGID_CONFIGD_INIT, 2, PMLOGKS("CONFIG_TYPE", "support_qml_booster"), PMLOGKS("value", (value?"true":"false")), "");
}

void Settings::setAssetFallbackKeys(const pbnjson::JValue& keys_arr)
{

    m_assetFallbackPrecedence.clear();

    LOG_INFO(MSGID_CONFIGD_INIT, 1,
             PMLOGKS("CONFIG_TYPE", "asset_fallback_precedence"),
             "%s", keys_arr.duplicate().stringify().c_str());

    if (!keys_arr.isArray())
        return;

    int keys_num = keys_arr.arraySize();

    for (int i = 0; i < keys_num; ++i) {
        std::string key = "";
        if (!keys_arr[i].isString() || keys_arr[i].asString(key) != CONV_OK)
            continue;

        LOG_INFO(MSGID_CONFIGD_INIT, 2, PMLOGKS("CONFIG_TYPE", "asset_fallback_precedence"), PMLOGKS("key", key.c_str()), "");
        m_assetFallbackPrecedence.push_back(key);
    }
}

std::string Settings::getAppInstallBase(bool verified) const
{
    return (verified) ? m_appInstallBase : m_devAppsBasePath;
}

bool Settings::isInHostAppsList(const std::string& app_id) const
{
    auto it = std::find(m_hostAppsForAlias.begin(), m_hostAppsForAlias.end(), app_id);
    if (it != m_hostAppsForAlias.end())
        return true;
    return false;
}

bool Settings::isKeepAliveApp(const std::string& appId) const
{
    auto it = std::find(m_keepAliveApps.begin(), m_keepAliveApps.end(), appId);
    if (it != m_keepAliveApps.end())
        return true;
    return false;
}

bool Settings::isInNoJailApps(const std::string& appId) const
{
    auto it = std::find(m_noJailApps.begin(), m_noJailApps.end(), appId);
    if (it != m_noJailApps.end())
        return true;
    return false;
}

void Settings::addBootTimeApp(const std::string& app_id)
{
    auto it = std::find(m_bootTimeApps.begin(), m_bootTimeApps.end(), app_id);
    if (it != m_bootTimeApps.end())
        return;
    m_bootTimeApps.push_back(app_id);
}

bool Settings::isBootTimeApp(const std::string& app_id)
{
    for (auto& id : m_bootTimeApps)
        if (app_id == id)
            return true;
    return false;
}

void Settings::deletedSystemAppsToStringVector(std::vector<std::string>& apps)
{
    int arraySize = m_deletedSystemApps[DELETED_LIST_KEY].arraySize();
    if (arraySize < 1)
        return;

    for (int i = 0; i < arraySize; ++i) {
        apps.push_back(m_deletedSystemApps[DELETED_LIST_KEY][i].asString());
    }
}

void Settings::setSystemAppAsRemoved(const std::string& appId)
{
    if (isDeletedSystemApp(appId)) {
        LOG_DEBUG("Already in deleted list");
        return;
    }

    m_deletedSystemApps[DELETED_LIST_KEY].append(appId);

    std::string deletedList = m_deletedSystemApps.stringify();

    LOG_DEBUG("[REMOVE SYSTEM APP] deletedList: %s", deletedList.c_str());

    if (!g_file_set_contents(m_deletedSystemAppListPath.c_str(), deletedList.c_str(), deletedList.length(), NULL)) {
        LOG_WARNING(MSGID_FAIL_WRITING_DELETEDLIST, 2, PMLOGKS("CONTENT", deletedList.c_str()), PMLOGKS("PATH", m_deletedSystemAppListPath.c_str()), "");
    }
}

bool Settings::isDeletedSystemApp(const std::string& appId) const
{
    if (m_deletedSystemApps.isNull() || !m_deletedSystemApps.isObject() || !m_deletedSystemApps.hasKey(DELETED_LIST_KEY) || !m_deletedSystemApps[DELETED_LIST_KEY].isArray())
        return false;
    int arraySize = m_deletedSystemApps[DELETED_LIST_KEY].arraySize();
    if (arraySize < 1)
        return false;

    for (int i = 0; i < arraySize; ++i) {
        if (appId == m_deletedSystemApps[DELETED_LIST_KEY][i].asString())
            return true;
    }

    return false;
}

void Settings::setLifeCycleReason(const pbnjson::JValue& data)
{
    if (data.hasKey("launchReason") && data["launchReason"].isObject() && data["launchReason"].objectSize() > 0) {

        for (auto it : data["launchReason"].children()) {
            if (!it.second.isArray() || it.second.arraySize() < 1)
                continue;

            std::string launch_reason = it.first.asString();
            for (auto detail : it.second.items()) {
                std::string caller_id;
                std::string reason;

                if (detail.hasKey("callerId") && detail["callerId"].isString())
                    caller_id = detail["callerId"].asString();
                if (detail.hasKey("reason") && detail["reason"].isString())
                    reason = detail["reason"].asString();

                addLaunchReason(caller_id, reason, launch_reason);
            }
        }
    }

    if (data.hasKey("closeReason") && data["closeReason"].isObject() && data["closeReason"].objectSize() > 0) {

        for (auto it : data["closeReason"].children()) {
            if (!it.second.isArray() || it.second.arraySize() < 1)
                continue;

            std::string close_reason = it.first.asString();
            for (auto detail : it.second.items()) {
                std::string caller_id;
                std::string reason;

                if (detail.hasKey("callerId") && detail["callerId"].isString())
                    caller_id = detail["callerId"].asString();
                if (detail.hasKey("reason") && detail["reason"].isString())
                    reason = detail["reason"].asString();

                addCloseReason(caller_id, reason, close_reason);
            }
        }
    }
}

void Settings::addLaunchReason(const std::string& caller_id, const std::string& reason, const std::string& launch_reason)
{
    m_launchEventReasonMap[caller_id + ":" + reason] = launch_reason;
}

std::string Settings::getLaunchReason(const std::string& caller_id, const std::string& reason)
{
    std::string key = caller_id + ":" + reason;
    if (!m_launchEventReasonMap.count(key)) {
        return "undefined";
    } else {
        return m_launchEventReasonMap[key];
    }
}

void Settings::addCloseReason(const std::string& caller_id, const std::string& reason, const std::string& close_reason)
{
    m_closeEventReasonMap[caller_id + ":" + reason] = close_reason;
}

std::string Settings::getCloseReason(const std::string& caller_id, const std::string& reason)
{
    std::string key = caller_id + ":" + reason;
    if (!m_closeEventReasonMap.count(key)) {
        return "undefined";
    } else {
        return m_closeEventReasonMap[key];
    }
}

void Settings::addCRIUSupportApp(const std::string& app_id)
{
    if (supportCRIU(app_id))
        return;

    LOG_INFO(MSGID_SETTING_INFO, 2, PMLOGKS("set_key", "criu_app"), PMLOGKS("app_id", app_id.c_str()), "");
    m_criuSupportApps.push_back(app_id);
}

bool Settings::supportCRIU(const std::string& app_id) const
{
    for (auto id : m_criuSupportApps) {
        if (id == app_id)
            return true;
    }
    return false;
}
