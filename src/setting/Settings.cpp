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

#include "Settings.h"

#include <errno.h>
#include <unistd.h>
#include <string>

#include "util/JValueUtil.h"
#include "util/File.h"

Settings::Settings()
    : m_isRespawned(false),
      m_isDevmodeEnabled(false),
      m_isJailerDisabled(false)
{
    setClassName("Settings");
    SettingsImpl::getInstance().loadReadOnlyConf();
    SettingsImpl::getInstance().loadReadWriteConf();
}

Settings::~Settings()
{
    m_appDirs.clear();
}

bool Settings::loadReadOnlyConf()
{
    std::string path = PATH_SAM_CONF;
    Logger::info(getClassName(), __FUNCTION__, Logger::format("Load conf(%s)", path.c_str()));
    m_readOnlyDatabase = JDomParser::fromFile(path.c_str(), JValueUtil::getSchema("sam-conf"));
    if (m_readOnlyDatabase.isNull()) {
        Logger::warning(getClassName(), __FUNCTION__, path);
        return false;
    }

    if (m_readOnlyDatabase["ApplicationPaths"].isArray()) {
        pbnjson::JValue applicationPaths = m_readOnlyDatabase["ApplicationPaths"];
        int arraySize = applicationPaths.arraySize();

        for (int i = 0; i < arraySize; i++) {
            std::string path, type;
            AppLocation typeByDir;
            if (applicationPaths[i]["typeByDir"].asString(type) != CONV_OK)
                continue;
            if (applicationPaths[i]["path"].asString(path) != CONV_OK)
                continue;

            if ("system_builtin" == type)
                typeByDir = AppLocation::AppLocation_System_ReadOnly;
            else if ("system_updatable" == type)
                typeByDir = AppLocation::AppLocation_System_ReadWrite;
            else if ("store" == type)
                typeByDir = AppLocation::AppLocation_AppStore_Internal;
            else if ("dev" == type) {
                m_devAppsPaths.push_back(path);
                continue;
            }
            else
                continue;

            addAppDir(path, typeByDir);
        }

        if (m_appDirs.size() < 1) {
            Logger::warning(getClassName(), __FUNCTION__, path);
            return false;
        }
    }

    return true;
}

void Settings::loadReadWriteConf()
{
    m_isDevmodeEnabled = File::isFile(this->getDevModePath());
    m_isRespawned = File::isFile(this->getRespawnedPath());
    m_isJailerDisabled = File::isFile(this->getJailModePath());

    if (m_isDevmodeEnabled) {
        for (auto it : m_devAppsPaths) {
            addAppDir(it, AppLocation::AppLocation_Devmode);
        }
    }

    if (!m_isRespawned) {
        File::createFile(this->getRespawnedPath());
    }

    Logger::info(getClassName(), __FUNCTION__, Logger::format("DEVMODE(%s) RESPAWNED(%s)", Logger::toString(m_isDevmodeEnabled), Logger::toString(m_isRespawned)));
}

void Settings::saveReadWriteConf()
{

}

void Settings::addAppDir(std::string path, AppLocation type)
{
    Logger::debug(getClassName(), __FUNCTION__, Logger::format("Add App DIR(%s)", path.c_str()));
    File::trimPath(path);
    m_appDirs[path] = type;
}
