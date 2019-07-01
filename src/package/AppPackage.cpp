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
#include <bus/client/SettingService.h>
#include <package/AppPackage.h>
#include <setting/Settings.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/Utils.h>
#include <string>

std::string AppPackage::toString(AppType type)
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

bool AppPackage::getSelectedPropsFromAppInfo(const pbnjson::JValue& appinfo, const pbnjson::JValue& wanted_props, pbnjson::JValue& result)
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

bool AppPackage::getSelectedPropsFromApps(const pbnjson::JValue& apps, const pbnjson::JValue& wanted_props, pbnjson::JValue& result)
{

    if (apps.isNull() || !apps.isArray() || wanted_props.isNull() || !wanted_props.isArray() || wanted_props.arraySize() < 1 || result.isNull() || !result.isArray()) {
        return false;
    }

    for (int i = 0; i < apps.arraySize(); ++i) {
        pbnjson::JValue new_props = pbnjson::Object();
        if (!getSelectedPropsFromAppInfo(apps[i], wanted_props, new_props)) {
            LOG_WARNING(MSGID_FAIL_GET_SELECTED_PROPS, 2,
                        PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                        PMLOGKFV(LOG_KEY_LINE, "%d", __LINE__), "");
            continue;
        }

        result.append(new_props);
    }

    return true;
}

bool AppPackage::isHigherVersionThanMe(AppPackagePtr me, AppPackagePtr another)
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

bool AppPackage::isSame(AppPackagePtr me, AppPackagePtr another)
{
    if (me->m_appId != another->m_appId)
        return false;
    if (me->m_folderPath != another->m_folderPath)
        return false;
    if (me->m_version != another->m_version)
        return false;
    return true;
}

AppPackage::AppPackage()
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
      m_splashOnLaunch(true),
      m_spinnerOnLaunch(false),
      m_isLocked(false)
{
}

AppPackage::~AppPackage()
{
}

bool AppPackage::loadJson(pbnjson::JValue& appinfo, const AppTypeByDir& appTypeByDir)
{
    if (appinfo.isNull()) {
        return false;
    }

    if ((!appinfo.hasKey("folderPath")) ||
        (!appinfo.hasKey("id")) ||
        (!appinfo.hasKey("main")) ||
        (!appinfo.hasKey("title"))) {
        return false;
    }

    // app_type_by_dir
    if (appTypeByDir <= AppTypeByDir::AppTypeByDir_None) {
        LOG_WARNING(MSGID_PACKAGE_LOAD, 1,
                    PMLOGKS("status", "invalide_type"),
                    "type_by_dir: %d", (int )appTypeByDir);
        return false;
    }
    m_typeByDir = appTypeByDir;

    // app_type
    std::string type = appinfo["type"].asString();
    bool privilegedJail = appinfo.hasKey("privilegedJail") ? appinfo["privilegedJail"].asBool() : false;

    if ("web" == type)
        m_appType = AppType::AppType_Web;
    else if ("stub" == type)
        m_appType = AppType::AppType_Stub;
    else if ("native" == type)
        m_appType = (privilegedJail) ? AppType::AppType_Native_Mvpd : AppType::AppType_Native;
    else if ("native_builtin" == type)
        m_appType = AppType::AppType_Native_Builtin;
    else if ("native_appshell" == type)
        m_appType = AppType::AppType_Native_AppShell;
    else if ("qml" == type && (AppTypeByDir::AppTypeByDir_System_BuiltIn == appTypeByDir || AppTypeByDir::AppTypeByDir_System_Updatable == appTypeByDir)) {
        if (SettingsImpl::getInstance().m_useQmlBooster)
            m_appType = AppType::AppType_Qml;
        else
            m_appType = AppType::AppType_Native_Qml;
    } else if ("qml" == type && AppTypeByDir::AppTypeByDir_Dev == appTypeByDir) {
        if (SettingsImpl::getInstance().m_useQmlBooster)
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
    m_folderPath = appinfo["folderPath"].asString();

    // load system asset
    loadAsset(appinfo);

    // app_id
    m_appId = appinfo["id"].asString();
    if (AppTypeByDir::AppTypeByDir_System_Updatable == appTypeByDir && !isPrivileged())
        return false;
    if (AppTypeByDir::AppTypeByDir_Dev == appTypeByDir && isPrivileged())
        return false;

    // title
    m_title = appinfo["title"].asString();

    // entry_point
    m_main = appinfo["main"].asString();
    if (!strstr(m_main.c_str(), "://"))
        m_main = std::string("file://") + m_folderPath + std::string("/") + m_main;

    // version
    m_version = appinfo["version"].asString();
    std::vector<std::string> version_info;
    boost::split(version_info, m_version, boost::is_any_of("."));
    uint16_t major_ver = version_info.size() > 0 ? (uint16_t) std::stoi(version_info[0]) : 0;
    uint16_t minor_ver = version_info.size() > 1 ? (uint16_t) std::stoi(version_info[1]) : 0;
    uint16_t micro_ver = version_info.size() > 2 ? (uint16_t) std::stoi(version_info[2]) : 0;
    setIntVersion(major_ver, minor_ver, micro_ver);

    if (!appinfo.hasKey("trustLevel")) {
        appinfo.put("trustLevel", "default");
    }
    if (AppTypeByDir::AppTypeByDir_Dev == appTypeByDir) {
        appinfo.put("inspectable", true);
    }

    // splash_background
    if (appinfo.hasKey("splashBackground")) {
        m_splashBackground = appinfo["splashBackground"].asString();

        if (!strstr(m_splashBackground.c_str(), "://"))
            m_splashBackground = std::string("file://") + m_folderPath + std::string("/") + m_splashBackground;
    }

    // native_interface_version
    if (appinfo.hasKey("nativeLifeCycleInterfaceVersion") && appinfo["nativeLifeCycleInterfaceVersion"].isNumber()) {
        m_nativeInterfaceVersion = appinfo["nativeLifeCycleInterfaceVersion"].asNumber<int>();
    }

    // system_app
    if (AppTypeByDir::AppTypeByDir_System_BuiltIn == appTypeByDir ||
        AppTypeByDir::AppTypeByDir_System_Updatable == appTypeByDir) {
        appinfo.put("systemApp", true);
        m_isBuiltinBasedApp = true;
    } else {
        appinfo.put("systemApp", false);
    }

    // removable
    if (AppTypeByDir::AppTypeByDir_System_BuiltIn == appTypeByDir ||
        AppTypeByDir::AppTypeByDir_System_Updatable == appTypeByDir) {
        m_removable = false;
        appinfo.put("removable", false);
    } else {
        m_removable = appinfo["removable"].asBool();
    }

    // visible: default true
    m_visible = appinfo["visible"].asBool();

    // required memory
    if (appinfo.hasKey("requiredMemory")) {
        m_requiredMemory = appinfo["requiredMemory"].asNumber<int>();
    }

    // default window type
    if (appinfo.hasKey("defaultWindowType"))
        m_defaultWindowType = appinfo["defaultWindowType"].asString();

    // window_group
    if (appinfo.hasKey("windowGroup") &&
        appinfo["windowGroup"].hasKey("owner") &&
        appinfo["windowGroup"]["owner"].isBoolean()) {
        m_windowGroup = true;
        m_windowGroupOwner = appinfo["windowGroup"]["owner"].asBool();

        LOG_DEBUG("[AppDesc:WindowGroup] window_group: %s, window_group_owner: %s",
                  m_windowGroup ? "true" : "false",
                  m_windowGroupOwner ? "true" : "false");
    }

    // splash_on_launch
    if (appinfo.hasKey("noSplashOnLaunch"))
        m_splashOnLaunch = !appinfo["noSplashOnLaunch"].asBool();

    // spinner_on_launch
    if (appinfo.hasKey("spinnerOnLaunch"))
        m_spinnerOnLaunch = appinfo["spinnerOnLaunch"].asBool();

    m_appinfo = appinfo;
    return true;
}

void AppPackage::loadAsset(pbnjson::JValue& appinfo)
{
    static const std::vector<std::string> supportingAssets { "icon", "largeIcon", "bgImage", "splashBackground" };

    std::string variant = SettingsImpl::getInstance().m_packageAssetVariant;
    std::string asset_base_path = appinfo.hasKey("sysAssetsBasePath") ? appinfo["sysAssetsBasePath"].asString() : "sys-assets";
    set_slash_to_base_path(asset_base_path);

    for (const auto& key : supportingAssets) {
        std::string value;
        std::string variant_path;

        if (!appinfo.hasKey(key) || !appinfo[key].isString() || appinfo[key].asString(value) != CONV_OK || value.empty())
            continue;

        // check if it requests assets control finding symbol '$'
        if (value.length() < 2 || value[0] != '$') {
            continue;
        }

        std::string filename = value.substr(1);
        bool foundAsset = false;

        const std::vector<std::string>& fallbacks = SettingsImpl::getInstance().m_assetFallbackPrecedence;
        for (const auto& fallback_key : fallbacks) {
            std::string assetPath = asset_base_path + fallback_key + "/" + filename;
            std::string path_to_check = "";

            // check variant asset first
            if (!variant.empty() && concatToFilename(assetPath, variant_path, variant)) {
                path_to_check = m_folderPath + std::string("/") + variant_path;
                if (0 == access(path_to_check.c_str(), F_OK)) {
                    appinfo.put(key, variant_path);
                    foundAsset = true;
                    break;
                }
            }

            // set asset without variant
            path_to_check = m_folderPath + std::string("/") + assetPath;
            LOG_DEBUG("patch_to_check: %s\n", path_to_check.c_str());

            if (0 == access(path_to_check.c_str(), F_OK)) {
                appinfo.put(key, assetPath);
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
                appinfo.put(key, variant_path);
                continue;
            }
        }

        // failed to find all sys assets, set default now
        appinfo.put(key, defaultAsset);
    }
}

bool AppPackage::isPrivileged() const
{
    if (m_appId.find("com.palm.") == 0 || m_appId.find("com.webos.") == 0 || m_appId.find("com.lge.") == 0)
        return true;
    return false;
}

bool AppPackage::securityChecksVerified()
{
    if (m_folderPath.length() <= m_appId.length() ||
        strcmp(m_folderPath.c_str() + m_folderPath.length() - m_appId.length(), m_appId.c_str()) != 0) {
        LOG_ERROR(MSGID_SECURITY_VIOLATION, 3,
                  PMLOGKS("title", m_title.c_str()),
                  PMLOGKS("path", m_folderPath.c_str()),
                  PMLOGKS(LOG_KEY_APPID, m_appId.c_str()), "App path does not match app id");
        return false;
    }
    return true;
}
