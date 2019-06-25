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

#ifndef LAUNCH_POINT_H
#define LAUNCH_POINT_H

#include <glib.h>
#include <list>
#include <functional>
#include <memory>
#include <pbnjson.hpp>
#include <string>

class LaunchPoint;

#define DEFAULT_ICON_W 64
#define DEFAULT_ICON_H 64

typedef std::shared_ptr<const LaunchPoint> LaunchPointConstPtr;
typedef std::shared_ptr<LaunchPoint> LaunchPointPtr;
typedef std::function<LaunchPointPtr(const std::string&, const pbnjson::JValue&, std::string&)> CreateLaunchPointFunc;

enum class LPType : int8_t {
    UNKNOWN = 0,
    DEFAULT,
    BOOKMARK
};

class LaunchPoint {
public:
    LaunchPoint(const std::string& id, const std::string& lp_id);
    virtual ~LaunchPoint();

    void convertPath(std::string& path);

    void setLpType(const LPType lp_type)
    {
        m_LPType = lp_type;
    }
    void setFolderPath(const std::string& path)
    {
        m_folderPath = path;
    }
    void setTitle(const std::string& title_str);
    void setIcon(const std::string& icon);
    void setBgImage(const std::string& bg_image);
    void setBgImages(const pbnjson::JValue& bg_images);
    void setBgColor(const std::string& bg_color);
    void setImageForRecents(const std::string& image_for_recents);
    void setIconColor(const std::string& icon_color);
    void setLargeIcon(const std::string& large_icon);
    void setAppDescription(const std::string& app_description);
    void setUserData(const std::string& user_data);
    void setParams(const pbnjson::JValue& params);
    void setMiniIcon(const std::string& mini_icon)
    {
        m_miniIcon = mini_icon;
    }
    void setDefaultLp(bool val)
    {
        m_default = val;
    }
    void setSystemApp(bool val)
    {
        m_systemApp = val;
    }
    void setRemovable(bool val)
    {
        m_removable = val;
    }
    void setVisible(bool val)
    {
        m_visible = val;
    }
    void setUnmovable(bool val)
    {
        m_unmovable = val;
    }
    void setStoredInDb(bool val)
    {
        m_storedInDB = val;
    }

    LPType getLPType() const
    {
        return m_LPType;
    }
    std::string LPTypeAsString() const;
    const std::string& Id() const
    {
        return m_appId;
    }
    const std::string& getLunchPointId() const
    {
        return m_launchPointId;
    }
    const std::string& getTitle() const
    {
        return m_title;
    }
    const std::string& getIcon() const
    {
        return m_icon;
    }
    const std::string& getBgImage() const
    {
        return m_bgImage;
    }
    const pbnjson::JValue& getBgImages() const
    {
        return m_bgImages;
    }
    const std::string& getBgColor() const
    {
        return m_bgColor;
    }
    const std::string& getImageForRecents() const
    {
        return m_imageForRecents;
    }
    const std::string& getIconColor() const
    {
        return m_iconColor;
    }
    const std::string& getLargeIcon() const
    {
        return m_largeIcon;
    }
    const std::string& getAppDescription() const
    {
        return m_appDescription;
    }
    const std::string& getUserData() const
    {
        return m_userData;
    }
    const pbnjson::JValue& getParams() const
    {
        return m_params;
    }
    const std::string& getMiniIcon() const
    {
        return m_miniIcon;
    }
    bool isSystemApp() const
    {
        return m_systemApp;
    }
    bool isDefault() const
    {
        return m_default;
    }
    bool isRemovable() const
    {
        return m_removable;
    }
    bool isVisible() const
    {
        return m_visible;
    }
    bool isUnmovable() const
    {
        return m_unmovable;
    }
    bool isStoredInDb() const
    {
        return m_storedInDB;
    }

    void updateIfEmptyTitle(LaunchPointPtr lp);

    virtual pbnjson::JValue toJValue() const;
    virtual std::string update(const pbnjson::JValue& data);
    virtual void updateIfEmpty(LaunchPointPtr lp);
    virtual void setAttrWithJson(const pbnjson::JValue& data);


    void setFavicon(const std::string& favicon);
    void setIcons(const pbnjson::JValue& icons);
    void setRelaunch(bool val)
    {
        m_relaunch = val;
    }

    const std::string& getFavicon() const
    {
        return m_favicon;
    }

    const pbnjson::JValue& getIcons() const
    {
        return m_icons;
    }

    bool isRelaunch() const
    {
        return m_relaunch;
    }

protected:
    // prevent object copy
    LaunchPoint(const LaunchPoint&);
    LaunchPoint& operator=(const LaunchPoint&) const;

    LPType m_LPType;
    std::string m_folderPath;
    std::string m_appId;
    std::string m_launchPointId;
    std::string m_title;
    std::string m_icon;
    std::string m_bgImage;
    pbnjson::JValue m_bgImages;
    std::string m_bgColor;
    std::string m_imageForRecents;
    std::string m_iconColor;
    std::string m_largeIcon;
    std::string m_appDescription;
    std::string m_userData;
    pbnjson::JValue m_params;
    std::string m_installTime;
    std::string m_miniIcon;
    bool m_systemApp;
    bool m_removable;
    bool m_default;
    bool m_visible;
    bool m_unmovable;
    bool m_storedInDB;
    std::string m_favicon;
    pbnjson::JValue m_icons;
    bool m_relaunch;
};

class LPDefault: public LaunchPoint {
public:
    LPDefault(const std::string& id, const std::string& lp_id)
        : LaunchPoint(id, lp_id)
    {
    }

    static LaunchPointPtr create(const std::string& lp_id, const pbnjson::JValue& data, std::string& errText);
    virtual std::string update(const pbnjson::JValue& data);
};

class LPBookmark: public LaunchPoint {
public:
    LPBookmark(const std::string& id, const std::string& lp_id)
        : LaunchPoint(id, lp_id)
    {
    }

    static LaunchPointPtr create(const std::string& lp_id, const pbnjson::JValue& data, std::string& errText);
    virtual std::string update(const pbnjson::JValue& data);
};

typedef std::list<LaunchPointPtr> LaunchPointList;

#endif /* LAUNCH_POINT_H */
