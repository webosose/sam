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

#include <launchpoint/launch_point/LaunchPoint.h>
#include <launchpoint/launch_point/LaunchPointFactory.h>

const char* LOCAL_FILE_URI = "file://";

LaunchPoint::LaunchPoint(const std::string& appId, const std::string& launchPointId)
    : m_LPType(LPType::UNKNOWN),
      m_appId(appId),
      m_launchPointId(launchPointId),
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

void LaunchPoint::updateIfEmptyTitle(LaunchPointPtr launchPointPtr)
{
    if (launchPointPtr == nullptr)
        return;

    if (getTitle().empty())
        setTitle(launchPointPtr->getTitle());
}

void LaunchPoint::updateIfEmpty(LaunchPointPtr launchPointPtr)
{
    if (launchPointPtr == nullptr)
        return;

    if (m_appDescription.empty())
        setAppDescription(launchPointPtr->getAppDescription());
    if (m_bgColor.empty())
        setBgColor(launchPointPtr->getBgColor());
    if (m_bgImage.empty())
        setBgImage(launchPointPtr->getBgImage());
    if (m_iconColor.empty())
        setIconColor(launchPointPtr->getIconColor());
    if (m_icon.empty())
        setIcon(launchPointPtr->getIcon());
    if (m_imageForRecents.empty())
        setImageForRecents(launchPointPtr->getImageForRecents());
    if (m_largeIcon.empty())
        setLargeIcon(launchPointPtr->getLargeIcon());
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
    setUserData(data["context"].asString());
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

void LaunchPoint::setUserData(const std::string& context)
{
    if (context.empty())
        return;
    m_context = context;
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
LaunchPointPtr LPDefault::create(const std::string& launchPointId, const pbnjson::JValue& data, std::string& errorText)
{
    std::string appId = data["id"].asString();
    if (appId.empty())
        return nullptr;

    LaunchPointPtr newLaunchPoint = std::make_shared<LPDefault>(appId, launchPointId);
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

std::string LPDefault::update(const pbnjson::JValue& data)
{
    setAttrWithJson(data);
    return "";
}

/****************************/
/******** LPBookmark ********/
/****************************/
LaunchPointPtr LPBookmark::create(const std::string& launchPointId, const pbnjson::JValue& data, std::string& errorText)
{
    std::string appId = data["id"].asString();
    if (appId.empty())
        return nullptr;

    LaunchPointPtr newLaunchPoint = std::make_shared<LPBookmark>(appId, launchPointId);
    if (newLaunchPoint == nullptr)
        return nullptr;

    newLaunchPoint->setAttrWithJson(data);

    newLaunchPoint->setLpType(LPType::BOOKMARK);
    newLaunchPoint->setDefaultLp(false);
    newLaunchPoint->setRemovable(true);
    newLaunchPoint->setSystemApp(false);
    newLaunchPoint->setVisible(true);

    return newLaunchPoint;
}

std::string LPBookmark::update(const pbnjson::JValue& data)
{
    setAttrWithJson(data);
    return "";
}
