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

#include "base/RunningAppList.h"

#include "bus/service/ApplicationManager.h"

RunningAppList::RunningAppList()
{
    setClassName("RunningAppList");
}

RunningAppList::~RunningAppList()
{
}

RunningAppPtr RunningAppList::createByLunaTask(LunaTaskPtr lunaTask)
{
    if (lunaTask == nullptr)
        return nullptr;
    string appId = lunaTask->getAppId();
    string launchPointId = lunaTask->getLaunchPointId();

    if (!lunaTask->getLaunchPointId().empty()) {
        return createByLaunchPointId(lunaTask->getLaunchPointId());
    } else if (!lunaTask->getAppId().empty()) {
        return createByAppId(lunaTask->getAppId());
    } else {
        return nullptr;
    }
}

RunningAppPtr RunningAppList::createByAppId(const string& appId)
{
    string launchPointId = appId + "_default";
    return createByLaunchPointId(launchPointId);
}

RunningAppPtr RunningAppList::createByLaunchPointId(const string& launchPointId)
{
    LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByLaunchPointId(launchPointId);
    if (launchPoint == nullptr) {
        Logger::warning(getClassName(), __FUNCTION__, "Cannot find proper launchPoint");
        return nullptr;
    }
    return createByLaunchPoint(launchPoint);
}

RunningAppPtr RunningAppList::createByLaunchPoint(LaunchPointPtr launchPoint)
{
    if (launchPoint == nullptr)
        return nullptr;

    RunningAppPtr runningApp = make_shared<RunningApp>(launchPoint);
    if (runningApp == nullptr) {
        Logger::error(getClassName(), __FUNCTION__, "Failed to create new RunningApp");
        return nullptr;
    }
    return runningApp;
}

RunningAppPtr RunningAppList::getByLunaTask(LunaTaskPtr lunaTask)
{
    if (lunaTask == nullptr)
        return nullptr;
    string appId = lunaTask->getAppId();
    string launchPointId = lunaTask->getLaunchPointId();
    string instanceId = lunaTask->getInstanceId();

    return getByIds(instanceId, launchPointId, appId);
}

RunningAppPtr RunningAppList::getByIds(const string& instanceId, const string& launchPointId, const string& appId)
{
    RunningAppPtr runningApp = nullptr;
    if (!instanceId.empty())
        runningApp = getByInstanceId(instanceId);
    else if (!launchPointId.empty())
        runningApp = getByLaunchPointId(launchPointId);
    else if (!appId.empty())
        runningApp = getByAppId(appId);

    if (runningApp == nullptr)
        return nullptr;

    if (!instanceId.empty() && instanceId != runningApp->getInstanceId())
        return nullptr;
    if (!launchPointId.empty() && launchPointId != runningApp->getLaunchPointId())
            return nullptr;
    if (!appId.empty() && appId != runningApp->getAppId())
            return nullptr;
    return runningApp;
}

RunningAppPtr RunningAppList::getByInstanceId(const string& instanceId)
{
    if (instanceId.empty())
        return nullptr;
    if (m_map.find(instanceId) == m_map.end()) {
        return nullptr;
    }
    return m_map[instanceId];
}

RunningAppPtr RunningAppList::getByToken(const LSMessageToken& token)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getToken() == token) {
            return it->second;
        }
    }
    return nullptr;
}

RunningAppPtr RunningAppList::getByLaunchPointId(const string& launchPointId)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getLaunchPointId() == launchPointId) {
            return it->second;
        }
    }
    return nullptr;
}

RunningAppPtr RunningAppList::getByAppIdAndDisplayId(const string& appId, const int displayId)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getAppId() == appId && (*it).second->getDisplayId() == displayId) {
            return it->second;
        }
    }
    return nullptr;
}

RunningAppPtr RunningAppList::getByAppId(const string& appId)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getAppId() == appId) {
            return it->second;
        }
    }
    return nullptr;
}

RunningAppPtr RunningAppList::getByPid(const string& pid)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getProcessId() == pid) {
            return it->second;
        }
    }
    return nullptr;
}

RunningAppPtr RunningAppList::getByWebprocessid(const string& webprocessid)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getWebprocessid() == webprocessid) {
            return it->second;
        }
    }
    return nullptr;
}

bool RunningAppList::add(RunningAppPtr runningApp)
{
    if (runningApp == nullptr) {
        return false;
    }
    if (runningApp->getInstanceId().empty()) {
        return false;
    }
    if (m_map.find(runningApp->getInstanceId()) != m_map.end()) {
        Logger::info(getClassName(), __FUNCTION__, runningApp->getInstanceId(), "InstanceId is already exist");
        return false;
    }
    m_map[runningApp->getInstanceId()] = runningApp;
    onAdd(runningApp);
    return true;
}

bool RunningAppList::removeByObject(RunningAppPtr runningApp)
{
    if (runningApp == nullptr)
        return false;

    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second == runningApp) {
            RunningAppPtr runningApp = it->second;
            m_map.erase(it);
            onRemove(runningApp);
            return true;
        }
    }
    return false;
}

bool RunningAppList::removeByIds(const string& instanceId, const string& launchPointId, const string& appId)
{
    RunningAppPtr runningApp = nullptr;
    if (!instanceId.empty())
        runningApp = getByInstanceId(instanceId);
    else if (!launchPointId.empty())
        runningApp = getByLaunchPointId(launchPointId);
    else if (!appId.empty())
        runningApp = getByAppId(appId);

    if (runningApp == nullptr)
        return false;

    if (!instanceId.empty() && instanceId != runningApp->getInstanceId())
        return false;
    if (!launchPointId.empty() && launchPointId != runningApp->getLaunchPointId())
        return false;
    if (!appId.empty() && appId != runningApp->getAppId())
        return false;
    return removeByObject(runningApp);
}

bool RunningAppList::removeByInstanceId(const string& instanceId)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getInstanceId() == instanceId) {
            RunningAppPtr runningApp = it->second;
            m_map.erase(it);
            onRemove(runningApp);
            return true;
        }
    }
    return false;
}

bool RunningAppList::removeByPid(const string& processId)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getProcessId() == processId) {
            RunningAppPtr runningApp = it->second;
            m_map.erase(it);
            onRemove(runningApp);
            return true;
        }
    }
    return false;
}

bool RunningAppList::removeAllByType(AppType type)
{
    for (auto it = m_map.cbegin(); it != m_map.cend() ;) {
        if (it->second->getLaunchPoint()->getAppDesc()->getAppType() == type) {
            RunningAppPtr runningApp = it->second;
            it = m_map.erase(it);
            onRemove(runningApp);
        } else {
            ++it;
        }
    }
    return true;
}

bool RunningAppList::removeAllByConext(AppType type, const int context)
{
    for (auto it = m_map.cbegin(); it != m_map.cend() ;) {
        if (it->second->getLaunchPoint()->getAppDesc()->getAppType() == type && it->second->getContext() == context) {
            RunningAppPtr runningApp = it->second;
            it = m_map.erase(it);
            onRemove(runningApp);
        } else {
            ++it;
        }
    }
    return true;
}

bool RunningAppList::removeAllByLaunchPoint(LaunchPointPtr launchPoint)
{
    for (auto it = m_map.cbegin(); it != m_map.cend() ;) {
        if (it->second->getLaunchPoint() == launchPoint) {
            RunningAppPtr runningApp = it->second;
            it = m_map.erase(it);
            onRemove(runningApp);
        } else {
            ++it;
        }
    }
    return true;
}

bool RunningAppList::setConext(AppType type, const int context)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if (it->second->getLaunchPoint()->getAppDesc()->getAppType() == type) {
            it->second->setContext(context);
        }
    }
    return true;
}

void RunningAppList::toJson(JValue& array, bool devmodeOnly)
{
    if (!array.isArray())
        return;

    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if (devmodeOnly && AppLocation::AppLocation_Devmode != it->second->getLaunchPoint()->getAppDesc()->getAppLocation()) {
             continue;
         }

         pbnjson::JValue object = pbnjson::Object();
         it->second->toJson(object, false);
         array.append(object);
    }
}

void RunningAppList::onAdd(RunningAppPtr runningApp)
{
    // Status should be defined before calling this method
    Logger::info(getClassName(), __FUNCTION__, runningApp->getInstanceId() + " is added");
    ApplicationManager::getInstance().postRunning(runningApp);
}

void RunningAppList::onRemove(RunningAppPtr runningApp)
{
    Logger::info(getClassName(), __FUNCTION__, runningApp->getInstanceId() + " is removed");
    runningApp->setLifeStatus(LifeStatus::LifeStatus_STOP);
    ApplicationManager::getInstance().postRunning(runningApp);
}