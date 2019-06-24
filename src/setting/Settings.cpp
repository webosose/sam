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
        appMgrPreferenceDir(kAppMgrPreferenceDir), deletedSystemAppListPath(kDeletedSystemAppListPath), devModePath("/var/luna/preferences/devmode_enabled"), localeInfoPath(kLocaleInfoFile), schemaPath(
                kSchemaPath), respawnedPath("/tmp/sam-respawned"), jailModePath("/var/luna/preferences/jailer_disabled"), log_path_(kLogBasePath), isRespawned(false), isDevMode(false), isJailMode(
                false), isRestLoaded(false), qmlRunnerPath("/usr/bin/qml-runner"), appshellRunnerPath("/usr/bin/app-shell/run_appshell"), jailerPath("/usr/bin/jailer"), use_qml_booster_(false), launch_expired_timeout_(
                120000000000ULL), // 120sec
        loading_expired_timeout_(30000000000ULL), // 30sec
        last_loading_app_timeout_(30000), // 30sec
        appInstallBase(kAppInstallBase), appInstallRelative("usr/palm/applications"), devAppsBasePath("/media/developer/apps"), tempAliasAppBasePath("/tmp/alias/apps"), aliasAppBasePath(
                "/media/alias/apps"), m_deletedSystemApps(pbnjson::Object()), usePartialKeywordAppSearch(true)
{
    launch_point_dbkind_ = pbnjson::Object();
    launch_point_permissions_ = pbnjson::Object();
}

Settings::~Settings()
{
    base_app_dirs_.clear();
}

bool Settings::load(const char* filePath)
{
    // TODO: move this to configd
    // launch reason
    AddLaunchReason("com.webos.surfacemanager", "launcher", "launcher");
    AddLaunchReason("com.webos.surfacemanager", "recent", "recent");
    AddLaunchReason("com.webos.surfacemanager", "autoReload", "autoReload");
    AddLaunchReason("com.webos.surfacemanager", "manualLastApp", "manualLastApp");
    AddLaunchReason("com.webos.surfacemanager", "autoLastApp", "autoLastApp");
    AddLaunchReason("com.webos.surfacemanager", "hotKey", "hotKey");
    AddLaunchReason("com.webos.surfacemanager", "wedge", "wedge");

    AddLaunchReason("com.webos.surfacemanager.inputpicker", "", "inputPicker");
    AddLaunchReason("com.webos.surfacemanager.quicksettings", "", "quickSetting");
    AddLaunchReason("com.webos.service.preloadmanager", "", "preload");
    AddLaunchReason("com.webos.keyfilter.magicnumber", "", "magicNumber");
    AddLaunchReason("com.webos.app.voiceagent", "", "voiceCommand");
    AddLaunchReason("com.webos.app.voiceview", "", "searchResult");
    AddLaunchReason("com.webos.app.voice", "", "searchResult");
    AddLaunchReason("com.webos.app.favshows", "", "myContents");
    AddLaunchReason("com.webos.app.livemenu", "", "directAccess");
    AddLaunchReason("com.webos.service.dial", "", "dial");
    AddLaunchReason("com.webos.bootManager", "", "boot");
    AddLaunchReason("com.webos.app.cpcover", "", "boot");

    // close reason
    AddCloseReason("com.webos.surfacemanager.windowext", "windowPolicy", "windowPolicy");
    AddCloseReason("com.webos.surfacemanager.windowext", "recent", "recent");
    AddCloseReason("com.webos.surfacemanager.windowext", "systemUI", "systemUI");

    AddCloseReason("com.webos.memorymanager", "", "memoryReclaim");
    AddCloseReason("com.webos.service.tvpower", "", "powerSuspend");
    AddCloseReason("com.webos.service.dial", "", "dial");

    return LoadStaticConfig(filePath);
}

void Settings::onRestLoad()
{
    isRestLoaded = true;
    LoadConfigOnRWFilesystemReady();
}

void Settings::LoadConfigOnRWFilesystemReady()
{
    // Add base app dir path for dev apps
    if (0 == access(devModePath.c_str(), F_OK)) {
        isDevMode = true;
        for (auto it : devAppsPaths) {
            AddBaseAppDirPath(it, AppTypeByDir::AppTypeByDir_Dev);
        }
    }

    LOG_INFO(MSGID_SETTING_INFO, 1, PMLOGKS("key", "devmode"), "val: %s", (isDevMode ? "on" : "off"));

    if (0 == access(respawnedPath.c_str(), F_OK)) {
        isRespawned = true;
    } else {
        //create respawnded file
        if (!writeFile(respawnedPath.c_str(), "")) {
            LOG_WARNING(MSGID_SETTINGS_ERR, 1, PMLOGKS("respawnedPath", respawnedPath.c_str()), "cannot create respawned file");
        }
    }

    LOG_INFO(MSGID_SAM_LOADING_SEQ, 2, PMLOGKS("status", "analyze_loading_reason"), PMLOGKS("loading_status", (isRespawned ? "respawned" : "fresh_start")), "");

    if (0 != g_mkdir_with_parents(appMgrPreferenceDir.c_str(), 0700)) {
        LOG_WARNING(MSGID_SETTINGS_ERR, 1, PMLOGKS("PreferencesDir", appMgrPreferenceDir.c_str()), "cannot create PreferenceDir");
    }

    // Load Deleted SystemApp list
    pbnjson::JValue json = JUtil::parseFile(deletedSystemAppListPath, "deletedSystemAppList", NULL);
    if (json.isNull()) {
        LOG_DEBUG("[DELETED SYSTEM APP LIST] fail to launch list");
        m_deletedSystemApps.put(DELETED_LIST_KEY, pbnjson::Array());
    } else {
        m_deletedSystemApps = json;
    }

    LOG_DEBUG("[Load deletedList] %s", m_deletedSystemApps.stringify().c_str());
}

bool Settings::LoadStaticConfig(const char* filePath)
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
        devModePath = root["DevModePath"].asString();

    if (root["JailModePath"].isString())
        jailModePath = root["JailModePath"].asString();

    if (root["JailerPath"].isString())
        jailerPath = root["JailerPath"].asString();

    if (root["QmlRunnerPath"].isString())
        qmlRunnerPath = root["QmlRunnerPath"].asString();

    if (root["AppShellRunnerPath"].isString())
        appshellRunnerPath = root["AppShellRunnerPath"].asString();

    if (root["RespawnedPath"].isString())
        respawnedPath = root["RespawnedPath"].asBool();

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
                devAppsPaths.push_back(path);
                continue;
            }
            else
                continue;

            LOG_DEBUG("PathType : %s, Path : %s", type.c_str(), path.c_str());

            AddBaseAppDirPath(path, typeByDir);
        }

        if (base_app_dirs_.size() < 1) {
            LOG_WARNING(MSGID_NO_APP_PATHS, 1, PMLOGKS("FILE", conf_path.c_str()), "");
            return false;
        }
    }

    if (root["UsePartialKeywordMatchForAppSearch"].isBoolean()) {
        usePartialKeywordAppSearch = root["UsePartialKeywordMatchForAppSearch"].asBool();
    }

    if (root.hasKey("FullscreenWindowType") && root["FullscreenWindowType"].isArray()) {
        int array_size = root["FullscreenWindowType"].arraySize();
        for (int i = 0; i < array_size; ++i) {
            std::string value;
            if (root["FullscreenWindowType"][i].asString(value) != CONV_OK)
                continue;
            fullscreen_window_types.push_back(value);
        }
    }

    if (root.hasKey("HostAppsForAlias") && root["HostAppsForAlias"].isArray()) {
        for (int i = 0; i < root["HostAppsForAlias"].arraySize(); ++i) {
            std::string app_id = "";
            if (!root["HostAppsForAlias"][i].isString() || root["HostAppsForAlias"][i].asString(app_id) != CONV_OK)
                continue;

            LOG_INFO("HOSTAPPS", 1, PMLOGKS("app_id", app_id.c_str()), "");
            host_apps_for_alias.push_back(app_id);
        }
    }

    if (root["NoJailApps"].isArray()) {
        pbnjson::JValue appList = root["NoJailApps"];
        int arraySize = appList.arraySize();
        for (int i = 0; i < arraySize; ++i) {
            std::string str;
            if (appList[i].asString(str) != CONV_OK)
                continue;
            noJailApps.push_back(str);
            LOG_DEBUG("NoJailApp: %s", str.c_str());
        }
    }

    if (root.hasKey("BootTimeApps") && root["BootTimeApps"].isArray()) {
        for (auto it : root["BootTimeApps"].items()) {
            if (!it.isString())
                continue;
            AddBootTimeApp(it.asString());
        }
    }

    if (root["StoreApp"].isString()) {
        app_store_id_ = root["StoreApp"].asString();
    }
    return true;
}

bool Settings::LoadSAMConf()
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
            SettingsImpl::instance().AddBootTimeApp(it.asString());
        }
    }

    if (root.hasKey("CRIUSupportApps") && root["CRIUSupportApps"].isArray()) {
        for (auto it : root["CRIUSupportApps"].items()) {
            if (!it.isString())
                continue;
            SettingsImpl::instance().AddCRIUSupportApp(it.asString());
        }
    }

    if (root.hasKey("LaunchPointDBKind"))
        launch_point_dbkind_ = root["LaunchPointDBKind"];

    if (root.hasKey("LaunchPointDBPermissions"))
        launch_point_permissions_ = root["LaunchPointDBPermissions"];

    return true;
}

void Settings::AddBaseAppDirPath(const std::string& path, AppTypeByDir type)
{
    base_app_dirs_.push_back( { path, type });
}

void Settings::setKeepAliveApps(const pbnjson::JValue& apps)
{
    if (!apps.isArray())
        return;
    keepAliveApps.clear();

    LOG_INFO(MSGID_CONFIGD_INIT, 1,
             PMLOGKS("CONFIG_TYPE", "keep_alive_apps"),
             "%s", apps.duplicate().stringify().c_str());

    int apps_num = apps.arraySize();
    for (int i = 0; i < apps_num; ++i) {
        std::string app_id = "";
        if (!apps[i].isString() || apps[i].asString(app_id) != CONV_OK)
            continue;

        LOG_INFO(MSGID_CONFIGD_INIT, 2, PMLOGKS("CONFIG_TYPE", "keep_alive_apps"), PMLOGKS("app_id", app_id.c_str()), "");
        keepAliveApps.push_back(app_id);
    }
}

void Settings::set_support_qml_booster(bool value)
{
    use_qml_booster_ = value;
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
    return (verified) ? appInstallBase : devAppsBasePath;
}

bool Settings::is_in_host_apps_list(const std::string& app_id) const
{
    auto it = std::find(host_apps_for_alias.begin(), host_apps_for_alias.end(), app_id);
    if (it != host_apps_for_alias.end())
        return true;
    return false;
}

bool Settings::IsKeepAliveApp(const std::string& appId) const
{
    auto it = std::find(keepAliveApps.begin(), keepAliveApps.end(), appId);
    if (it != keepAliveApps.end())
        return true;
    return false;
}

bool Settings::checkAppAgainstNoJailAppList(const std::string& appId) const
{
    auto it = std::find(noJailApps.begin(), noJailApps.end(), appId);
    if (it != noJailApps.end())
        return true;
    return false;
}

void Settings::AddBootTimeApp(const std::string& app_id)
{
    auto it = std::find(boot_time_apps_.begin(), boot_time_apps_.end(), app_id);
    if (it != boot_time_apps_.end())
        return;
    boot_time_apps_.push_back(app_id);
}

bool Settings::IsBootTimeApp(const std::string& app_id)
{
    for (auto& id : boot_time_apps_)
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

    if (!g_file_set_contents(deletedSystemAppListPath.c_str(), deletedList.c_str(), deletedList.length(), NULL)) {
        LOG_WARNING(MSGID_FAIL_WRITING_DELETEDLIST, 2, PMLOGKS("CONTENT", deletedList.c_str()), PMLOGKS("PATH", deletedSystemAppListPath.c_str()), "");
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

void Settings::SetLifeCycleReason(const pbnjson::JValue& data)
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

                AddLaunchReason(caller_id, reason, launch_reason);
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

                AddCloseReason(caller_id, reason, close_reason);
            }
        }
    }
}

void Settings::AddLaunchReason(const std::string& caller_id, const std::string& reason, const std::string& launch_reason)
{
    launch_event_reason_map_[caller_id + ":" + reason] = launch_reason;
}

std::string Settings::GetLaunchReason(const std::string& caller_id, const std::string& reason)
{
    std::string key = caller_id + ":" + reason;
    if (!launch_event_reason_map_.count(key)) {
        return "undefined";
    } else {
        return launch_event_reason_map_[key];
    }
}

void Settings::AddCloseReason(const std::string& caller_id, const std::string& reason, const std::string& close_reason)
{
    close_event_reason_map_[caller_id + ":" + reason] = close_reason;
}

std::string Settings::GetCloseReason(const std::string& caller_id, const std::string& reason)
{
    std::string key = caller_id + ":" + reason;
    if (!close_event_reason_map_.count(key)) {
        return "undefined";
    } else {
        return close_event_reason_map_[key];
    }
}

void Settings::AddCRIUSupportApp(const std::string& app_id)
{
    if (SupportCRIU(app_id))
        return;

    LOG_INFO(MSGID_SETTING_INFO, 2, PMLOGKS("set_key", "criu_app"), PMLOGKS("app_id", app_id.c_str()), "");
    criu_support_apps_.push_back(app_id);
}

bool Settings::SupportCRIU(const std::string& app_id) const
{
    for (auto id : criu_support_apps_) {
        if (id == app_id)
            return true;
    }
    return false;
}
