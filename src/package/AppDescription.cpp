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

#include <glib.h>
#include <stdio.h>
#include <sys/stat.h>

#include <cstring>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <bus/LocalePreferences.h>
#include <package/AppDescription.h>
#include <setting/Settings.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/Utils.h>
#include <string>


AppDescription::AppDescription()
    : m_appType(AppType::AppType_None),
      m_typeByDir(AppTypeByDir::AppTypeByDir_None),
      m_handlerType(LifeHandlerType::LifeHandlerType_None),
      m_version("1.0.0"), // default version
      m_intVersion(1, 0, 0),
      m_nativeInterfaceVersion(1),
      m_isBuiltinBasedApp(false),
      m_flaggedForRemoval(false),
      m_removable(false),
      m_visible(true),
      m_requiredMemory(0),
      m_windowGroup(false),
      m_windowGroupOwner(false),
      m_isLockedForExcution(false),
      m_splashOnLaunch(true),
      m_spinnerOnLaunch(false)
{
    m_deviceId = pbnjson::Object();
    m_redirection = pbnjson::Object();
}

AppDescription::~AppDescription()
{
}

std::string AppDescription::toString(AppType type)
{
    std::string str_appType;
    switch (type) {
    case AppType::AppType_Web:
        str_appType = "web";
        break;
    case AppType::AppType_Stub:
        str_appType = "stub";
        break;
    case AppType::AppType_Native:
        str_appType = "native";
        break;
    case AppType::AppType_Native_Builtin:
        str_appType = "native_builtin";
        break;
    case AppType::AppType_Native_Mvpd:
        str_appType = "native_mvpd";
        break;
    case AppType::AppType_Native_Qml:
        str_appType = "native_qml";
        break;
    case AppType::AppType_Native_AppShell:
        str_appType = "native_appshell";
        break;
    case AppType::AppType_Qml:
        str_appType = "qml";
        break;
    default:
        str_appType = "unknown";
        break;
    }
    return str_appType;
}

bool AppDescription::loadJson(pbnjson::JValue& jdesc, const AppTypeByDir& type_by_dir)
{
    if (jdesc.isNull()) {
        return false;
    }

    if ((!jdesc.hasKey("folderPath")) ||
        (!jdesc.hasKey("id")) ||
        (!jdesc.hasKey("main")) ||
        (!jdesc.hasKey("title"))) {
        return false;
    }

    // app_type_by_dir
    if (type_by_dir <= AppTypeByDir::AppTypeByDir_None) {
        LOG_WARNING(MSGID_PACKAGE_LOAD, 1, PMLOGKS("status", "invalide_type"), "type_by_dir: %d", (int )type_by_dir);
        return false;
    } else {
        m_typeByDir = type_by_dir;
    }

    // app_type
    std::string app_type = jdesc["type"].asString();
    bool privileged_jail = jdesc.hasKey("privilegedJail") ? jdesc["privilegedJail"].asBool() : false;

    if ("web" == app_type)
        m_appType = AppType::AppType_Web;
    else if ("stub" == app_type)
        m_appType = AppType::AppType_Stub;
    else if ("native" == app_type)
        m_appType = (privileged_jail) ? AppType::AppType_Native_Mvpd : AppType::AppType_Native;
    else if ("native_builtin" == app_type)
        m_appType = AppType::AppType_Native_Builtin;
    else if ("native_appshell" == app_type)
        m_appType = AppType::AppType_Native_AppShell;
    else if ("qml" == app_type && (AppTypeByDir::AppTypeByDir_System_BuiltIn == type_by_dir || AppTypeByDir::AppTypeByDir_System_Updatable == type_by_dir)) {
        if (SettingsImpl::instance().m_useQmlBooster)
            m_appType = AppType::AppType_Qml;
        else
            m_appType = AppType::AppType_Native_Qml;
    } else if ("qml" == app_type && AppTypeByDir::AppTypeByDir_Dev == type_by_dir) {
        if (SettingsImpl::instance().m_useQmlBooster)
            m_appType = AppType::AppType_Qml;
        else
            m_appType = AppType::AppType_Native_Qml;
    }
    else
        return false;

    // handler_type
    switch (m_appType) {
    case AppType::AppType_Web:
        m_handlerType = LifeHandlerType::LifeHandlerType_Web;
        break;
    case AppType::AppType_Qml:
        m_handlerType = LifeHandlerType::LifeHandlerType_Qml;
        break;
    case AppType::AppType_Native:
    case AppType::AppType_Native_AppShell:
    case AppType::AppType_Native_Builtin:
    case AppType::AppType_Native_Mvpd:
    case AppType::AppType_Native_Qml:
        m_handlerType = LifeHandlerType::LifeHandlerType_Native;
        break;
    case AppType::AppType_Stub:
    default:
        m_handlerType = LifeHandlerType::LifeHandlerType_None;
        break;
    }

    // folder_path
    m_folderPath = jdesc["folderPath"].asString();

    // load system asset
    loadAsset(jdesc);

    // app_id
    m_appId = jdesc["id"].asString();
    if (AppTypeByDir::AppTypeByDir_System_Updatable == type_by_dir && !isPrivileged())
        return false;
    if (AppTypeByDir::AppTypeByDir_Dev == type_by_dir && isPrivileged())
        return false;

    // title
    m_title = jdesc["title"].asString();

    // entry_point
    m_entryPoint = jdesc["main"].asString();
    if (!strstr(m_entryPoint.c_str(), "://"))
        m_entryPoint = std::string("file://") + m_folderPath + std::string("/") + m_entryPoint;

    // version
    m_version = jdesc["version"].asString();
    std::vector<std::string> version_info;
    boost::split(version_info, m_version, boost::is_any_of("."));
    uint16_t major_ver = version_info.size() > 0 ? (uint16_t) std::stoi(version_info[0]) : 0;
    uint16_t minor_ver = version_info.size() > 1 ? (uint16_t) std::stoi(version_info[1]) : 0;
    uint16_t micro_ver = version_info.size() > 2 ? (uint16_t) std::stoi(version_info[2]) : 0;
    setIntVersion(major_ver, minor_ver, micro_ver);

    // trust_level
    if (jdesc["trustLevel"].asString(m_trustLevel) != CONV_OK) {
        jdesc.put("trustLevel", "default");
        m_trustLevel = "default";
    }

    if (AppTypeByDir::AppTypeByDir_Dev == type_by_dir) {
        if (m_trustLevel == "trusted") {
            return false;
        }
        jdesc.put("inspectable", true);
    }

    // splash_background
    if (jdesc.hasKey("splashBackground")) {
        m_splashBackground = jdesc["splashBackground"].asString();

        if (!strstr(m_splashBackground.c_str(), "://"))
            m_splashBackground = std::string("file://") + m_folderPath + std::string("/") + m_splashBackground;
    }

    // native_interface_version
    if (jdesc.hasKey("nativeLifeCycleInterfaceVersion") && jdesc["nativeLifeCycleInterfaceVersion"].isNumber()) {
        m_nativeInterfaceVersion = jdesc["nativeLifeCycleInterfaceVersion"].asNumber<int>();
    }

    // system_app
    if (AppTypeByDir::AppTypeByDir_System_BuiltIn == type_by_dir || AppTypeByDir::AppTypeByDir_System_Updatable == type_by_dir) {
        jdesc.put("systemApp", true);
        m_isBuiltinBasedApp = true;
    } else {
        jdesc.put("systemApp", false);
    }

    // removable
    if (AppTypeByDir::AppTypeByDir_System_BuiltIn == type_by_dir || AppTypeByDir::AppTypeByDir_System_Updatable == type_by_dir) {
        m_removable = false;
        jdesc.put("removable", false);
    } else {
        m_removable = jdesc["removable"].asBool();
    }

    // visible: default true
    m_visible = jdesc["visible"].asBool();

    // required memory
    if (jdesc.hasKey("requiredMemory")) {
        m_requiredMemory = jdesc["requiredMemory"].asNumber<int>();
    }

    // default window type
    if (jdesc.hasKey("defaultWindowType"))
        m_defaultWindowType = jdesc["defaultWindowType"].asString();

    // window_group
    if (jdesc.hasKey("windowGroup") && jdesc["windowGroup"].hasKey("owner") && jdesc["windowGroup"]["owner"].isBoolean()) {
        m_windowGroup = true;
        m_windowGroupOwner = jdesc["windowGroup"]["owner"].asBool();
        m_windowGroupOwnerId = m_windowGroupOwner ? "" : jdesc["windowGroup"]["name"].asString();

        LOG_DEBUG("[AppDesc:WindowGroup] window_group: %s, window_group_owner: %s, window_group_owner_id: %s", m_windowGroup ? "true" : "false", m_windowGroupOwner ? "true" : "false",
                m_windowGroupOwnerId.c_str());
    }

    // redirection
    if (AppType::AppType_Stub == m_appType && jdesc.hasKey("redirection"))
        m_redirection = jdesc["redirection"];

    // splash_on_launch
    if (jdesc.hasKey("noSplashOnLaunch"))
        m_splashOnLaunch = !jdesc["noSplashOnLaunch"].asBool();

    // spinner_on_launch
    if (jdesc.hasKey("spinnerOnLaunch"))
        m_spinnerOnLaunch = jdesc["spinnerOnLaunch"].asBool();

    // launch_params: this params for launchPoint
    if (jdesc.hasKey("launchParams"))
        m_launchParams = jdesc["launchParams"];

    // containerJS: This is used for checking the app will use container app : optional
    if (jdesc.hasKey("containerJS"))
        m_containerJS = jdesc["containerJS"].asString();

    // enyoVersion: This is used for comparing with container app's enyo version.
    if (jdesc.hasKey("enyoVersion"))
        m_enyoVersion = jdesc["enyoVersion"].asString();

    m_appinfoJson = jdesc;
    return true;
}

void AppDescription::loadAsset(pbnjson::JValue& jdesc)
{
    static const std::vector<std::string> supporting_assets { "icon", "largeIcon", "bgImage", "splashBackground" };

    std::string variant = SettingsImpl::instance().m_packageAssetVariant;
    std::string asset_base_path = jdesc.hasKey("sysAssetsBasePath") ? jdesc["sysAssetsBasePath"].asString() : "sys-assets";
    set_slash_to_base_path(asset_base_path);

    for (const auto& key : supporting_assets) {
        std::string value;
        std::string variant_path;

        if (!jdesc.hasKey(key) || !jdesc[key].isString() || jdesc[key].asString(value) != CONV_OK || value.empty())
            continue;

        // check if it requests assets control finding symbol '$'
        if (value.length() < 2 || value[0] != '$') {
            continue;
        }

        std::string filename = value.substr(1);
        bool foundAsset = false;

        const std::vector<std::string>& fallbacks = SettingsImpl::instance().m_assetFallbackPrecedence;
        for (const auto& fallback_key : fallbacks) {
            std::string assetPath = asset_base_path + fallback_key + "/" + filename;
            std::string path_to_check = "";

            // check variant asset first
            if (!variant.empty() && concatToFilename(assetPath, variant_path, variant)) {
                path_to_check = m_folderPath + std::string("/") + variant_path;
                if (0 == access(path_to_check.c_str(), F_OK)) {
                    jdesc.put(key, variant_path);
                    foundAsset = true;
                    break;
                }
            }

            // set asset without variant
            path_to_check = m_folderPath + std::string("/") + assetPath;
            LOG_DEBUG("patch_to_check: %s\n", path_to_check.c_str());

            if (0 == access(path_to_check.c_str(), F_OK)) {
                jdesc.put(key, assetPath);
                foundAsset = true;
                break;
            }
        }

        if (foundAsset) {
            continue;
        }

        std::string defaultAsset = asset_base_path + filename;

        if (!variant.empty() && concatToFilename(defaultAsset, variant_path, variant)) {
            std::string path_variant_to_check = m_folderPath + std::string("/") + variant_path;
            if (0 == access(path_variant_to_check.c_str(), F_OK)) {
                jdesc.put(key, variant_path);
                continue;
            }
        }

        // failed to find all sys assets, set default now
        jdesc.put(key, defaultAsset);
    }
}

bool AppDescription::getSelectedPropsFromAppInfo(const pbnjson::JValue& appinfo, const pbnjson::JValue& wanted_props, pbnjson::JValue& result)
{

    if (appinfo.isNull() || !appinfo.isObject() || wanted_props.isNull() || !wanted_props.isArray() || wanted_props.arraySize() < 1 || result.isNull() || !result.isObject()) {
        return false;
    }

    pbnjson::JValue emptyProperty = pbnjson::Array();

    // get selected props from appinfo
    for (int i = 0; i < wanted_props.arraySize(); ++i) {
        std::string strKey = "";
        if (!wanted_props[i].isString() || wanted_props[i].asString(strKey) != CONV_OK)
            continue;
        if (result.hasKey(strKey))
            continue;

        if (appinfo.hasKey(strKey))
            result.put(strKey, appinfo[strKey]);
        else
            JUtil::addStringToStrArrayNoDuplicate(emptyProperty, strKey);
    }

    // add not-specified info if there's missed keys
    if (emptyProperty.arraySize() > 0)
        result.put("notSpecified", emptyProperty);

    return true;
}

bool AppDescription::getSelectedPropsFromApps(const pbnjson::JValue& apps, const pbnjson::JValue& wanted_props, pbnjson::JValue& result)
{

    if (apps.isNull() || !apps.isArray() || wanted_props.isNull() || !wanted_props.isArray() || wanted_props.arraySize() < 1 || result.isNull() || !result.isArray()) {
        return false;
    }

    for (int i = 0; i < apps.arraySize(); ++i) {
        pbnjson::JValue new_props = pbnjson::Object();
        if (!getSelectedPropsFromAppInfo(apps[i], wanted_props, new_props)) {
            LOG_WARNING(MSGID_FAIL_GET_SELECTED_PROPS, 1, PMLOGKS("where", __FUNCTION__), "line: %d", __LINE__);
            continue;
        }

        result.append(new_props);
    }

    return true;
}

bool AppDescription::isPrivileged() const
{
    if (m_appId.find("com.palm.") == 0 || m_appId.find("com.webos.") == 0 || m_appId.find("com.lge.") == 0)
        return true;
    return false;
}

bool AppDescription::securityChecksVerified()
{
    if (m_folderPath.length() <= m_appId.length() ||
        strcmp(m_folderPath.c_str() + m_folderPath.length() - m_appId.length(), m_appId.c_str()) != 0) {
        LOG_ERROR(MSGID_SECURITY_VIOLATION, 3,
                  PMLOGKS("TITLE", m_title.c_str()),
                  PMLOGKS("PATH", m_folderPath.c_str()),
                  PMLOGKS("APP_ID", m_appId.c_str()), "App path does not match app id");
        return false;
    }
    return true;
}

bool AppDescription::isHigherVersionThanMe(AppDescPtr me, AppDescPtr another)
{

    // always return true, if remove flag is on for current app
    if (me->isRemoveFlagged())
        return true;

    // Non dev app has always higher priority than dev apps
    if (AppTypeByDir::AppTypeByDir_Dev != me->getTypeByDir() && AppTypeByDir::AppTypeByDir_Dev == another->getTypeByDir()) {
        return false;
    }

    const AppIntVersion& me_ver = me->getIntVersion();
    const AppIntVersion& new_ver = another->getIntVersion();

    // compare version
    if (std::get<0>(me_ver) < std::get<0>(new_ver))
        return true;
    else if (std::get<0>(me_ver) > std::get<0>(new_ver))
        return false;
    if (std::get<1>(me_ver) < std::get<1>(new_ver))
        return true;
    else if (std::get<1>(me_ver) > std::get<1>(new_ver))
        return false;
    if (std::get<2>(me_ver) < std::get<2>(new_ver))
        return true;
    else if (std::get<2>(me_ver) > std::get<2>(new_ver))
        return false;

    // if same version, check type_by_dir priority
    if ((int) me->getTypeByDir() > (int) me->getTypeByDir())
        return true;

    return false;
}

bool AppDescription::isSame(AppDescPtr me, AppDescPtr another)
{
    if (me->m_appId != another->m_appId)
        return false;
    if (me->m_folderPath != another->m_folderPath)
        return false;
    if (me->m_version != another->m_version)
        return false;
    return true;
}
