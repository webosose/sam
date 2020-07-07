// Copyright (c) 2020 LG Electronics, Inc.
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

#include "RuntimeInfo.h"

#include <stdlib.h>

RuntimeInfo::RuntimeInfo()
    : m_displayId(-1),
      m_isInContainer(false)
{
    setClassName("RuntimeInfo");
}

RuntimeInfo::~RuntimeInfo()
{
}

void RuntimeInfo::initialize()
{
    char* displayId = getenv("DISPLAY_ID");
    char* deviceType = getenv("DEVICE_TYPE");
    char* container = getenv("container");

    if (displayId != nullptr) {
        m_displayId = stoi(displayId);
    }
    if (deviceType != nullptr) {
        m_deviceType = deviceType;
    }
    if (container != nullptr) {
        m_isInContainer = true;
    }
    Logger::info(getClassName(), __FUNCTION__,
                 Logger::format("DisplayId(%d) DeviceType(%s) IsInContainer(%s)",
                                 m_displayId, m_deviceType.c_str(), Logger::toString(m_isInContainer)));
    load();
}

bool RuntimeInfo::getValue(const string& key, JValue& value)
{
    if (!m_database.hasKey(key))
        return false;
    value = m_database[key];
    return true;
}

bool RuntimeInfo::setValue(const string& key, JValue& value)
{
    if (!m_database.put(key, value.duplicate()))
        return false;
    return save();
}

bool RuntimeInfo::save()
{
    if (!File::writeFile(PATH_RUNTIME_INFO, m_database.stringify("    "))) {
        Logger::warning(getClassName(), __FUNCTION__, PATH_RUNTIME_INFO, "Failed to save RuntimeInfo");
        return false;
    }
    return true;
}

bool RuntimeInfo::load()
{
    m_database = JDomParser::fromFile(PATH_RUNTIME_INFO);
    if (m_database.isNull()) {
        m_database = pbnjson::Object();
        save();
    }
    return true;
}
