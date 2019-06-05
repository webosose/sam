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

#include <launch_point/launch_point/launch_point_4_base.h>
#include <launch_point/launch_point/launch_point_factory_4_base.h>
#include <launch_point/launch_point/launch_point_factory_4_base.h>
#include <util/jutil.h>
#include <util/logging.h>
#include <util/utils.h>

LaunchPoint4Base::LaunchPoint4Base(const std::string& id, const std::string& lp_id) :
        LaunchPoint(id, lp_id), relaunch_(false)
{
    icons_ = pbnjson::Array();
}

LaunchPoint4Base::~LaunchPoint4Base()
{
}

std::string LaunchPoint4Base::Update(const pbnjson::JValue& data)
{
    return "";
}

void LaunchPoint4Base::UpdateIfEmpty(LaunchPointPtr lp)
{
    if (lp == nullptr)
        return;

    if (Icon().empty())
        SetIcon(lp->Icon());
    if (IconColor().empty())
        SetIconColor(lp->IconColor());
    if (BgImage().empty())
        SetBgImage(lp->BgImage());
    if (BgColor().empty())
        SetBgColor(lp->BgColor());
    if (ImageForRecents().empty())
        SetImageForRecents(lp->ImageForRecents());
    if (LargeIcon().empty())
        SetLargeIcon(lp->LargeIcon());
    if (AppDescription().empty())
        SetAppDescription(lp->AppDescription());
}

void LaunchPoint4Base::SetAttrWithJson(const pbnjson::JValue& data)
{
    SetFolderPath(data["folderPath"].asString());
    SetTitle(data["title"].asString());
    SetIcon(data["icon"].asString());
    SetBgImage(data["bgImage"].asString());
    SetBgColor(data["bgColor"].asString());
    SetImageForRecents(data["imageForRecents"].asString());
    SetIconColor(data["iconColor"].asString());
    SetLargeIcon(data["largeIcon"].asString());
    SetIcons(data["icons"]);
    SetFavicon(data["favicon"].asString());
    SetAppDescription(data["appDescription"].asString());
    SetUserData(data["userData"].asString());

    if (data.hasKey("bgImages"))
        SetBgImages(data["bgImages"]);

    if (data.hasKey("params"))
        SetParams(data["params"]);

    if (data.hasKey("relaunch"))
        SetRelaunch(data["relaunch"].asBool());

    if (data.hasKey("unmovable"))
        SetUnmovable(data["unmovable"].asBool());
}

void LaunchPoint4Base::SetFavicon(const std::string& favicon)
{
    if (favicon.empty())
        return;

    favicon_ = favicon;
    ConvertPath(favicon_);
}

void LaunchPoint4Base::SetIcons(const pbnjson::JValue& icons)
{
    if (icons.isNull())
        return;
    if (icons.arraySize() <= 0)
        return;

    int arr_idx = icons.arraySize();
    for (int idx = 0; idx < arr_idx; ++idx) {
        pbnjson::JValue icon_set = pbnjson::Object();
        std::string icon_path, icon_color;

        if (!icons[idx].hasKey("icon") || !icons[idx].hasKey("iconColor"))
            continue;

        icon_color = icons[idx]["iconColor"].asString();
        icon_path = icons[idx]["icon"].asString();
        ConvertPath(icon_path);

        icon_set.put("icon", icon_path);
        icon_set.put("iconColor", icon_color);

        icons_.append(icon_set);
    }
}

pbnjson::JValue LaunchPoint4Base::ToJValue() const
{
    pbnjson::JValue json = pbnjson::Object();
    json.put("id", Id());
    json.put("launchPointId", LaunchPointId());
    json.put("lptype", LpTypeAsString());

    json.put("appDescription", AppDescription());
    json.put("bgColor", BgColor());
    json.put("bgImage", BgImage());
    json.put("bgImages", BgImages());
    json.put("favicon", Favicon());
    json.put("icon", Icon());
    json.put("iconColor", IconColor());
    json.put("imageForRecents", ImageForRecents());
    json.put("largeIcon", LargeIcon());
    json.put("icons", Icons());
    json.put("unmovable", IsUnmovable());
    json.put("removable", IsRemovable());
    json.put("relaunch", IsRelaunch());
    json.put("systemApp", IsSystemApp());
    json.put("title", Title());
    json.put("userData", UserData());
    json.put("miniicon", MiniIcon());

    if (!Params().isNull())
        json.put("params", Params());

    return json;
}

