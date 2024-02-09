// Copyright (c) 2019-2021 LG Electronics, Inc.
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
#include "conf/RuntimeInfo.h"

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

    RunningAppPtr runningApp = nullptr;
    if (!lunaTask->getLaunchPointId().empty()) {
        runningApp = createByLaunchPointId(lunaTask->getLaunchPointId());
    } else if (!lunaTask->getAppId().empty()) {
        runningApp = createByAppId(lunaTask->getAppId());
    }

    if (runningApp) {
        runningApp->loadRequestPayload(lunaTask->getRequestPayload());
        runningApp->setInstanceId(lunaTask->getInstanceId());
        runningApp->setDisplayId(lunaTask->getDisplayId());

        lunaTask->setLaunchPointId(runningApp->getLaunchPointId());
        lunaTask->setAppId(runningApp->getAppId());
    }
    return runningApp;
}

RunningAppPtr RunningAppList::createByJson(const JValue& json)
{
    if (json.isNull() || !json.isValid())
        return nullptr;

    string launchPointId;
    string instanceId;
    int processId = -1;
    int displayId = -1;

    if (!JValueUtil::getValue(json, "launchPointId", launchPointId) ||
        !JValueUtil::getValue(json, "instanceId", instanceId) ||
        !JValueUtil::getValue(json, "processId", processId) ||
        !JValueUtil::getValue(json, "displayId", displayId)) {
        return nullptr;
    }

    RunningAppPtr runningApp = createByLaunchPointId(launchPointId);
    if (runningApp == nullptr)
        return nullptr;

    runningApp->setInstanceId(instanceId);
    runningApp->setProcessId(processId);
    runningApp->setDisplayId(displayId);
    return runningApp;
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
        Logger::warning(getClassName(), __FUNCTION__, "Cannot find launchPoint");
        return nullptr;
    }
    RunningAppPtr runningApp = make_shared<RunningApp>(launchPoint);
    return runningApp;
}

RunningAppPtr RunningAppList::getByLunaTask(LunaTaskPtr lunaTask, bool verifyLaunchPointId)
{
    if (lunaTask == nullptr)
        return nullptr;

    // key of runningApp are {instanceId} or {appId, displayId}
    const string& instanceId = lunaTask->getInstanceId();
    const string& launchPointId = lunaTask->getLaunchPointId();
    const int& displayId = lunaTask->getDisplayId();
    string appId = lunaTask->getAppId();

    if (appId.empty() && !launchPointId.empty()) {
        LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByLaunchPointId(launchPointId);
        if (launchPoint) {
            appId = launchPoint->getAppId();
        }
    }

    RunningAppPtr runningApp = nullptr;
    if (!instanceId.empty())
        runningApp = getByInstanceId(instanceId);
    else if (!appId.empty())
        runningApp = getByAppId(appId, displayId);

    if (runningApp == nullptr)
        return nullptr;

    // Check validation
    if (!instanceId.empty() && instanceId != runningApp->getInstanceId())
        return nullptr;
    if (!appId.empty() && appId != runningApp->getAppId())
        return nullptr;
    // launch ==> LaunchPointId should be ignored because webOS doesn't allow multiple apps based on launchPointId
    // close, pause ==> LaunchPointId should be checked because those APIs are for runningApp.
    if (verifyLaunchPointId && !launchPointId.empty() && launchPointId != runningApp->getLaunchPointId())
        return nullptr;
    if (displayId != -1 && displayId != runningApp->getDisplayId())
        return nullptr;

    // Sync!
    // Normally, lunaTask doesn't have all information about running application
    // However, SAM needs all information internally during managing application lifecycle.
    if (runningApp) {
        lunaTask->setInstanceId(runningApp->getInstanceId());
        lunaTask->setLaunchPointId(runningApp->getLaunchPointId());
        lunaTask->setAppId(runningApp->getAppId());
        lunaTask->setDisplayId(runningApp->getDisplayId());
    }
    return runningApp;
}

RunningAppPtr RunningAppList::getByIds(const string& instanceId, const string& appId, const int displayId)
{
    RunningAppPtr runningApp = nullptr;
    if (!instanceId.empty())
        runningApp = getByInstanceId(instanceId);
    else if (!appId.empty())
        runningApp = getByAppId(appId, displayId);

    if (runningApp == nullptr)
        return nullptr;

    if (!instanceId.empty() && instanceId != runningApp->getInstanceId())
        return nullptr;
    if (!appId.empty() && appId != runningApp->getAppId())
        return nullptr;
    if (displayId != -1 && displayId != runningApp->getDisplayId())
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

RunningAppPtr RunningAppList::getByAppId(const string& appId, const int displayId)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getAppId() == appId) {
            if (displayId == -1)
                return it->second;
            if ((*it).second->getDisplayId() == displayId)
                return it->second;
        }
    }
    return nullptr;
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

RunningAppPtr RunningAppList::getByLS2Name(const string& ls2name)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getLS2Name() == ls2name) {
            return it->second;
        }
    }
    return nullptr;
}

RunningAppPtr RunningAppList::getByPid(const pid_t pid)
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
    onAdd(std::move(runningApp));
    return true;
}

void RunningAppList::removeByObject(RunningAppPtr runningApp)
{
    if (runningApp == nullptr)
        return;

    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second == runningApp) {
            RunningAppPtr ptr = it->second;
            m_map.erase(it);
            onRemove(std::move(ptr));
            return;
        }
    }
}

void RunningAppList::removeByInstanceId(const string& instanceId)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getInstanceId() == instanceId) {
            RunningAppPtr ptr = it->second;
            m_map.erase(it);
            onRemove(std::move(ptr));
            return;
        }
    }
}

void RunningAppList::removeByPid(const pid_t pid)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if ((*it).second->getProcessId() == pid) {
            RunningAppPtr ptr = it->second;
            m_map.erase(it);
            onRemove(std::move(ptr));
            return;
        }
    }
}

void RunningAppList::removeAllByType(AppType type)
{
    for (auto it = m_map.cbegin(); it != m_map.cend() ;) {
        if (it->second->getLaunchPoint()->getAppDesc()->getAppType() == type) {
            RunningAppPtr ptr = it->second;
            it = m_map.erase(it);
            onRemove(std::move(ptr));
        } else {
            ++it;
        }
    }
}

void RunningAppList::removeAllByConext(AppType type, const int context)
{
    for (auto it = m_map.cbegin(); it != m_map.cend() ;) {
        if (it->second->getLaunchPoint()->getAppDesc()->getAppType() == type &&
            it->second->getContext() == context &&
            it->second->getLifeStatus() != LifeStatus::LifeStatus_LAUNCHING &&
            it->second->getLifeStatus() != LifeStatus::LifeStatus_SPLASHING) {
            // Apps which is in LifeStatus_LAUNCHING & LifeStatus_SPLASHING should not be removed
            RunningAppPtr ptr = it->second;
            it = m_map.erase(it);
            onRemove(std::move(ptr));
        } else {
            ++it;
        }
    }
}

void RunningAppList::removeAllByLaunchPoint(LaunchPointPtr launchPoint)
{
    for (auto it = m_map.cbegin(); it != m_map.cend() ;) {
        if (it->second->getLaunchPoint() == launchPoint) {
            RunningAppPtr ptr = it->second;
            it = m_map.erase(it);
            onRemove(std::move(ptr));
        } else {
            ++it;
        }
    }
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

bool RunningAppList::isTransition(bool devmodeOnly)
{
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if (devmodeOnly) {
            if ((*it).second->getLaunchPoint()->getAppDesc()->isDevmodeApp() &&
                (*it).second->isTransition()) return true;
        } else {
            if ((*it).second->isTransition()) return true;
        }
    }
    return false;
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
         it->second->toAPIJson(object, true);
         array.append(object);
    }
}

void RunningAppList::onAdd(RunningAppPtr runningApp)
{
    // Status should be defined before calling this method
    Logger::info(getClassName(), __FUNCTION__, runningApp->getInstanceId() + " is added");
    ApplicationManager::getInstance().postRunning(std::move(runningApp));
}

void RunningAppList::onRemove(RunningAppPtr runningApp)
{
    Logger::info(getClassName(), __FUNCTION__, runningApp->getInstanceId() + " is removed");
    runningApp->setLifeStatus(LifeStatus::LifeStatus_STOP);
    ApplicationManager::getInstance().postRunning(std::move(runningApp));
}
