// Copyright (c) 2017-2018 LG Electronics, Inc.
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

#include <launch_point/launch_point/launch_point_bookmark_4_base.h>
#include <launch_point/launch_point/launch_point_default_4_base.h>
#include <launch_point/launch_point/launch_point_factory_4_base.h>
#include <util/base_logs.h>

LaunchPointFactory4Basic::LaunchPointFactory4Basic()
{
}

LaunchPointFactory4Basic::~LaunchPointFactory4Basic()
{
}

LaunchPointPtr LaunchPointFactory4Basic::CreateLaunchPoint(const LPType type, const std::string& lp_id, const pbnjson::JValue& data, std::string& errText)
{
    switch (type) {
    case DEFAULT:
        return LaunchPointDefault4Base::Create(lp_id, data, errText);
    case BOOKMARK:
        return LaunchPointBookmark4Base::Create(lp_id, data, errText);
    default:
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 1, PMLOGKS("status", "fail_to_create_launch_point"), "");
        errText = "unknown type";
    }
    return nullptr;
}
