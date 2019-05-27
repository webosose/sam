// Copyright (c) 2012-2018 LG Electronics, Inc.
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

#include "core/lifecycle/app_info.h"

#include "core/base/logging.h"

AppInfo::AppInfo(const std::string& app_id) :
        m_appId(app_id), m_executionLock(false), m_removalFlag(false), m_preloadModeOn(false), m_lastLaunchTime(0), m_lifeStatus(LifeStatus::STOP), m_runtimeStatus(RuntimeStatus::STOP), m_virtualLaunchParams(
                pbnjson::Object())
{
}

AppInfo::~AppInfo()
{
}

void AppInfo::set_life_status(const LifeStatus& status)
{
    LOG_DEBUG("[AppInfo] (%s) app_id: %s, status: %d", __FUNCTION__, m_appId.c_str(), (int ) status);
    m_lifeStatus = status;
}
