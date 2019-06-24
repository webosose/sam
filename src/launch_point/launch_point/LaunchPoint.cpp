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

#include <launch_point/launch_point/LaunchPoint.h>
#include <launch_point/launch_point/LaunchPointFactory.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/Utils.h>


const char* LOCAL_FILE_URI = "file://";

LaunchPoint::LaunchPoint(const std::string& id, const std::string& lp_id) :
        lp_type_(LPType::UNKNOWN), id_(id), launch_point_id_(lp_id), system_app_(false), removable_(false), default_(false), visible_(false), unmovable_(false), stored_in_db_(false)
{
    bg_images_ = pbnjson::Array();
    params_ = pbnjson::Object();
}

LaunchPoint::~LaunchPoint()
{
}

std::string LaunchPoint::LpTypeAsString() const
{
    switch (lp_type_) {
    case DEFAULT:
        return std::string("default");
    case BOOKMARK:
        return std::string("bookmark");
    default:
        break;
    }

    return std::string("unknown");
}

std::string LaunchPoint::Update(const pbnjson::JValue& data)
{
    return "";
}

void LaunchPoint::UpdateIfEmptyTitle(LaunchPointPtr lp)
{
    if (lp == nullptr)
        return;

    if (Title().empty())
        SetTitle(lp->Title());
}

void LaunchPoint::UpdateIfEmpty(LaunchPointPtr lp)
{
    if (lp == nullptr)
        return;

    if (app_description_.empty())
        SetAppDescription(lp->AppDescription());
    if (bg_color_.empty())
        SetBgColor(lp->BgColor());
    if (bg_image_.empty())
        SetBgImage(lp->BgImage());
    if (icon_color_.empty())
        SetIconColor(lp->IconColor());
    if (icon_.empty())
        SetIcon(lp->Icon());
    if (image_for_recents_.empty())
        SetImageForRecents(lp->ImageForRecents());
    if (large_icon_.empty())
        SetLargeIcon(lp->LargeIcon());
}

void LaunchPoint::SetAttrWithJson(const pbnjson::JValue& data)
{
    SetAppDescription(data["appDescription"].asString());
    SetBgColor(data["bgColor"].asString());
    SetBgImage(data["bgImage"].asString());
    SetBgImages(data["bgImages"]);
    SetFolderPath(data["folderPath"].asString());
    SetIconColor(data["iconColor"].asString());
    SetIcon(data["icon"].asString());
    SetImageForRecents(data["imageForRecents"].asString());
    SetLargeIcon(data["largeIcon"].asString());
    SetTitle(data["title"].asString());
    SetUserData(data["userData"].asString());

    if (data.hasKey("params"))
        SetParams(data["params"]);

    if (data.hasKey("unmovable"))
        SetUnmovable(data["unmovable"].asBool());
}

void LaunchPoint::ConvertPath(std::string& path)
{
    if (path.compare(0, 7, LOCAL_FILE_URI) == 0)
        path.erase(0, 7);

    if (folder_path_.empty())
        return;

    if (path.empty())
        return;

    if (path[0] == '/')
        return;

    path = folder_path_ + "/" + path;
}

void LaunchPoint::SetTitle(const std::string& title)
{
    if (title.empty())
        return;
    title_ = title;
}

void LaunchPoint::SetIcon(const std::string& icon)
{
    if (icon.empty())
        return;
    icon_ = icon;
    ConvertPath(icon_);
}

void LaunchPoint::SetBgImage(const std::string& bg_image)
{
    if (bg_image.empty())
        return;
    bg_image_ = bg_image;
    ConvertPath(bg_image_);
}

void LaunchPoint::SetBgImages(const pbnjson::JValue& bg_images)
{
    if (bg_images.isNull())
        return;

    bg_images_ = pbnjson::Array();
    if (bg_images.arraySize() > 0)
        bg_images_ = bg_images.duplicate();
}

void LaunchPoint::SetBgColor(const std::string& bg_color)
{
    if (bg_color.empty())
        return;
    bg_color_ = bg_color;
}

void LaunchPoint::SetImageForRecents(const std::string& image_for_recents)
{
    if (image_for_recents.empty())
        return;
    image_for_recents_ = image_for_recents;
    ConvertPath(image_for_recents_);
}

void LaunchPoint::SetIconColor(const std::string& icon_color)
{
    if (icon_color.empty())
        return;
    icon_color_ = icon_color;
}

void LaunchPoint::SetLargeIcon(const std::string& large_icon)
{
    if (large_icon.empty())
        return;
    large_icon_ = large_icon;
    ConvertPath(large_icon_);
}

void LaunchPoint::SetAppDescription(const std::string& app_description)
{
    if (app_description.empty())
        return;
    app_description_ = app_description;
}

void LaunchPoint::SetUserData(const std::string& user_data)
{
    if (user_data.empty())
        return;
    user_data_ = user_data;
}

void LaunchPoint::SetParams(const pbnjson::JValue& params)
{
    if (params.isNull())
        return;
    params_ = pbnjson::Object();
    params_ = params.duplicate();
}

pbnjson::JValue LaunchPoint::ToJValue() const
{
    pbnjson::JValue json = pbnjson::Object();
    json.put("id", Id());
    json.put("launchPointId", LaunchPointId());
    json.put("lptype", LpTypeAsString());

    json.put("appDescription", AppDescription());
    json.put("bgColor", BgColor());
    json.put("bgImage", BgImage());
    json.put("bgImages", BgImages());
    json.put("icon", Icon());
    json.put("iconColor", IconColor());
    json.put("imageForRecents", ImageForRecents());
    json.put("largeIcon", LargeIcon());
    json.put("unmovable", IsUnmovable());
    json.put("removable", IsRemovable());
    json.put("systemApp", IsSystemApp());
    json.put("title", Title());
    json.put("userData", UserData());
    json.put("miniicon", MiniIcon());

    if (!Params().isNull())
        json.put("params", Params());

    return json;
}

/****************************/
/***** LPDefault ************/
/****************************/
LaunchPointPtr LPDefault::Create(const std::string& lp_id, const pbnjson::JValue& data, std::string& err_text)
{
    std::string app_id = data["id"].asString();
    if (app_id.empty())
        return nullptr;

    LaunchPointPtr new_lp = std::make_shared<LPDefault>(app_id, lp_id);
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

std::string LPDefault::Update(const pbnjson::JValue& data)
{
    SetAttrWithJson(data);
    return "";
}

/****************************/
/******** LPBookmark ********/
/****************************/
LaunchPointPtr LPBookmark::Create(const std::string& lp_id, const pbnjson::JValue& data, std::string& err_text)
{
    std::string app_id = data["id"].asString();
    if (app_id.empty())
        return nullptr;

    LaunchPointPtr new_lp = std::make_shared<LPBookmark>(app_id, lp_id);
    if (new_lp == nullptr)
        return nullptr;

    new_lp->SetAttrWithJson(data);

    new_lp->SetLpType(LPType::BOOKMARK);
    new_lp->SetDefaultLp(false);
    new_lp->SetRemovable(true);
    new_lp->SetSystemApp(false);
    new_lp->SetVisible(true);

    return new_lp;
}

std::string LPBookmark::Update(const pbnjson::JValue& data)
{
    SetAttrWithJson(data);
    return "";
}

/***************************/
/******** LP Factory *******/
/***************************/
LaunchPointFactory::LaunchPointFactory()
{
    RegisterItem(LPType::DEFAULT, &LPDefault::Create);
    RegisterItem(LPType::BOOKMARK, &LPBookmark::Create);
}

LaunchPointFactory::~LaunchPointFactory()
{
}

void LaunchPointFactory::RegisterItem(const LPType type, CreateLaunchPointFunc func)
{
    factory_map_[type] = func;
}

LaunchPointPtr LaunchPointFactory::CreateLaunchPoint(const LPType type, const std::string& lp_id, const pbnjson::JValue& data, std::string& err_text)
{
    auto item = factory_map_.find(type);
    if (item != factory_map_.end())
        return item->second(lp_id, data, err_text);

    LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 1, PMLOGKS("status", "fail_to_create_launch_point"), "");
    err_text = "unknown type";
    return nullptr;
}
