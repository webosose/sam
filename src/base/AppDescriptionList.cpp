// Copyright (c) 2019 LG Electronics, Inc.
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

#include "base/AppDescriptionList.h"
#include "base/LaunchPointList.h"
#include "bus/service/ApplicationManager.h"
#include "conf/SAMConf.h"
#include "util/File.h"

bool AppDescriptionList::compare(AppDescriptionPtr me, AppDescriptionPtr another)
{
    if (me->m_appId != another->m_appId)
        return false;
    if (me->m_folderPath != another->m_folderPath)
        return false;
    if (me->m_version != another->m_version)
        return false;
    return true;
}

bool AppDescriptionList::compareVersion(AppDescriptionPtr me, AppDescriptionPtr another)
{
    // Non dev app has always higher priority than dev apps
    if (AppLocation::AppLocation_Devmode != me->getAppLocation() && AppLocation::AppLocation_Devmode == another->getAppLocation()) {
        return false;
    }

    const AppIntVersion& me_ver = me->getIntVersion();
    const AppIntVersion& new_ver = another->getIntVersion();

    // compare version
    if (get<0>(me_ver) < get<0>(new_ver))
        return true;
    else if (get<0>(me_ver) > get<0>(new_ver))
        return false;
    if (get<1>(me_ver) < get<1>(new_ver))
        return true;
    else if (get<1>(me_ver) > get<1>(new_ver))
        return false;
    if (get<2>(me_ver) < get<2>(new_ver))
        return true;
    else if (get<2>(me_ver) > get<2>(new_ver))
        return false;

    // if same version, check type_by_dir priority
    if ((int) me->getAppLocation() > (int) me->getAppLocation())
        return true;

    return false;
}

AppDescriptionList::AppDescriptionList()
{
    setClassName("AppDescriptionList");
}

AppDescriptionList::~AppDescriptionList()
{
}

void AppDescriptionList::changeLocale()
{
    for (auto appDesc : m_map) {
        appDesc.second->loadAppinfo();
    }
}

void AppDescriptionList::scanFull()
{
    JValue applicationPaths = SAMConf::getInstance().getApplicationPaths();
    for (int i = 0; i < applicationPaths.arraySize(); i++) {
        string path = "";
        string typeByDir = "";
        AppLocation appLocation;

        JValueUtil::getValue(applicationPaths[i], "path", path);
        JValueUtil::getValue(applicationPaths[i], "typeByDir", typeByDir);
        appLocation = AppDescription::toAppLocation(typeByDir);

        if (path.empty() || typeByDir.empty() || appLocation == AppLocation::AppLocation_None) {
            Logger::warning(getClassName(), __FUNCTION__,
                            Logger::format("Invalid appLocation type: path(%s) typeByDir(%s)", path.c_str(), typeByDir.c_str()));
            continue;
        }
        if (!File::isDirectory(path)) {
            Logger::warning(getClassName(), __FUNCTION__,
                            Logger::format("Directory is not exist: path(%s) typeByDir(%s)", path.c_str(), typeByDir.c_str()));
            continue;
        }
        if (appLocation == AppLocation::AppLocation_Devmode && !SAMConf::getInstance().isDevmodeEnabled()) {
            Logger::debug(getClassName(), __FUNCTION__,
                          Logger::format("Devmode directory is skipped: path(%s) typeByDir(%s)", path.c_str(), typeByDir.c_str()));
            continue;
        }
        scanDir(path, appLocation);
    }
    return;
}

void AppDescriptionList::scanDir(const string& path, const AppLocation& appLocation)
{
    dirent** entries = NULL;
    int appCount = ::scandir(path.c_str(), &entries, 0, alphasort);
    if (entries == NULL || appCount == 0) {
        Logger::warning(getClassName(), __FUNCTION__, "Failed to call scandir",
                        Logger::format("path(%s) appLocation(%s)", path.c_str(), AppDescription::toString(appLocation)));
        goto Done;
    }

    for (int i = 0; i < appCount; ++i) {
        if (!entries[i] || entries[i]->d_name[0] == '.') {
            continue;
        }

        string folderPath = File::join(path, entries[i]->d_name);
        if (appLocation == AppLocation::AppLocation_System_ReadOnly &&
            SAMConf::getInstance().isDeletedSystemApp(entries[i]->d_name)) {
            Logger::info(getClassName(), __FUNCTION__, "The app is deleted. Skipped",
                         Logger::format("forderPath(%s)", folderPath.c_str()));
            continue;
        }
        scanApp(entries[i]->d_name, folderPath, appLocation);
    }

Done:
    for (int i = 0; i < appCount; ++i) {
        if (entries[i])
            free(entries[i]);
    }
    free(entries);
    entries = NULL;
    return;
}

void AppDescriptionList::scanApp(const string& appId, const string& folderPath, const AppLocation& appLocation)
{
    if (!File::isDirectory(folderPath)) {
        Logger::warning(getClassName(), __FUNCTION__, appId, "The folderPath is not exist");
        return;
    }
    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().create(appId);
    if (!appDesc) {
        Logger::warning(getClassName(), __FUNCTION__, appId, "Cannot create application description");
        return;
    }
    if (!appDesc->scan(folderPath, appLocation)) {
        Logger::warning(getClassName(), __FUNCTION__, appId, "Cannot scan AppDescription");
        return;
    }
    AppDescriptionList::getInstance().add(appDesc);
}

AppDescriptionPtr AppDescriptionList::create(const string& appId)
{
    if (appId.empty()) {
        Logger::warning(getClassName(), __FUNCTION__, "AppId is empty");
        return nullptr;
    }

    AppDescriptionPtr appDesc = make_shared<AppDescription>(appId);
    return appDesc;
}

AppDescriptionPtr AppDescriptionList::getByAppId(const string& appId)
{
    if (m_map.count(appId) == 0)
        return NULL;
    return m_map[appId];
}

bool AppDescriptionList::add(AppDescriptionPtr appDesc)
{
    if (appDesc == nullptr) {
        Logger::error(getClassName(), __FUNCTION__, "Invalid AppDescription");
        return false;
    }

    if (!appDesc->isAllowedAppId()) {
        Logger::warning(getClassName(), __FUNCTION__, appDesc->getAppId(), "Ignored: Appid is not allowed");
        return false;
    }

    if (AppLocation::AppLocation_Devmode == appDesc->getAppLocation() && appDesc->isPrivilegedAppId()) {
        Logger::warning(getClassName(), __FUNCTION__, appDesc->getAppId(), "Ignored: Reserved AppId is not allowed for 3rd party app");
        return false;
    }

    if (appDesc->m_appinfo.isNull() || !appDesc->m_appinfo.isValid()) {
        Logger::warning(getClassName(), __FUNCTION__, appDesc->getAppId(), "Ignored: No valid appinfo.json");
        return false;
    }

    if (m_map.find(appDesc->getAppId()) != m_map.end() && !compareVersion(m_map[appDesc->getAppId()], appDesc)) {
        Logger::warning(getClassName(), __FUNCTION__, appDesc->getAppId(), "Ignored: Version is low");
        return false;
    }

    Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId());
    m_map[appDesc->getAppId()] = appDesc;
    ApplicationManager::getInstance().postListApps(appDesc, "added", "");
    LaunchPointPtr launchPoint = LaunchPointList::getInstance().createDefault(appDesc);
    LaunchPointList::getInstance().add(launchPoint);
    return true;
}

bool AppDescriptionList::remove(AppDescriptionPtr appDesc)
{
    if (appDesc->isSystemApp()) {
        Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId(), "remove system-app in read-write area");
        SAMConf::getInstance().appendDeletedSystemApp(appDesc->getAppId());
    }
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second == appDesc) {
            LaunchPointList::getInstance().removeByAppDesc(appDesc);
            m_map.erase(it);
            Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId());
            ApplicationManager::getInstance().postListApps(appDesc, "removed", "");
            return true;
        }
    }
    return false;
}

bool AppDescriptionList::isExist(const string& appId)
{
    if (m_map.count(appId) == 0)
        return false;
    return true;
}

void AppDescriptionList::toJson(JValue& json, JValue& properties, bool devmode)
{
    if (!json.isArray())
        return;

    for (auto appDesc : m_map) {
        if (devmode && appDesc.second->getAppLocation() != AppLocation::AppLocation_Devmode) continue;

        JValue item;
        if (properties.isArray() && properties.arraySize() > 0)
            item = appDesc.second->getJson(properties);
        else
            item = appDesc.second->getJson();
        json.append(item);
    }
}
