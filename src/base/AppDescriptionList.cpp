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

#include "setting/Settings.h"
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

AppDescriptionPtr AppDescriptionList::create(const std::string& appId)
{
    if (appId.empty()) {
        Logger::warning(getClassName(), __FUNCTION__, "AppId is empty");
        return nullptr;
    }

    AppDescriptionPtr appDesc = std::make_shared<AppDescription>(appId);
    return appDesc;
}

AppDescriptionPtr AppDescriptionList::add(AppDescriptionPtr appDesc)
{
    if (appDesc == nullptr)
        return nullptr;

    if (!appDesc->isAllowedAppId()) {
        Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId(),
                     Logger::format("Disallowed: AppId(%s) / AppDir(%s)", appDesc->getAppId().c_str(), appDesc->getFolderPath().c_str()));
        return nullptr;
    }

    if (AppLocation::AppLocation_Devmode == appDesc->getAppLocation() && appDesc->isPrivilegedAppId()) {
        Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId(),
                     Logger::format("Disallowed: Privileged appId(%s) is not allowed", appDesc->getAppId().c_str()));
        return nullptr;
    }

    if (m_map.find(appDesc->getAppId()) == m_map.end() || compareVersion(m_map[appDesc->getAppId()], appDesc)) {
        Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId(),
                     Logger::format("Added: AppDir(%s)", appDesc->getFolderPath().c_str()));
        m_map[appDesc->getAppId()] = appDesc;
        LaunchPointPtr launchPoint = LaunchPointList::getInstance().createDefault(appDesc);
        LaunchPointList::getInstance().add(launchPoint);
    } else {
        Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId(),
                     Logger::format("Ignored: AppDir(%s)", appDesc->getFolderPath().c_str()));
        return nullptr;
    }
    return m_map[appDesc->getAppId()];
}

AppDescriptionPtr AppDescriptionList::getById(const std::string& appId)
{
    if (m_map.count(appId) == 0)
        return NULL;
    return m_map[appId];
}

void AppDescriptionList::remove(AppDescriptionPtr appDesc)
{
    // 실제로 앱이 없어지더라도 하위 레이어에 있을수도 있다.
    // 상위 레이어의 앱이 사라지더라도 하위 레이어를 스캐닝해야 한다. 이때는 remove가 아닌 update가 발생해야 한다.
    // 아래 명령으로 appinfo를 찾아서 있으면 다시 로드하는 형태로 처리하자.
    // findAppDir(const string& appId);
    if (appDesc->isSystemApp()) {
        Logger::info(getClassName(), __FUNCTION__, appDesc->getAppId(), "remove system-app in read-write area");
        SettingsImpl::getInstance().appendDeletedSystemApp(appDesc->getAppId());
    }
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second == appDesc) {
            m_map.erase(it);
            return;
        }
    }
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

string AppDescriptionList::findAppDir(const string& appId)
{
    const map<std::string, AppLocation>& appDirs = Settings::getInstance().getAppDirs();
    string result = "";

    for (auto& appDir : appDirs) {
        if (File::isDirectory(appDir.first)) {
            continue;
        }
        if (appDir.second == AppLocation::AppLocation_Devmode && !SettingsImpl::getInstance().isDevmodeEnabled()) {
            continue;
        }

        dirent** entries = NULL;
        int appCount = ::scandir(appDir.first.c_str(), &entries, 0, alphasort);
        if (entries == NULL || appCount == 0) {
            continue;
        }

        for (int i = 0; i < appCount; ++i) {
            if (!entries[i] || entries[i]->d_name[0] == '.')
                continue;
            if (appDir.second == AppLocation::AppLocation_System_ReadOnly && SettingsImpl::getInstance().isDeletedSystemApp(entries[i]->d_name)) {
                continue;
            }
            if (appId == entries[i]->d_name) {
                string tmp = File::join(appDir.first, entries[i]->d_name);
                if (File::isDirectory(tmp)) {
                    result = tmp;
                }
            }
        }

        for (int idx = 0; idx < appCount; ++idx) {
            if (entries[idx])
                free(entries[idx]);
        }
        free(entries);
        entries = NULL;
    }
    return result;
}
