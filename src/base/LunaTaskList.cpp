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

#include "base/LunaTaskList.h"

#include <string.h>

LunaTaskList::LunaTaskList()
{
}

LunaTaskList::~LunaTaskList()
{
    m_list.clear();
}

LunaTaskPtr LunaTaskList::add(LunaTaskPtr lunaTask)
{
    m_list.push_back(lunaTask);
    return lunaTask;
}

LunaTaskPtr LunaTaskList::getByKindAndId(const char* kind, const string& appId)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if (strcmp((*it)->getRequest().getKind(), kind) == 0 && (*it)->getAppId() == appId)
            return *it;
    }
    return nullptr;
}

LunaTaskPtr LunaTaskList::getByAppId(const string& appId)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->getAppId() == appId)
            return *it;
    }
    return nullptr;
}

LunaTaskPtr LunaTaskList::getByInstanceId(const string& instanceId)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->getInstanceId() == instanceId)
            return *it;
    }
    return nullptr;
}

LunaTaskPtr LunaTaskList::getByToken(const LSMessageToken& token)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->getToken() == token)
            return *it;
    }
    return nullptr;
}

void LunaTaskList::remove(LunaTaskPtr lunaTask)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it) == lunaTask) {
            m_list.erase(it);
            return;
        }
    }
}

void LunaTaskList::toJson(JValue& array)
{
    if (!array.isArray())
        return;

    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        pbnjson::JValue object = pbnjson::Object();
        (*it)->toJson(object);
        array.append(object);
    }
}
