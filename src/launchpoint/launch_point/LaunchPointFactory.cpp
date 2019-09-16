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

#include <launchpoint/launch_point/LaunchPointFactory.h>

LaunchPointFactory::LaunchPointFactory()
{
    registerItem(LPType::DEFAULT, &LPDefault::create);
    registerItem(LPType::BOOKMARK, &LPBookmark::create);
}

LaunchPointFactory::~LaunchPointFactory()
{
}

void LaunchPointFactory::registerItem(const LPType type, CreateLaunchPointFunc func)
{
    m_factoryMap[type] = func;
}

LaunchPointPtr LaunchPointFactory::createLaunchPoint(const LPType type, const std::string& launchPointId, const pbnjson::JValue& data, std::string& errorText)
{
    auto item = m_factoryMap.find(type);
    if (item != m_factoryMap.end())
        return item->second(launchPointId, data, errorText);

    Logger::error(getClassName(), __FUNCTION__, "", "fail_to_create_launch_point");
    errorText = "unknown type";
    return nullptr;
}
