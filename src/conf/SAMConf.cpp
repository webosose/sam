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

#include "SAMConf.h"

SAMConf::SAMConf()
    : m_isRespawned(false),
      m_isDevmodeEnabled(false),
      m_isJailerDisabled(false)
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

    m_isDevmodeEnabled = File::isFile(this->getDevModePath());
    m_isRespawned = File::isFile(this->getRespawnedPath());
    m_isJailerDisabled = File::isFile(this->getJailModePath());

    if (!m_isRespawned) {
        File::createFile(this->getRespawnedPath());
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
    m_readWriteDatabase = JDomParser::fromFile(PATH_RW_SAM_CONF);
    if (m_readWriteDatabase.isNull()) {
        m_readWriteDatabase = pbnjson::Object();
        saveReadWriteConf();
    }
}

void SAMConf::saveReadWriteConf()
{
    if (!File::writeFile(PATH_RW_SAM_CONF, m_readWriteDatabase.stringify("    ").c_str())) {
        Logger::warning(getClassName(), __FUNCTION__, PATH_RO_SAM_CONF, "Failed to save read-write sam-conf");
    }
}
