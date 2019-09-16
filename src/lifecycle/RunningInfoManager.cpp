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

#include <launchpoint/LaunchPointManager.h>
#include <lifecycle/RunningInfoManager.h>
#include <algorithm>

std::string RunningInfoManager::toString(const LifeStatus& status)
{
    std::string str;

    switch (status) {
    case LifeStatus::PRELOADING:
        str = "preloading";
        break;

    case LifeStatus::LAUNCHING:
        str = "launching";
        break;

    case LifeStatus::RELAUNCHING:
        str = "relaunching";
        break;

    case LifeStatus::CLOSING:
        str = "closing";
        break;

    case LifeStatus::STOP:
        str = "stop";
        break;

    case LifeStatus::FOREGROUND:
        str = "foreground";
        break;

    case LifeStatus::BACKGROUND:
        str = "background";
        break;

    case LifeStatus::PAUSING:
        str = "pausing";
        break;

    default:
        str = "unknown";
        break;
    }

    return str;
}

RunningInfoManager::RunningInfoManager()
    : m_foregroundInfo(pbnjson::Array())
{
}

RunningInfoManager::~RunningInfoManager()
{
}

void RunningInfoManager::initialize()
{
}

RunningInfoPtr RunningInfoManager::getRunningInfo(const std::string& appId, const std::string& display)
{
    string key = getKey(appId, display);

    auto it = m_runningList.find(key);
    if (it != m_runningList.end())
        return m_runningList[key];

    if (display != "default")
        return nullptr;

    // TODO this is temp solution. All caller should give display also.
    for (auto& runningInfo : m_runningList) {
        if (runningInfo.second->getAppId() == appId)
            return runningInfo.second;
    }
    return nullptr;
}

RunningInfoPtr RunningInfoManager::getRunningInfoByPid(const std::string& pid)
{
    for (auto& runningInfo : m_runningList) {
        if (runningInfo.second->getPid() == pid)
            return runningInfo.second;
    }
    return nullptr;
}

RunningInfoPtr RunningInfoManager::addRunningInfo(const std::string& appId, const std::string& display)
{
    string key = getKey(appId, display);

    auto it = m_runningList.find(key);
    if (it != m_runningList.end())
        return m_runningList[key];

    RunningInfoPtr runningInfoPtr = std::make_shared<RunningInfo>(appId);
    if (runningInfoPtr == nullptr) {
        Logger::error(getClassName(), __FUNCTION__, "failed_create_new_app_info");
        return nullptr;
    }

    runningInfoPtr->setDisplay(display);
    m_runningList[key] = runningInfoPtr;
    Logger::info(getClassName(), __FUNCTION__, appId, "item_added");
    return runningInfoPtr;
}

bool RunningInfoManager::removeRunningInfo(const std::string& appId, const std::string& display)
{
    string key = getKey(appId, display);
    auto it = m_runningList.find(key);
    if (it == m_runningList.end()) {
        Logger::error(getClassName(), __FUNCTION__, appId, "failed_to_remove: not found appId in running_list");
        return false;
    }

    m_runningList.erase(it);
    Logger::info(getClassName(), __FUNCTION__, appId, "removed");
    return true;
}

bool RunningInfoManager::isAppOnFullscreen(const std::string& appId)
{
    if (appId.empty())
        return false;
    return (m_foregroundAppId == appId);
}

void RunningInfoManager::getForegroundInfoById(const std::string& appId, pbnjson::JValue& info)
{
    if (!m_foregroundInfo.isArray() || m_foregroundInfo.arraySize() < 1)
        return;

    for (auto item : m_foregroundInfo.items()) {
        if (!item.hasKey(Logger::LOG_KEY_APPID) || !item[Logger::LOG_KEY_APPID].isString())
            continue;

        if (item[Logger::LOG_KEY_APPID].asString() == appId) {
            info = item.duplicate();
            return;
        }
    }
}

void RunningInfoManager::getRunningAppIds(std::vector<std::string>& runningAppIds)
{
    for (auto& runningInfo : m_runningList)
        runningAppIds.push_back(runningInfo.second->getAppId());
}

void RunningInfoManager::getRunningList(pbnjson::JValue& runningList, bool devmodeOnly)
{
    if (!runningList.isArray())
        return;

    for (auto& runningInfo : m_runningList) {
        pbnjson::JValue info = pbnjson::Object();

        AppPackagePtr appPackagePtr = AppPackageManager::getInstance().getAppById(runningInfo.second->getAppId());
        if (appPackagePtr == nullptr) {
            Logger::error(getClassName(), __FUNCTION__, runningInfo.second->getAppId(), "failed_to_remove: not found appId in running_list");
            continue;
        }

        if (devmodeOnly && AppTypeByDir::AppTypeByDir_Dev != appPackagePtr->getTypeByDir()) {
            continue;
        }

        std::string appType = AppPackage::toString(appPackagePtr->getAppType());
        info.put("id", runningInfo.second->getAppId());
        info.put("display", runningInfo.second->getDisplay());
        info.put("processid", runningInfo.second->getPid());
        info.put("webprocessid", runningInfo.second->getWebprocid());
        info.put("defaultWindowType", appPackagePtr->getDefaultWindowType());
        info.put("appType", appType);

        runningList.append(info);
    }
}

bool RunningInfoManager::isRunning(const std::string& appId, const std::string& display)
{
    if (getRunningInfo(appId, display) == nullptr)
        return false;
    return true;
}
