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

#include <glib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string>
#include <cstring>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "base/AppDescription.h"
#include "bus/client/SettingService.h"
#include "conf/SAMConf.h"
#include "util/JValueUtil.h"
#include "util/File.h"

const std::vector<std::string> AppDescription::PROPS_PROHIBITED = {
    "id", "type", "trustLevel"
};
const std::vector<std::string> AppDescription::ASSETS_SUPPORTED = {
    "icon", "largeIcon", "bgImage", "splashBackground"
};
const std::vector<std::string> AppDescription::PROPS_IMAGES = {
    "icon", "largeIcon", "bgImage", "splashBackground", "miniicon", "mediumIcon", "splashicon", "imageForRecents"
};

const string AppDescription::CLASS_NAME = "AppDescription";

std::string AppDescription::toString(const AppStatusEvent& event)
{
    switch (event) {
    case AppStatusEvent::AppStatusEvent_Nothing:
        return "";
        break;

    case AppStatusEvent::AppStatusEvent_Installed:
        return "appInstalled";
        break;

    case AppStatusEvent::AppStatusEvent_Uninstalled:
        return "appUninstalled";
        break;

    case AppStatusEvent::AppStatusEvent_UpdateCompleted:
        return "updateCompleted";
        break;

    default:
        return "unknown";
        break;
    }
}

std::string AppDescription::toString(AppType type)
{
    std::string str;
    switch (type) {
    case AppType::AppType_Web:
        str = "web";
        break;

    case AppType::AppType_Stub:
        str = "stub";
        break;

    case AppType::AppType_Native:
        str = "native";
        break;

    case AppType::AppType_Native_Builtin:
        str = "native_builtin";
        break;

    case AppType::AppType_Native_Mvpd:
        str = "native_mvpd";
        break;

    case AppType::AppType_Native_Qml:
        str = "native_qml";
        break;

    case AppType::AppType_Native_AppShell:
        str = "native_appshell";
        break;

    case AppType::AppType_Booster:
        str = "qml";
        break;

    default:
        str = "unknown";
        break;
    }
    return str;
}

AppType AppDescription::toAppType(const string& type)
{
    if (type == "web") {
        return AppType::AppType_Web;
    } else if (type == "stub") {
        return AppType::AppType_Stub;
    } else if (type == "native") {
        return AppType::AppType_Native;
    } else if (type == "native_builtin") {
        return AppType::AppType_Native_Builtin;
    } else if (type == "native_mvpd") {
        return AppType::AppType_Native_Mvpd;
    } else if (type == "native_qml") {
        return AppType::AppType_Native_Qml;
    } else if (type == "native_appshell") {
        return AppType::AppType_Native_AppShell;
    } else if (type == "qml") {
        return AppType::AppType_Booster;
    }

    return AppType::AppType_None;
}

std::string AppDescription::toString(AppLocation location)
{
    std::string str;
    switch (location) {
    case AppLocation::AppLocation_Devmode:
        str = "web";
        break;

    case AppLocation::AppLocation_AppStore_Internal:
        str = "store_internal";
        break;

    case AppLocation::AppLocation_AppStore_External:
        str = "store_external";
        break;

    case AppLocation::AppLocation_System_ReadWrite:
        str = "system_updatable";
        break;

    case AppLocation::AppLocation_System_ReadOnly:
        str = "system_builtin";
        break;

    default:
        str = "unknown";
        break;
    }
    return str;
}

AppLocation AppDescription::toAppLocation(const string& type)
{
    if (type == "store" || type == "store_internal") {
        return AppLocation::AppLocation_AppStore_Internal;
    } else if (type == "store_external") { // TODO
        return AppLocation::AppLocation_AppStore_External;
    } else if (type == "system_updatable") {
        return AppLocation::AppLocation_System_ReadWrite;
    } else if (type == "system_builtin") {
        return AppLocation::AppLocation_System_ReadOnly;
    } else if (type == "dev") {
        return AppLocation::AppLocation_Devmode;
    } else {
        return AppLocation::AppLocation_None;
    }
}

AppDescription::AppDescription(const std::string& appId)
    : m_appLocation(AppLocation::AppLocation_None),
      m_folderPath(""),
      m_appType(AppType::AppType_None),
      m_handlerType(LifeHandlerType::LifeHandlerType_None),
      m_appId(appId),
      m_version("1.0.0"),
      m_intVersion(1, 0, 0),
      m_absMain(""),
      m_absSplashBackground(""),
      m_isLocked(false)
{
}

AppDescription::~AppDescription()
{
}

void AppDescription::scan()
{
//    // rescan file system
//    AppDescriptionPtr newAppDesc = AppDescriptionList::getInstance().create(appId);
//    if (!newAppDesc) {
//        notifyOneAppChange(appDesc, "removed", event);
//        AppDescriptionList::getInstance().remove(appDesc);
//        RunningAppList::getInstance().removeByAppId(appId);
//        return;
//    }
//
//    // compare current app and rescanned app
//    // if same
//    if (AppDescriptionList::compare(appDesc, newAppDesc)) {
//        // (still remains in file system)
//        return;
//    }
//
//    // if not same
//    AppDescriptionList::getInstance().add(newAppDesc, true);
//    notifyOneAppChange(newAppDesc, "updated", event);
//    Logger::info(getClassName(), __FUNCTION__, appId, Logger::format("clear_memory: event: %d", event));
//
//
//    Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "Failed to install");
//    AppDescriptionManager::getInstance().reloadApp(appId);
//
//    oldAppDesc = AppDescriptionList::getInstance().getById(appId);
//    if (!oldAppDesc) {
//        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "reload: no current description, just skip");
//        break;
//    }
//
//
//
//
//    oldAppDesc->lock();
//    newAppDesc = AppDescriptionList::getInstance().create(appId);
//    if (!newAppDesc) {
//        AppDescriptionManager::getInstance().notifyOneAppChange(oldAppDesc, "removed", AppStatusEvent::AppStatusEvent_Uninstalled);
//        AppDescriptionList::getInstance().remove(oldAppDesc);
//        RunningAppList::getInstance().removeByAppId(appId);
//        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "reload: no app package left, just remove");
//        break;
//    }
//
//    if (AppDescriptionList::compare(oldAppDesc, newAppDesc)) {
//        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "reload: no change, just skip");
//        break;
//    } else {
//        AppDescriptionList::getInstance().add(newAppDesc, true);
//        AppDescriptionManager::getInstance().notifyOneAppChange(newAppDesc, "updated", AppStatusEvent::AppStatusEvent_UpdateCompleted);
//        Logger::info(getInstance().getClassName(), __FUNCTION__, appId, "reload: different package detected, update info now");
//    }
}

void AppDescription::applyFolderPath(std::string& path)
{
    if (path.compare(0, 7, "file://") == 0)
        path.erase(0, 7);

    if (path.empty() || path[0] == '/')
        return;

    if (m_folderPath.empty())
        return;

    path = File::join(m_folderPath, path);
}

bool AppDescription::loadAppinfo()
{
    if (!File::isDirectory(m_folderPath))
        return false;

    // Specify application description depending on available locale string.
    // This string is in BCP-47 format which can contain a language, language-region,
    // or language-region-script. When SAM loads the appinfo.json file,
    // it should look in resources/<language>/appinfo.json,
    // resources/<language>/<region>/appinfo.json,
    // or resources/<language>/<script>/<region>/appinfo.json respectively.
    // (Note that the script dir goes in between the language and region dirs.)

    const string appinfoPath = File::join(m_folderPath, "/appinfo.json");
    m_appinfo = JDomParser::fromFile(appinfoPath.c_str(), JValueUtil::getSchema("ApplicationDescription"));
    if (m_appinfo.isNull()) {
        Logger::info(CLASS_NAME, __FUNCTION__, m_appId, Logger::format("Failed to parse appinfo.json(%s)", appinfoPath.c_str()));
        m_appinfo = pbnjson::JValue();
        return false;
    }

    /// Add folderPath to JSON
    m_appinfo.put("folderPath", m_folderPath);

    std::vector<std::string> localizationDirs;
    std::string resourcePath = m_folderPath + "/resources/" + SAMConf::getInstance().getLanguage() + "/";
    localizationDirs.push_back(resourcePath);

    resourcePath += SAMConf::getInstance().getScript() + "/";
    localizationDirs.push_back(resourcePath);

    resourcePath += SAMConf::getInstance().getRegion() + "/";
    localizationDirs.push_back(resourcePath);

    // apply localization (overwrite from low to high)
    for (const auto& localizationDir : localizationDirs) {
        std::string AbsoluteLocaleAppinfoPath = localizationDir + "appinfo.json";
        std::string RelativeLocaleAppinfoPath = localizationDir.substr(m_folderPath.length());

        if (!File::isFile(AbsoluteLocaleAppinfoPath)) {
            continue;
        }

        pbnjson::JValue localeAppinfo = JDomParser::fromFile(AbsoluteLocaleAppinfoPath.c_str());
        if (localeAppinfo.isNull()) {
            Logger::info(CLASS_NAME, __FUNCTION__, "IGNORRED", Logger::format("failed_to_load_localication: %s", localizationDir.c_str()));
            continue;
        }

        for (auto item : localeAppinfo.children()) {
            std::string key = item.first.asString();

            if (!m_appinfo.hasKey(key) || m_appinfo[key].getType() != localeAppinfo[key].getType()) {
                Logger::warning(CLASS_NAME, __FUNCTION__, m_appId, AbsoluteLocaleAppinfoPath, "localization is unmatchted with root");
                continue;
            }

            if (m_appinfo[key] == localeAppinfo[key]) {
                continue;
            }

            if (std::find(PROPS_PROHIBITED.begin(), PROPS_PROHIBITED.end(), key) != PROPS_PROHIBITED.end()) {
                Logger::warning(CLASS_NAME, __FUNCTION__, m_appId, AbsoluteLocaleAppinfoPath, "localization is prohibited_props");
                continue;
            }

            if (std::find(ASSETS_SUPPORTED.begin(), ASSETS_SUPPORTED.end(), key) != ASSETS_SUPPORTED.end()) {
                // check asset variation rule with root value
                std::string baseAssetValue = m_appinfo[key].asString();
                std::string localeAssetValue = localeAppinfo[key].asString();

                // if assets variation rule is specified, dont support localization
                if (baseAssetValue.length() > 0 && baseAssetValue[0] == '$')
                    continue;
                if (localeAssetValue.length() > 0 && localeAssetValue[0] == '$')
                    continue;
            }

            auto it = std::find(PROPS_IMAGES.begin(), PROPS_IMAGES.end(), key);
            if (it != PROPS_IMAGES.end()) {
                m_appinfo.put(key, RelativeLocaleAppinfoPath + localeAppinfo[key].asString());
            } else {
                m_appinfo.put(key, localeAppinfo[key]);
            }
        }
    }
    readAppinfo();
    readAsset();
    return true;
}

pbnjson::JValue AppDescription::getJson(JValue& properties)
{
    if (properties.isNull() || !properties.isArray()) {
        return pbnjson::Object();
    }

    if (properties.arraySize() == 0)
        return m_appinfo;

    pbnjson::JValue result = pbnjson::Object();
    pbnjson::JValue notSpecified = pbnjson::Array();

    // get selected props from appinfo
    for (int i = 0; i < properties.arraySize(); ++i) {
        std::string property = "";
        if (!properties[i].isString() || properties[i].asString(property) != CONV_OK)
            continue;
        if (m_appinfo.hasKey(property))
            result.put(property, m_appinfo[property]);
        else
            JValueUtil::addUniqueItemToArray(notSpecified, property);
    }

    if (notSpecified.arraySize() > 0)
        result.put("notSpecified", notSpecified);

    return result;
}

bool AppDescription::readAppinfo()
{
    if (m_appinfo.isNull()) {
        return false;
    }

    if ((!m_appinfo.hasKey("folderPath")) ||
        (!m_appinfo.hasKey("id")) ||
        (!m_appinfo.hasKey("main")) ||
        (!m_appinfo.hasKey("title"))) {
        return false;
    }

    // appId
    if (m_appId != m_appinfo["id"].asString()) {
        return false;
    }
    if (AppLocation::AppLocation_System_ReadWrite == m_appLocation && !isPrivilegedAppId()) {
        return false;
    }
    if (AppLocation::AppLocation_Devmode == m_appLocation && isPrivilegedAppId()) {
        return false;
    }

    // entry_point
    JValueUtil::getValue(m_appinfo, "main", m_absMain);
    if (!strstr(m_absMain.c_str(), "://"))
        m_absMain = std::string("file://") + m_folderPath + std::string("/") + m_absMain;

    // splash_background
    JValueUtil::getValue(m_appinfo, "splashBackground", m_absSplashBackground);
    if (!strstr(m_absSplashBackground.c_str(), "://"))
        m_absSplashBackground = std::string("file://") + m_folderPath + std::string("/") + m_absSplashBackground;

    // version
    m_version = m_appinfo["version"].asString();
    std::vector<std::string> versionInfo;
    boost::split(versionInfo, m_version, boost::is_any_of("."));
    uint16_t major_ver = versionInfo.size() > 0 ? (uint16_t) std::stoi(versionInfo[0]) : 0;
    uint16_t minor_ver = versionInfo.size() > 1 ? (uint16_t) std::stoi(versionInfo[1]) : 0;
    uint16_t micro_ver = versionInfo.size() > 2 ? (uint16_t) std::stoi(versionInfo[2]) : 0;
    m_intVersion = { major_ver, minor_ver, micro_ver };

    // app_type
    bool privilegedJail = m_appinfo.hasKey("privilegedJail") ? m_appinfo["privilegedJail"].asBool() : false;
    m_appType = toAppType(m_appinfo["type"].asString());

    if (m_appType == AppType::AppType_Native && privilegedJail)
        m_appType = AppType::AppType_Native_Mvpd;

    if (m_appType == AppType::AppType_Booster) {
        if (!SAMConf::getInstance().isSupportQmlBooster())
            m_appType = AppType::AppType_Native_Qml;
        else if (!isSystemApp() || !isSystemApp())
            m_appType = AppType::AppType_Native_Qml;
    }

    // handler_type
    switch (m_appType) {
    case AppType::AppType_Web:
        m_handlerType = LifeHandlerType::LifeHandlerType_Web;
        break;

    case AppType::AppType_Booster:
        m_handlerType = LifeHandlerType::LifeHandlerType_Booster;
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

    if (isSystemApp())
        m_appinfo.put("systemApp", true);
    else
        m_appinfo.put("systemApp", false);

    if (AppLocation::AppLocation_Devmode == m_appLocation) {
        m_appinfo.put("inspectable", true);
    }

    return true;
}

void AppDescription::readAsset()
{
    std::string sysAssetsBasePath = "sys-assets";

    JValueUtil::getValue(m_appinfo, "sysAssetsBasePath", sysAssetsBasePath);

    for (const auto& key : ASSETS_SUPPORTED) {
        std::string value;
        std::string variant_path;

        if (!JValueUtil::getValue(m_appinfo, key, value) || value.empty()) {
            continue;
        }

        // check if it requests assets control finding symbol '$'
        if (value.length() < 2 || value[0] != '$') {
            continue;
        }

        std::string filename = value.substr(1);
        bool foundAsset = false;

        JValue fallbacks = SAMConf::getInstance().getSysAssetFallbackPrecedence();
        for (int i = 0; i < fallbacks.arraySize(); i++) {
            std::string assetPath = File::join(File::join(sysAssetsBasePath, fallbacks[i].asString()), filename);
            std::string pathToCheck = "";

            // set asset without variant
            pathToCheck = m_folderPath + std::string("/") + assetPath;
            Logger::debug(CLASS_NAME, __FUNCTION__, Logger::format("patch_to_check: %s\n", pathToCheck.c_str()));

            if (0 == access(pathToCheck.c_str(), F_OK)) {
                m_appinfo.put(key, assetPath);
                foundAsset = true;
                break;
            }
        }

        if (foundAsset) {
            continue;
        }

        string defaultAsset = File::join(sysAssetsBasePath, filename);
        m_appinfo.put(key, defaultAsset);
    }
}

