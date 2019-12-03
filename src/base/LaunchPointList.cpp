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

#include "base/LaunchPointList.h"

#include <sys/time.h>
#include <boost/lexical_cast.hpp>

#include "RunningAppList.h"
#include "bus/client/DB8.h"
#include "bus/service/ApplicationManager.h"
#include "util/JValueUtil.h"

LaunchPointList::LaunchPointList()
{
    setClassName("LaunchPointList");
}

LaunchPointList::~LaunchPointList()
{

}

void LaunchPointList::clear()
{
    m_list.clear();
}

void LaunchPointList::sort()
{
    m_list.sort(LaunchPoint::compareTitle);
}

LaunchPointPtr LaunchPointList::createBootmarkByAPI(AppDescriptionPtr appDesc, const JValue& database)
{
    string launchPointId = generateLaunchPointId(LaunchPointType::LaunchPoint_BOOKMARK, appDesc->getAppId());
    LaunchPointPtr launchPoint = make_shared<LaunchPoint>(appDesc, launchPointId);
    launchPoint->setType(LaunchPointType::LaunchPoint_BOOKMARK);
    launchPoint->updateDatabase(database);
    return launchPoint;
}

LaunchPointPtr LaunchPointList::createBootmarkByDB(AppDescriptionPtr appDesc, const JValue& database)
{
    string launchPointId = "";

    if (!JValueUtil::getValue(database, "launchPointId", launchPointId))
        return nullptr;

    LaunchPointPtr launchPoint = make_shared<LaunchPoint>(appDesc, launchPointId);
    launchPoint->setType(LaunchPointType::LaunchPoint_BOOKMARK);
    launchPoint->setDatabase(database);
    return launchPoint;
}

LaunchPointPtr LaunchPointList::createDefault(AppDescriptionPtr appDesc)
{
    string launchPointId = generateLaunchPointId(LaunchPointType::LaunchPoint_DEFAULT, appDesc->getAppId());
    LaunchPointPtr launchPoint = make_shared<LaunchPoint>(appDesc, launchPointId);
    launchPoint->setType(LaunchPointType::LaunchPoint_DEFAULT);
    return launchPoint;
}

LaunchPointPtr LaunchPointList::getByLunaTask(LunaTaskPtr lunaTask)
{
    if (lunaTask == nullptr)
        return nullptr;

    return getByIds(lunaTask->getLaunchPointId(), lunaTask->getAppId());
}

LaunchPointPtr LaunchPointList::getByIds(const string& launchPointId, const string& appId)
{
    LaunchPointPtr launchPoint = nullptr;
    if (!launchPointId.empty())
        launchPoint = getByLaunchPointId(launchPointId);
    else if (!appId.empty())
        launchPoint = getByAppId(appId);

    if (launchPoint == nullptr)
        return nullptr;

    if (!launchPointId.empty() && launchPointId != launchPoint->getLaunchPointId())
        return nullptr;
    if (!appId.empty() && appId != launchPoint->getAppId())
        return nullptr;

    return launchPoint;
}

LaunchPointPtr LaunchPointList::getByAppId(const string& appId)
{
    if (appId.empty())
        return nullptr;

    string launchPointId = generateLaunchPointId(LaunchPointType::LaunchPoint_DEFAULT, appId);
    return getByLaunchPointId(launchPointId);
}

LaunchPointPtr LaunchPointList::getByLaunchPointId(const string& launchPointId)
{
    if (launchPointId.empty())
        return nullptr;

    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->getLaunchPointId() == launchPointId) {
            Logger::error(getClassName(), __FUNCTION__, "Found LaunchPoint");
            return *it;
        }
    }
    return nullptr;
}

bool LaunchPointList::add(LaunchPointPtr launchPoint)
{
    if (launchPoint == nullptr || launchPoint->getLaunchPointId().empty()) {
        Logger::error(getClassName(), __FUNCTION__, "Invalid launchPoint");
        return false;
    }
    if (isExist(launchPoint->getLaunchPointId())) {
        Logger::error(getClassName(), __FUNCTION__, "The launchPoint is already registered");
        return false;
    }

    onAdd(launchPoint);
    return true;
}

bool LaunchPointList::remove(LaunchPointPtr launchPoint)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it) == launchPoint) {
            m_list.erase(it);
            onRemove(launchPoint);
            return true;
        }
    }
    return true;
}

bool LaunchPointList::removeByAppDesc(AppDescriptionPtr appDesc)
{
    for (auto it = m_list.begin(); it != m_list.end();) {
        if ((*it)->getAppDesc() == appDesc) {
            LaunchPointPtr launchPoint = *it;
            it = m_list.erase(it);
            onRemove(launchPoint);
        } else {
            ++it;
        }
    }
    return true;
}

bool LaunchPointList::removeByAppId(const string& appId)
{
    for (auto it = m_list.begin(); it != m_list.end();) {
        if ((*it)->getAppDesc()->getAppId() == appId) {
            LaunchPointPtr launchPoint = *it;
            it = m_list.erase(it);
            onRemove(launchPoint);
        } else {
            ++it;
        }
    }
    return true;
}

bool LaunchPointList::removeByLaunchPointId(const string& launchPointId)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->getLaunchPointId() == launchPointId) {
            LaunchPointPtr launchPoint = *it;
            it = m_list.erase(it);
            onRemove(launchPoint);
            return true;
        }
    }
    return false;
}

bool LaunchPointList::isExist(const string& launchPointId)
{
    if (launchPointId.empty())
        return false;

    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->getLaunchPointId() == launchPointId)
            return true;
    }
    return false;
}

void LaunchPointList::toJson(JValue& json)
{
    if (!json.isArray()) {
        return;
    }

    for (auto it = m_list.begin(); it != m_list.end(); ++it) {
        if ((*it)->isVisible()) {
            JValue item;
            (*it)->toJson(item);
            json.append(item);
        }
    }
}

string LaunchPointList::generateLaunchPointId(LaunchPointType type, const string& appId)
{
    if (type == LaunchPointType::LaunchPoint_DEFAULT) {
        return appId + "_default";
    }

    while (true) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        double verifier = tv.tv_usec;

        string launchPointId = appId + "_" + boost::lexical_cast<string>(verifier);
        if (LaunchPointList::getInstance().getByLaunchPointId(launchPointId) == nullptr)
            return launchPointId;
    }

    return string("");
}

void LaunchPointList::onAdd(LaunchPointPtr launchPoint)
{
    Logger::info(getClassName(), __FUNCTION__, launchPoint->getLaunchPointId() + " is added");
    launchPoint->syncDatabase();
    m_list.push_back(launchPoint);
    ApplicationManager::getInstance().postListLaunchPoints(launchPoint, "added");
}

void LaunchPointList::onRemove(LaunchPointPtr launchPoint)
{
    Logger::info(getClassName(), __FUNCTION__, launchPoint->getLaunchPointId() + " is removed");
    RunningAppList::getInstance().removeByLaunchPoint(launchPoint);
    DB8::getInstance().deleteLaunchPoint(launchPoint->getLaunchPointId());
    ApplicationManager::getInstance().postListLaunchPoints(launchPoint, "removed");
}
