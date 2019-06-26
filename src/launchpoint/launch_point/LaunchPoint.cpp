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

#include <launchpoint/launch_point/LaunchPoint.h>
#include <launchpoint/launch_point/LaunchPointFactory.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/Utils.h>


const char* LOCAL_FILE_URI = "file://";

LaunchPoint::LaunchPoint(const std::string& id, const std::string& lp_id)
    : m_LPType(LPType::UNKNOWN),
      m_appId(id),
      m_launchPointId(lp_id),
      m_systemApp(false),
      m_removable(false),
      m_default(false),
      m_visible(false),
      m_unmovable(false),
      m_storedInDB(false),
      m_relaunch(false)
{
    m_bgImages = pbnjson::Array();
    m_params = pbnjson::Object();
    m_icons = pbnjson::Array();
}

LaunchPoint::~LaunchPoint()
{
}

std::string LaunchPoint::LPTypeAsString() const
{
    switch (m_LPType) {
    case LPType::DEFAULT:
        return std::string("default");
    case LPType::BOOKMARK:
        return std::string("bookmark");
    default:
        break;
    }

    return std::string("unknown");
}

std::string LaunchPoint::update(const pbnjson::JValue& data)
{
    return "";
}

void LaunchPoint::updateIfEmptyTitle(LaunchPointPtr lp)
{
    if (lp == nullptr)
        return;

    if (getTitle().empty())
        setTitle(lp->getTitle());
}

void LaunchPoint::updateIfEmpty(LaunchPointPtr lp)
{
    if (lp == nullptr)
        return;

    if (m_appDescription.empty())
        setAppDescription(lp->getAppDescription());
    if (m_bgColor.empty())
        setBgColor(lp->getBgColor());
    if (m_bgImage.empty())
        setBgImage(lp->getBgImage());
    if (m_iconColor.empty())
        setIconColor(lp->getIconColor());
    if (m_icon.empty())
        setIcon(lp->getIcon());
    if (m_imageForRecents.empty())
        setImageForRecents(lp->getImageForRecents());
    if (m_largeIcon.empty())
        setLargeIcon(lp->getLargeIcon());
}

void LaunchPoint::setAttrWithJson(const pbnjson::JValue& data)
{
    setAppDescription(data["appDescription"].asString());
    setBgColor(data["bgColor"].asString());
    setBgImage(data["bgImage"].asString());
    setBgImages(data["bgImages"]);
    setFolderPath(data["folderPath"].asString());
    setIconColor(data["iconColor"].asString());
    setIcon(data["icon"].asString());
    setIcons(data["icons"]);
    setImageForRecents(data["imageForRecents"].asString());
    setLargeIcon(data["largeIcon"].asString());
    setTitle(data["title"].asString());
    setUserData(data["userData"].asString());
    setFavicon(data["favicon"].asString());

    if (data.hasKey("params"))
        setParams(data["params"]);

    if (data.hasKey("relaunch"))
        setRelaunch(data["relaunch"].asBool());

    if (data.hasKey("unmovable"))
        setUnmovable(data["unmovable"].asBool());
}

void LaunchPoint::convertPath(std::string& path)
{
    if (path.compare(0, 7, LOCAL_FILE_URI) == 0)
        path.erase(0, 7);

    if (m_folderPath.empty())
        return;

    if (path.empty())
        return;

    if (path[0] == '/')
        return;

    path = m_folderPath + "/" + path;
}

void LaunchPoint::setTitle(const std::string& title)
{
    if (title.empty())
        return;
    m_title = title;
}

void LaunchPoint::setIcon(const std::string& icon)
{
    if (icon.empty())
        return;
    m_icon = icon;
    convertPath(m_icon);
}

void LaunchPoint::setBgImage(const std::string& bg_image)
{
    if (bg_image.empty())
        return;
    m_bgImage = bg_image;
    convertPath(m_bgImage);
}

void LaunchPoint::setBgImages(const pbnjson::JValue& bg_images)
{
    if (bg_images.isNull())
        return;

    m_bgImages = pbnjson::Array();
    if (bg_images.arraySize() > 0)
        m_bgImages = bg_images.duplicate();
}

void LaunchPoint::setBgColor(const std::string& bg_color)
{
    if (bg_color.empty())
        return;
    m_bgColor = bg_color;
}

void LaunchPoint::setImageForRecents(const std::string& image_for_recents)
{
    if (image_for_recents.empty())
        return;
    m_imageForRecents = image_for_recents;
    convertPath(m_imageForRecents);
}

void LaunchPoint::setIconColor(const std::string& icon_color)
{
    if (icon_color.empty())
        return;
    m_iconColor = icon_color;
}

void LaunchPoint::setLargeIcon(const std::string& large_icon)
{
    if (large_icon.empty())
        return;
    m_largeIcon = large_icon;
    convertPath(m_largeIcon);
}

void LaunchPoint::setAppDescription(const std::string& app_description)
{
    if (app_description.empty())
        return;
    m_appDescription = app_description;
}

void LaunchPoint::setUserData(const std::string& user_data)
{
    if (user_data.empty())
        return;
    m_userData = user_data;
}

void LaunchPoint::setParams(const pbnjson::JValue& params)
{
    if (params.isNull())
        return;
    m_params = pbnjson::Object();
    m_params = params.duplicate();
}

pbnjson::JValue LaunchPoint::toJValue() const
{
    pbnjson::JValue json = pbnjson::Object();
    json.put("id", Id());
    json.put("launchPointId", getLunchPointId());
    json.put("lptype", LPTypeAsString());

    json.put("appDescription", getAppDescription());
    json.put("bgColor", getBgColor());
    json.put("bgImage", getBgImage());
    json.put("bgImages", getBgImages());
    json.put("favicon", getFavicon());
    json.put("icon", getIcon());
    json.put("iconColor", getIconColor());
    json.put("imageForRecents", getImageForRecents());
    json.put("largeIcon", getLargeIcon());
    json.put("unmovable", isUnmovable());
    json.put("removable", isRemovable());
    json.put("relaunch", isRelaunch());
    json.put("systemApp", isSystemApp());
    json.put("title", getTitle());
    json.put("userData", getUserData());
    json.put("miniicon", getMiniIcon());

    if (!getParams().isNull())
        json.put("params", getParams());

    return json;
}

void LaunchPoint::setFavicon(const std::string& favicon)
{
    if (favicon.empty())
        return;

    m_favicon = favicon;
    convertPath(m_favicon);
}

void LaunchPoint::setIcons(const pbnjson::JValue& icons)
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
        convertPath(icon_path);

        icon_set.put("icon", icon_path);
        icon_set.put("iconColor", icon_color);

        m_icons.append(icon_set);
    }
}


/****************************/
/***** LPDefault ************/
/****************************/
LaunchPointPtr LPDefault::create(const std::string& lp_id, const pbnjson::JValue& data, std::string& err_text)
{
    std::string app_id = data["id"].asString();
    if (app_id.empty())
        return nullptr;

    LaunchPointPtr new_lp = std::make_shared<LPDefault>(app_id, lp_id);
    if (new_lp == nullptr) {
        err_text = "fail to create instance";
        return nullptr;
    }

    new_lp->setAttrWithJson(data);

    new_lp->setLpType(LPType::DEFAULT);
    new_lp->setDefaultLp(true);
    new_lp->setRemovable(data["removable"].asBool());
    new_lp->setSystemApp(data["systemApp"].asBool());
    new_lp->setVisible(data["visible"].asBool());
    new_lp->setMiniIcon(data["miniicon"].asString());

    return new_lp;
}

std::string LPDefault::update(const pbnjson::JValue& data)
{
    setAttrWithJson(data);
    return "";
}

/****************************/
/******** LPBookmark ********/
/****************************/
LaunchPointPtr LPBookmark::create(const std::string& lp_id, const pbnjson::JValue& data, std::string& err_text)
{
    std::string app_id = data["id"].asString();
    if (app_id.empty())
        return nullptr;

    LaunchPointPtr new_lp = std::make_shared<LPBookmark>(app_id, lp_id);
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

std::string LPBookmark::update(const pbnjson::JValue& data)
{
    setAttrWithJson(data);
    return "";
}
