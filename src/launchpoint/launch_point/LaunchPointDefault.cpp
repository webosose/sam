// Copyright (c) 2017-2019 LG Electronics, Inc.
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

#include <launchpoint/launch_point/LaunchPointDefault.h>

LaunchPointPtr LaunchPointDefault::create(const std::string& launchPointId, const pbnjson::JValue& data, std::string& errorText)
{
    std::string appId = data["id"].asString();
    if (appId.empty())
        return nullptr;

    LaunchPointPtr newLaunchPoint = std::make_shared<LaunchPointDefault>(appId, launchPointId);
    if (newLaunchPoint == nullptr) {
        errorText = "fail to create instance";
        return nullptr;
    }

    newLaunchPoint->setAttrWithJson(data);

    newLaunchPoint->setLpType(LPType::DEFAULT);
    newLaunchPoint->setDefaultLp(true);
    newLaunchPoint->setRemovable(data["removable"].asBool());
    newLaunchPoint->setSystemApp(data["systemApp"].asBool());
    newLaunchPoint->setVisible(data["visible"].asBool());
    newLaunchPoint->setMiniIcon(data["miniicon"].asString());

    return newLaunchPoint;
}

std::string LaunchPointDefault::update(const pbnjson::JValue& data)
{
    setAttrWithJson(data);
    return "";
}
