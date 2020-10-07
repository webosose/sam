// Copyright (c) 2012-2020 LG Electronics, Inc.
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

#include "SAMConf.h"

#include "RuntimeInfo.h"

SAMConf::SAMConf()
    : m_isRespawned(false),
      m_isDevmodeEnabled(false),
      m_isJailerDisabled(true)
{
    setClassName("Settings");
}

SAMConf::~SAMConf()
{
}

void SAMConf::initialize()
{
    loadReadOnlyConf();
    loadReadWriteConf();
    loadBlockedList();

    // TODO this should be moved in RuntimeInfo
    m_isRespawned = File::isFile(this->getRespawnedPath());

    if (File::isFile(this->getDevModePath()))
        m_isDevmodeEnabled = true;

    if (File::isFile(this->getJailModePath()))
        m_isJailerDisabled = true;

    if (!m_isRespawned) {
        if (!File::createFile(this->getRespawnedPath())) {
            Logger::info(getClassName(), __FUNCTION__, "Failed to create respawned file");
        }
    }

    Logger::info(getClassName(), __FUNCTION__,
                 Logger::format("isDevmodeEnabled(%s) isRespawned(%s) isJailerDisabled(%s)",
                 Logger::toString(m_isDevmodeEnabled), Logger::toString(m_isRespawned), Logger::toString(m_isJailerDisabled)));
}

void SAMConf::loadReadOnlyConf()
{
    m_readOnlyDatabase = JDomParser::fromFile(PATH_RO_SAM_CONF, JValueUtil::getSchema("sam-conf"));
    if (m_readOnlyDatabase.isNull()) {
        Logger::warning(getClassName(), __FUNCTION__, PATH_RO_SAM_CONF, "Failed to parse read-only sam-conf");
    }
}

void SAMConf::loadReadWriteConf()
{
    string path = "";

    if (!RuntimeInfo::getInstance().getHome().empty()) {
        path = RuntimeInfo::getInstance().getHome() + "/.config";
        File::makeDirectory(path);
        path += "/sam-conf.json";
    } else {
        path = PATH_RW_SAM_CONF;
    }
    m_readWriteDatabase = JDomParser::fromFile(path.c_str());
    if (m_readWriteDatabase.isNull()) {
        m_readWriteDatabase = pbnjson::Object();
        saveReadWriteConf();
    }
}

void SAMConf::saveReadWriteConf()
{
    string path = "";

    if (!RuntimeInfo::getInstance().getHome().empty()) {
        path = RuntimeInfo::getInstance().getHome() + "/.config/sam-conf.json";
    } else {
        path = PATH_RW_SAM_CONF;
    }

    if (!File::writeFile(path, m_readWriteDatabase.stringify("    ").c_str())) {
        Logger::warning(getClassName(), __FUNCTION__, PATH_RO_SAM_CONF, "Failed to save read-write sam-conf");
    }
}

void SAMConf::loadBlockedList()
{
    m_blockedListDatabase = JDomParser::fromFile(PATH_BLOCKED_LIST);
    if (m_blockedListDatabase.isNull()) {
        Logger::warning(getClassName(), __FUNCTION__, PATH_RO_SAM_CONF, "Failed to parse blocked-file sam-conf");
    }
}
