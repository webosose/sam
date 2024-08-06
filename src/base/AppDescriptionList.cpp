// Copyright (c) 2019-2020 LG Electronics, Inc.
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
    // Non dev app has always higher priority than dev apps
    if (AppLocation::AppLocation_Devmode != me->getAppLocation() &&
        AppLocation::AppLocation_Devmode == another->getAppLocation()) {
        return false;
    }

    const AppIntVersion& meVersion = me->getIntVersion();
    const AppIntVersion& anotherVersion = another->getIntVersion();

    // compare version
    if (get<0>(meVersion) < get<0>(anotherVersion))
        return true;
    else if (get<0>(meVersion) > get<0>(anotherVersion))
        return false;
    if (get<1>(meVersion) < get<1>(anotherVersion))
        return true;
    else if (get<1>(meVersion) > get<1>(anotherVersion))
        return false;
    if (get<2>(meVersion) < get<2>(anotherVersion))
        return true;
    else if (get<2>(meVersion) > get<2>(anotherVersion))
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
    for (const auto& appDesc : m_map) {
        appDesc.second->scan();
    }
}

void AppDescriptionList::scanApp(const string& appId)
{
    AppDescriptionPtr newAppDesc = AppDescriptionList::getInstance().create(appId);
    if (newAppDesc == nullptr) {
        Logger::warning(getInstance().getClassName(), __FUNCTION__, appId, "Failed to create new AppDescription");
        return;
    }

    JValue applicationPaths = SAMConf::getInstance().getApplicationPaths();
    for (int i = applicationPaths.arraySize() - 1; i >= 0; i--) {
        string path = "";
        string typeByDir = "";

        if (!JValueUtil::getValue(applicationPaths[i], "path", path) ||
            !JValueUtil::getValue(applicationPaths[i], "typeByDir", typeByDir)) {
            continue;
        }

        AppLocation appLocation = AppDescription::toAppLocation(typeByDir);
        if (path.empty() || typeByDir.empty() || appLocation == AppLocation::AppLocation_None) {
            continue;
        }
        if (appLocation == AppLocation::AppLocation_Devmode && !SAMConf::getInstance().isDevmodeEnabled()) {
            continue;
        }

        string folderPath = File::join(path, appId);
        if (!File::isDirectory(folderPath)) {
            Logger::warning(getClassName(), __FUNCTION__, appId, folderPath + " is not exist");
            continue;
        }

        if (newAppDesc->scan(folderPath, appLocation)) {
            break;
        }
    }

    if (!newAppDesc->isScanned()) {
        Logger::warning(getClassName(), __FUNCTION__, appId, "Failed to scan AppDescription");
        AppDescriptionList::getInstance().removeByAppId(appId);
        return;
    }

    AppDescriptionList::getInstance().add(std::move(newAppDesc));
}

void AppDescriptionList::scanFull()
{
    JValue applicationPaths = SAMConf::getInstance().getApplicationPaths();
    for (int i = 0; i < applicationPaths.arraySize(); i++) {
        string path = "";
        string typeByDir = "";

        if (!JValueUtil::getValue(applicationPaths[i], "path", path) ||
            !JValueUtil::getValue(applicationPaths[i], "typeByDir", typeByDir)) {
            Logger::warning(getClassName(), __FUNCTION__,
                            Logger::format("Invalid Configuration: path(%s) typeByDir(%s)", path.c_str(), typeByDir.c_str()));
            continue;
        }

        AppLocation appLocation = AppDescription::toAppLocation(typeByDir);
        if (path.empty() || typeByDir.empty() || appLocation == AppLocation::AppLocation_None) {
            Logger::warning(getClassName(), __FUNCTION__,
                            Logger::format("Invalid Configuration: path(%s) typeByDir(%s)", path.c_str(), typeByDir.c_str()));
            continue;
        }
        if (appLocation == AppLocation::AppLocation_Devmode && !SAMConf::getInstance().isDevmodeEnabled()) {
            Logger::info(getClassName(), __FUNCTION__,
                         Logger::format("Devmode directory is skipped: path(%s) typeByDir(%s)", path.c_str(), typeByDir.c_str()));
            continue;
        }

        if (!File::isDirectory(path)) {
            Logger::warning(getClassName(), __FUNCTION__,
                            Logger::format("Directory is not exist: path(%s) typeByDir(%s)", path.c_str(), typeByDir.c_str()));
            continue;
        }
        scanDir(path, appLocation);
    }
    return;
}

void AppDescriptionList::scanDir(const string& path, const AppLocation& appLocation)
{
    dirent** entries = NULL;
    int entryCount = ::scandir(path.c_str(), &entries, 0, alphasort);
    if (entries == NULL || entryCount == 0) {
        Logger::warning(getClassName(), __FUNCTION__, "Failed to call scandir",
                        Logger::format("path(%s) appLocation(%s)", path.c_str(), AppDescription::toString(appLocation)));
        goto Done;
    }

    for (int i = 0; i < entryCount; ++i) {
        if (!entries[i] || entries[i]->d_name[0] == '.') {
            continue;
        }
        string folderPath = File::join(path, entries[i]->d_name);
        if (SAMConf::getInstance().isBlockedApp(entries[i]->d_name)) {
            Logger::info(getClassName(), __FUNCTION__, "BLOCKED",
                         Logger::format("forderPath(%s)", folderPath.c_str()));
            continue;
        }
        if (appLocation == AppLocation::AppLocation_System_ReadOnly &&
            SAMConf::getInstance().isDeletedSystemApp(entries[i]->d_name)) {
            Logger::info(getClassName(), __FUNCTION__, "DELETED",
                         Logger::format("forderPath(%s)", folderPath.c_str()));
            continue;
        }
        if (!File::isDirectory(folderPath)) {
            Logger::warning(getClassName(), __FUNCTION__, entries[i]->d_name, folderPath + " is not exist");
            continue;
        }

        AppDescriptionPtr appDesc = AppDescriptionList::getInstance().create(entries[i]->d_name);
        if (!appDesc) {
            Logger::warning(getClassName(), __FUNCTION__, entries[i]->d_name, "Cannot create application description");
            continue;
        }
        if (!appDesc->scan(folderPath, appLocation)) {
            Logger::warning(getClassName(), __FUNCTION__, entries[i]->d_name, "Cannot scan AppDescription");
            continue;
        }
        AppDescriptionList::getInstance().add(std::move(appDesc));

    }

Done:
    if (entries != NULL) {
        for (int i = 0; i < entryCount; ++i) {
            if (entries[i])
                free(entries[i]);
        }
        free(entries);
        entries = NULL;
    }
    return;
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

bool AppDescriptionList::add(AppDescriptionPtr newAppDesc)
{
    if (newAppDesc == nullptr) {
        Logger::error(getClassName(), __FUNCTION__, "Invalid AppDescription");
        return false;
    }
    if (!newAppDesc->isScanned()) {
        Logger::warning(getClassName(), __FUNCTION__, newAppDesc->getAppId(), "AppDescription is not scanned");
        return false;
    }

    if (m_map.find(newAppDesc->getAppId()) == m_map.end()) {
        Logger::info(getClassName(), __FUNCTION__, newAppDesc->getAppId() + " is added");
        m_map[newAppDesc->getAppId()] = newAppDesc;
        ApplicationManager::getInstance().postListApps(newAppDesc, "added", "");
        LaunchPointPtr launchPoint = LaunchPointList::getInstance().createDefault(newAppDesc);
        LaunchPointList::getInstance().add(std::move(launchPoint));
        return true;
    }

    if (m_map[newAppDesc->getAppId()]->getFolderPath() == newAppDesc->getFolderPath()) {
        // same directory means *update*
        AppDescriptionPtr oldAppDesc = m_map[newAppDesc->getAppId()];
        m_map[newAppDesc->getAppId()] = newAppDesc;
        ApplicationManager::getInstance().postListApps(newAppDesc, "updated", "");
        LaunchPointList::getInstance().update(std::move(oldAppDesc), newAppDesc);
    } else if (compare(m_map[newAppDesc->getAppId()], newAppDesc) || !m_map[newAppDesc->getAppId()]->scan()) {
        // TODO why second condition is needed?
        // check version of new app description.
        AppDescriptionPtr oldAppDesc = m_map[newAppDesc->getAppId()];
        m_map[newAppDesc->getAppId()] = newAppDesc;
        ApplicationManager::getInstance().postListApps(newAppDesc, "updated", "");
        LaunchPointList::getInstance().update(std::move(oldAppDesc), newAppDesc);
    }
    return true;
}

void AppDescriptionList::removeByAppId(const string& appId)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getAppId() == appId) {
            onRemove((*it).second);
            m_map.erase(it);
            return;
        }
    }
}

void AppDescriptionList::removeByObject(AppDescriptionPtr appDesc)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second == appDesc) {
            onRemove((*it).second);
            m_map.erase(it);
            return;
        }
    }
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

    for (const auto& appDesc : m_map) {
        if (devmode && appDesc.second->getAppLocation() != AppLocation::AppLocation_Devmode) continue;

        JValue item;
        if (properties.isArray() && properties.arraySize() > 0)
            item = appDesc.second->getJson(properties);
        else
            item = appDesc.second->getJson();
        json.append(item);
    }
}

void AppDescriptionList::onRemove(AppDescriptionPtr appDesc)
{
    if (appDesc->isSystemApp()) {
        Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId(), "remove system-app in read-write area");
        SAMConf::getInstance().appendDeletedSystemApp(appDesc->getAppId());
    }
    LaunchPointList::getInstance().removeByAppDesc(appDesc);
    Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId());
    ApplicationManager::getInstance().postListApps(std::move(appDesc), "removed", "");
}
