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

#include "core/package/application_description.h"

#include <glib.h>
#include <stdio.h>
#include <sys/stat.h>

#include <cstring>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <string>

#include "core/base/jutil.h"
#include "core/base/logging.h"
#include "core/base/utils.h"
#include "core/module/locale_preferences.h"
#include "core/setting/settings.h"

ApplicationDescription::ApplicationDescription()
    : app_type_(AppType::None),
      type_by_dir_(AppTypeByDir::None),
      handler_type_(LifeHandlerType::None),
      version_("1.0.0"), // default version
      int_version_(1, 0, 0),
      native_interface_version_(1),
      is_builtin_based_app_(false),
      flagged_for_removal_(false),
      removable_(false),
      visible_(true),
      required_memory_(0),
      window_group_(false),
      window_group_owner_(false),
      is_locked_for_excution_(false),
      lockable_(true),
      splash_on_launch_(true),
      spinner_on_launch_(false),
      boot_params_(pbnjson::JValue())
{
    m_deviceId = pbnjson::Object();
    redirection_ = pbnjson::Object();
}

ApplicationDescription::~ApplicationDescription()
{
}

std::string ApplicationDescription::appTypeToString(AppType type)
{
    std::string str_appType;
    switch (type) {
    case AppType::Web:
        str_appType = "web";
        break;
    case AppType::Stub:
        str_appType = "stub";
        break;
    case AppType::Native:
        str_appType = "native";
        break;
    case AppType::Native_Builtin:
        str_appType = "native_builtin";
        break;
    case AppType::Native_Mvpd:
        str_appType = "native_mvpd";
        break;
    case AppType::Native_Qml:
        str_appType = "native_qml";
        break;
    case AppType::Native_AppShell:
        str_appType = "native_appshell";
        break;
    case AppType::Qml:
        str_appType = "qml";
        break;
    default:
        str_appType = "unknown";
        break;
    }
    return str_appType;
}

std::string ApplicationDescription::CreateRegexpFromSchema(const std::string& schema)
{
    return std::string("^") + schema + std::string("://.*");
}

bool ApplicationDescription::LoadJson(pbnjson::JValue& jdesc, const AppTypeByDir& type_by_dir)
{

    if (jdesc.isNull()) {
        return false;
    }

    if ((!jdesc.hasKey("folderPath")) || (!jdesc.hasKey("id")) || (!jdesc.hasKey("main")) || (!jdesc.hasKey("title"))) {
        return false;
    }

    // app_type_by_dir
    if (type_by_dir <= AppTypeByDir::None && type_by_dir > AppTypeByDir::TempAlias) {
        LOG_WARNING(MSGID_PACKAGE_LOAD, 1, PMLOGKS("status", "invalide_type"), "type_by_dir: %d", (int )type_by_dir);
        return false;
    } else {
        type_by_dir_ = type_by_dir;
    }

    // app_type
    std::string app_type = jdesc["type"].asString();
    bool privileged_jail = jdesc.hasKey("privilegedJail") ? jdesc["privilegedJail"].asBool() : false;

    if ("web" == app_type)
        app_type_ = AppType::Web;
    else if ("stub" == app_type)
        app_type_ = AppType::Stub;
    else if ("native" == app_type)
        app_type_ = (privileged_jail) ? AppType::Native_Mvpd : AppType::Native;
    else if ("native_builtin" == app_type)
        app_type_ = AppType::Native_Builtin;
    else if ("native_appshell" == app_type)
        app_type_ = AppType::Native_AppShell;
    else if ("qml" == app_type && (AppTypeByDir::System_BuiltIn == type_by_dir || AppTypeByDir::System_Updatable == type_by_dir)) {
        if (SettingsImpl::instance().use_qml_booster_)
            app_type_ = AppType::Qml;
        else
            app_type_ = AppType::Native_Qml;
    }
    // TODO - MIGRATION (START)
    // Remove below line after inputcommon CCC (submissions/736)
    // See more info : https://gpro.lgsvl.com/#/c/195309
    else if ("qml" == app_type && AppTypeByDir::Dev == type_by_dir) {
        if (SettingsImpl::instance().use_qml_booster_)
            app_type_ = AppType::Qml;
        else
            app_type_ = AppType::Native_Qml;
    }
    // TODO - MIGRATION (END)
    else
        return false;

    // handler_type
    switch (app_type_) {
    case AppType::Web:
        handler_type_ = LifeHandlerType::Web;
        break;
    case AppType::Qml:
        handler_type_ = LifeHandlerType::Qml;
        break;
    case AppType::Native:
    case AppType::Native_AppShell:
    case AppType::Native_Builtin:
    case AppType::Native_Mvpd:
    case AppType::Native_Qml:
        handler_type_ = LifeHandlerType::Native;
        break;
    case AppType::Stub:
    default:
        handler_type_ = LifeHandlerType::None;
        break;
    }

    // folder_path
    folder_path_ = jdesc["folderPath"].asString();
    if (AppTypeByDir::Alias == type_by_dir || AppTypeByDir::TempAlias == type_by_dir) {
        folder_path_ = jdesc["hostFolderPath"].asString();
        jdesc.put("folderPath", folder_path_);
    }

    // load system asset
    LoadAsset(jdesc);

    // app_id
    app_id_ = jdesc["id"].asString();
    if (AppTypeByDir::System_Updatable == type_by_dir && !isPrivileged())
        return false;
    if (AppTypeByDir::Dev == type_by_dir && isPrivileged())
        return false;

    // title
    title_ = jdesc["title"].asString();

    // entry_point
    entry_point_ = jdesc["main"].asString();
    if (!strstr(entry_point_.c_str(), "://"))
        entry_point_ = std::string("file://") + folder_path_ + std::string("/") + entry_point_;

    // version
    version_ = jdesc["version"].asString();
    std::vector<std::string> version_info;
    boost::split(version_info, version_, boost::is_any_of("."));
    uint16_t major_ver = version_info.size() > 0 ? (uint16_t) std::stoi(version_info[0]) : 0;
    uint16_t minor_ver = version_info.size() > 1 ? (uint16_t) std::stoi(version_info[1]) : 0;
    uint16_t micro_ver = version_info.size() > 2 ? (uint16_t) std::stoi(version_info[2]) : 0;
    SetIntVersion(major_ver, minor_ver, micro_ver);

    // trust_level
    if (jdesc["trustLevel"].asString(trust_level_) != CONV_OK) {
        jdesc.put("trustLevel", "default");
        trust_level_ = "default";
    }

    if (AppTypeByDir::Dev == type_by_dir) {
        if (trust_level_ == "trusted") {
            return false;
        }
        jdesc.put("inspectable", true);
    }

    // splash_background
    if (jdesc.hasKey("splashBackground")) {
        splash_background_ = jdesc["splashBackground"].asString();
        if (AppTypeByDir::Alias == type_by_dir || AppTypeByDir::TempAlias == type_by_dir) {
            splash_background_ = std::string("file://") + splash_background_;
        } else {
            if (!strstr(splash_background_.c_str(), "://"))
                splash_background_ = std::string("file://") + folder_path_ + std::string("/") + splash_background_;
        }
    }

    // native_interface_version
    if (jdesc.hasKey("nativeLifeCycleInterfaceVersion") && jdesc["nativeLifeCycleInterfaceVersion"].isNumber()) {
        native_interface_version_ = jdesc["nativeLifeCycleInterfaceVersion"].asNumber<int>();
    }

    // system_app
    if (AppTypeByDir::System_BuiltIn == type_by_dir || AppTypeByDir::System_Updatable == type_by_dir) {
        jdesc.put("systemApp", true);
        is_builtin_based_app_ = true;
    } else {
        jdesc.put("systemApp", false);
    }

    // removable
    if (AppTypeByDir::System_BuiltIn == type_by_dir || AppTypeByDir::System_Updatable == type_by_dir) {
        removable_ = false;
        jdesc.put("removable", false);
    } else {
        removable_ = jdesc["removable"].asBool();
    }

    // visible: default true
    visible_ = jdesc["visible"].asBool();

    // required memory
    if (jdesc.hasKey("requiredMemory")) {
        required_memory_ = jdesc["requiredMemory"].asNumber<int>();
    }

    // default window type
    if (jdesc.hasKey("defaultWindowType"))
        default_window_type_ = jdesc["defaultWindowType"].asString();

    // window_group
    if (jdesc.hasKey("windowGroup") && jdesc["windowGroup"].hasKey("owner") && jdesc["windowGroup"]["owner"].isBoolean()) {
        window_group_ = true;
        window_group_owner_ = jdesc["windowGroup"]["owner"].asBool();
        window_group_owner_id_ = window_group_owner_ ? "" : jdesc["windowGroup"]["name"].asString();

        LOG_DEBUG("[AppDesc:WindowGroup] window_group: %s, window_group_owner: %s, window_group_owner_id: %s", window_group_ ? "true" : "false", window_group_owner_ ? "true" : "false",
                window_group_owner_id_.c_str());
    }

    // redirection
    if (AppType::Stub == app_type_ && jdesc.hasKey("redirection"))
        redirection_ = jdesc["redirection"];

    // lockable
    if (jdesc.hasKey("lockable"))
        lockable_ = jdesc["lockable"].asBool();

    // splash_on_launch
    if (jdesc.hasKey("noSplashOnLaunch"))
        splash_on_launch_ = !jdesc["noSplashOnLaunch"].asBool();

    // spinner_on_launch
    if (jdesc.hasKey("spinnerOnLaunch"))
        spinner_on_launch_ = jdesc["spinnerOnLaunch"].asBool();

    // boot_params: use for auto-launch
    if (jdesc.hasKey("bootLaunchParams"))
        boot_params_ = jdesc["bootLaunchParams"];

    // launch_params: this params for launchPoint
    if (jdesc.hasKey("launchParams"))
        launch_params_ = jdesc["launchParams"];

    // containerJS: This is used for checking the app will use container app : optional
    if (jdesc.hasKey("containerJS"))
        container_js_ = jdesc["containerJS"].asString();

    // enyoVersion: This is used for comparing with container app's enyo version.
    if (jdesc.hasKey("enyoVersion"))
        enyo_version_ = jdesc["enyoVersion"].asString();

    // Set installed time to 0 for built-in apps and dev apps
    if (!jdesc.hasKey("installTime"))
        jdesc.put("installTime", 0);

    appinfo_json_ = jdesc;
    return true;
}

void ApplicationDescription::LoadAsset(pbnjson::JValue& jdesc)
{

    static const std::vector<std::string> supporting_assets { "icon", "largeIcon", "bgImage", "splashBackground" };

    std::string variant = SettingsImpl::instance().package_asset_variant_;
    std::string asset_base_path = jdesc.hasKey("sysAssetsBasePath") ? jdesc["sysAssetsBasePath"].asString() : "sys-assets";
    set_slash_to_base_path(asset_base_path);

    for (const auto& key : supporting_assets) {
        std::string value;
        std::string variant_path;

        if (!jdesc.hasKey(key) || !jdesc[key].isString() || jdesc[key].asString(value) != CONV_OK || value.empty())
            continue;

        // check if it requests assets control finding symbol '$'
        if (value.length() < 2 || value[0] != '$')
            continue;

        std::string filename = value.substr(1);
        bool foundAsset = false;

        const std::vector<std::string>& fallbacks = SettingsImpl::instance().m_assetFallbackPrecedence;
        for (const auto& fallback_key : fallbacks) {
            std::string assetPath = asset_base_path + fallback_key + "/" + filename;
            std::string path_to_check = "";

            // check variant asset first
            if (!variant.empty() && concat_to_filename(assetPath, variant_path, variant)) {
                path_to_check = folder_path_ + std::string("/") + variant_path;
                if (0 == access(path_to_check.c_str(), F_OK)) {
                    jdesc.put(key, variant_path);
                    foundAsset = true;
                    break;
                }
            }

            // set asset without variant
            path_to_check = folder_path_ + std::string("/") + assetPath;
            LOG_DEBUG("patch_to_check: %s\n", path_to_check.c_str());

            if (0 == access(path_to_check.c_str(), F_OK)) {
                jdesc.put(key, assetPath);
                foundAsset = true;
                break;
            }
        }

        if (foundAsset)
            continue;

        std::string defaultAsset = asset_base_path + filename;

        if (!variant.empty() && concat_to_filename(defaultAsset, variant_path, variant)) {
            std::string path_variant_to_check = folder_path_ + std::string("/") + variant_path;
            if (0 == access(path_variant_to_check.c_str(), F_OK)) {
                jdesc.put(key, variant_path);
                continue;
            }
        }

        // failed to find all sys assets, set default now
        jdesc.put(key, defaultAsset);
    }
}

bool ApplicationDescription::getSelectedPropsFromAppInfo(const pbnjson::JValue& appinfo, const pbnjson::JValue& wanted_props, pbnjson::JValue& result)
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

bool ApplicationDescription::getSelectedPropsFromApps(const pbnjson::JValue& apps, const pbnjson::JValue& wanted_props, pbnjson::JValue& result)
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

bool ApplicationDescription::isPrivileged() const
{
    if (app_id_.find("com.palm.") == 0 || app_id_.find("com.webos.") == 0 || app_id_.find("com.lge.") == 0)
        return true;
    return false;
}

bool ApplicationDescription::securityChecksVerified()
{
    if (AppTypeByDir::Alias == type_by_dir_ || AppTypeByDir::TempAlias == type_by_dir_) {
        return true;
    }

    if (folder_path_.length() <= app_id_.length() || strcmp(folder_path_.c_str() + folder_path_.length() - app_id_.length(), app_id_.c_str()) != 0) {
        LOG_ERROR(MSGID_SECURITY_VIOLATION, 3, PMLOGKS("TITLE", title_.c_str()), PMLOGKS("PATH", folder_path_.c_str()), PMLOGKS("APP_ID", app_id_.c_str()), "App path does not match app id");
        return false;
    }
    return true;
}

int ApplicationDescription::ExtractMimeTypes(const pbnjson::JValue& jsonMimeTypeArray, std::vector<MimeRegInfo>& extractedMimeTypes)
{
    if (jsonMimeTypeArray.isNull())
        return 0;

    // "mimeTypes": [  {"mime":<string> , "stream":<bool> },
    //                 { "extension":<string> , "stream":<bool> },
    //                 { "urlPattern":<string;regexp> },
    //                 { "scheme":<string;regexp> }
    //              ]

    int rc = 0;
    for (int listIdx = 0; listIdx < jsonMimeTypeArray.arraySize(); ++listIdx) {
        pbnjson::JValue jsonListObject = jsonMimeTypeArray[listIdx];
        if (jsonListObject.isNull())
            continue;
        if (!jsonListObject.isObject())
            continue;
        MimeRegInfo mri;
        bool r = false;
        r |= jsonListObject["mime"].asString(mri.mimeType) == CONV_OK;
        r |= jsonListObject["extension"].asString(mri.extension) == CONV_OK;
        r |= jsonListObject["urlPattern"].asString(mri.urlPattern) == CONV_OK;
        r |= jsonListObject["scheme"].asString(mri.scheme) == CONV_OK;

        if (jsonListObject["stream"].asBool(mri.stream) != CONV_OK) {
            mri.stream = false;
        }

        if (r) {
            extractedMimeTypes.push_back(mri);
            ++rc;
        }
    }

    return rc;

}

AppVersion ApplicationDescription::GetAppVersionStruct(std::string strVersion)
{

    AppVersion structVersion;

    std::vector<std::string> parsingVersion;
    boost::split(parsingVersion, strVersion, boost::is_any_of("."));

    if (APP_VERSION_DIGIT == parsingVersion.size()) {
        structVersion.major = boost::lexical_cast<int>(parsingVersion[0]);
        structVersion.minor = boost::lexical_cast<int>(parsingVersion[1]);
        structVersion.micro = boost::lexical_cast<int>(parsingVersion[2]);
    }
    return structVersion;
}

bool ApplicationDescription::IsHigherVersionThanMe(AppDescPtr me, AppDescPtr another)
{

    // always return true, if remove flag is on for current app
    if (me->isRemoveFlagged())
        return true;

    // Non dev app has always higher priority than dev apps
    if (AppTypeByDir::Dev != me->getTypeByDir() && AppTypeByDir::Dev == another->getTypeByDir()) {
        return false;
    }

    const AppIntVersion& me_ver = me->IntVersion();
    const AppIntVersion& new_ver = another->IntVersion();

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

bool ApplicationDescription::IsSame(AppDescPtr me, AppDescPtr another)
{
    if (me->app_id_ != another->app_id_)
        return false;
    if (me->folder_path_ != another->folder_path_)
        return false;
    if (me->version_ != another->version_)
        return false;
    return true;
}
