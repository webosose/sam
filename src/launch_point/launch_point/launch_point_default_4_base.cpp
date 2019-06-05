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

#include <launch_point/launch_point/launch_point_default_4_base.h>

LaunchPointPtr LaunchPointDefault4Base::Create(const std::string& lp_id, const pbnjson::JValue& data, std::string& err_text)
{
    std::string app_id = data["id"].asString();
    if (app_id.empty())
        return nullptr;

    LaunchPoint4BasePtr new_lp = std::make_shared<LaunchPointDefault4Base>(app_id, lp_id);
    if (new_lp == nullptr) {
        err_text = "fail to create instance";
        return nullptr;
    }

    new_lp->SetAttrWithJson(data);

    new_lp->SetLpType(LPType::DEFAULT);
    new_lp->SetDefaultLp(true);
    new_lp->SetRemovable(data["removable"].asBool());
    new_lp->SetSystemApp(data["systemApp"].asBool());
    new_lp->SetVisible(data["visible"].asBool());
    new_lp->SetMiniIcon(data["miniicon"].asString());

    return new_lp;
}

std::string LaunchPointDefault4Base::Update(const pbnjson::JValue& data)
{
    SetAttrWithJson(data);
    return "";
}
