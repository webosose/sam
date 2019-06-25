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

#include <launch_point/launch_point/LaunchPointBookmark.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/Utils.h>


LaunchPointPtr LaunchPointBookmark::create(const std::string& lp_id, const pbnjson::JValue& data, std::string& err_text)
{
    std::string app_id = data["id"].asString();
    if (app_id.empty())
        return nullptr;

    LaunchPointPtr new_lp = std::make_shared<LaunchPointBookmark>(app_id, lp_id);
    if (new_lp == nullptr)
        return nullptr;

    new_lp->setAttrWithJson(data);

    new_lp->setLpType(LPType::BOOKMARK);
    new_lp->setDefaultLp(false);
    new_lp->setRemovable(true);
    new_lp->setSystemApp(false);
    new_lp->setVisible(true);

    return new_lp;
}

std::string LaunchPointBookmark::update(const pbnjson::JValue& data)
{
    setAttrWithJson(data);
    return "";
}
