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

RunningAppPtr RunningAppList::create(LaunchPointPtr launchPoint)
{
    RunningAppPtr runningApp = std::make_shared<RunningApp>(launchPoint);
    if (runningApp == nullptr) {
        Logger::error(getClassName(), __FUNCTION__, "failed_create_new_app_info");
        return nullptr;
    }
    return runningApp;
}

RunningAppPtr RunningAppList::add(RunningAppPtr runningApp)
{
    if (runningApp == nullptr)
        return nullptr;

    m_list.push_back(runningApp);
    ApplicationManager::getInstance().postRunning(runningApp->getLaunchPoint()->getAppDesc()->isDevmodeApp());
    return runningApp;
}

RunningAppPtr RunningAppList::getByAppId(const string& appId, bool createIfNotExist)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->getLaunchPoint()->getAppDesc()->getAppId() == appId)
            return *it;
    }

    if (!createIfNotExist)
        return nullptr;

    LaunchPointPtr launchPoint = LaunchPointList::getInstance().getByAppId(appId);
    if (launchPoint == nullptr)
        return nullptr;
    RunningAppPtr runningApp = create(launchPoint);
    add(runningApp);
    return runningApp;
}

RunningAppPtr RunningAppList::getByLaunchPointId(const string& launchPointId)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->getLaunchPoint()->getLaunchPointId() == launchPointId)
            return *it;
    }
    return nullptr;
}

RunningAppPtr RunningAppList::getByPid(const string& pid)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->getPid() == pid)
            return *it;
    }
    return nullptr;
}

RunningAppPtr RunningAppList::getByInstanceId(const string& launchPointId)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->getInstanceId() == launchPointId)
            return *it;
    }
    return nullptr;
}

RunningAppPtr RunningAppList::getForeground()
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->getLifeStatus() == LifeStatus::LifeStatus_FOREGROUND)
            return *it;
    }
    return nullptr;
}

void RunningAppList::remove(RunningAppPtr runningApp)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it) == runningApp) {
            m_list.erase(it);
            ApplicationManager::getInstance().postRunning(runningApp->getLaunchPoint()->getAppDesc()->isDevmodeApp());
            return;
        }
    }
}

void RunningAppList::removeByAppId(const string& appId)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->getLaunchPoint()->getAppDesc()->getAppId() == appId) {
            m_list.erase(it);
            ApplicationManager::getInstance().postRunning((*it)->getLaunchPoint()->getAppDesc()->isDevmodeApp());
            return;
        }
    }
}

bool RunningAppList::isRunning(const string& appId)
{
    return (getByAppId(appId) != nullptr);
}

bool RunningAppList::isForeground(const string& appId)
{
    RunningAppPtr runningApp = getByAppId(appId);
    if (runningApp == nullptr) return false;
    return (runningApp->getLifeStatus() == LifeStatus::LifeStatus_FOREGROUND);
}

void RunningAppList::toJson(JValue& array, bool devmodeOnly)
{
    if (!array.isArray())
        return;

    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if (devmodeOnly && AppLocation::AppLocation_Devmode != (*it)->getLaunchPoint()->getAppDesc()->getAppLocation()) {
            continue;
        }

        pbnjson::JValue object = pbnjson::Object();
        (*it)->toJson(object);
        array.append(object);
    }
}
