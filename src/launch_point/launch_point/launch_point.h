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

enum LPType {
    UNKNOWN = 0, DEFAULT, BOOKMARK
};

class LaunchPoint {
public:
    LaunchPoint(const std::string& id, const std::string& lp_id);
    virtual ~LaunchPoint();

    void ConvertPath(std::string& path);

    void SetLpType(const LPType lp_type)
    {
        lp_type_ = lp_type;
    }
    void SetFolderPath(const std::string& path)
    {
        folder_path_ = path;
    }
    void SetTitle(const std::string& title_str);
    void SetIcon(const std::string& icon);
    void SetBgImage(const std::string& bg_image);
    void SetBgImages(const pbnjson::JValue& bg_images);
    void SetBgColor(const std::string& bg_color);
    void SetImageForRecents(const std::string& image_for_recents);
    void SetIconColor(const std::string& icon_color);
    void SetLargeIcon(const std::string& large_icon);
    void SetAppDescription(const std::string& app_description);
    void SetUserData(const std::string& user_data);
    void SetParams(const pbnjson::JValue& params);
    void SetMiniIcon(const std::string& mini_icon)
    {
        mini_icon_ = mini_icon;
    }
    void SetDefaultLp(bool val)
    {
        default_ = val;
    }
    void SetSystemApp(bool val)
    {
        system_app_ = val;
    }
    void SetRemovable(bool val)
    {
        removable_ = val;
    }
    void SetVisible(bool val)
    {
        visible_ = val;
    }
    void SetUnmovable(bool val)
    {
        unmovable_ = val;
    }
    void SetStoredInDb(bool val)
    {
        stored_in_db_ = val;
    }

    LPType LpType() const
    {
        return lp_type_;
    }
    std::string LpTypeAsString() const;
    const std::string& Id() const
    {
        return id_;
    }
    const std::string& LaunchPointId() const
    {
        return launch_point_id_;
    }
    const std::string& Title() const
    {
        return title_;
    }
    const std::string& Icon() const
    {
        return icon_;
    }
    const std::string& BgImage() const
    {
        return bg_image_;
    }
    const pbnjson::JValue& BgImages() const
    {
        return bg_images_;
    }
    const std::string& BgColor() const
    {
        return bg_color_;
    }
    const std::string& ImageForRecents() const
    {
        return image_for_recents_;
    }
    const std::string& IconColor() const
    {
        return icon_color_;
    }
    const std::string& LargeIcon() const
    {
        return large_icon_;
    }
    const std::string& AppDescription() const
    {
        return app_description_;
    }
    const std::string& UserData() const
    {
        return user_data_;
    }
    const pbnjson::JValue& Params() const
    {
        return params_;
    }
    const std::string& MiniIcon() const
    {
        return mini_icon_;
    }
    bool IsSystemApp() const
    {
        return system_app_;
    }
    bool IsDefault() const
    {
        return default_;
    }
    bool IsRemovable() const
    {
        return removable_;
    }
    bool IsVisible() const
    {
        return visible_;
    }
    bool IsUnmovable() const
    {
        return unmovable_;
    }
    bool IsStoredInDb() const
    {
        return stored_in_db_;
    }

    void UpdateIfEmptyTitle(LaunchPointPtr lp);

    virtual pbnjson::JValue ToJValue() const;
    virtual std::string Update(const pbnjson::JValue& data);
    virtual void UpdateIfEmpty(LaunchPointPtr lp);
    virtual void SetAttrWithJson(const pbnjson::JValue& data);

private:
    // prevent object copy
    LaunchPoint(const LaunchPoint&);
    LaunchPoint& operator=(const LaunchPoint&) const;

    LPType lp_type_;
    std::string folder_path_;
    std::string id_;
    std::string launch_point_id_;
    std::string title_;
    std::string icon_;
    std::string bg_image_;
    pbnjson::JValue bg_images_;
    std::string bg_color_;
    std::string image_for_recents_;
    std::string icon_color_;
    std::string large_icon_;
    std::string app_description_;
    std::string user_data_;
    pbnjson::JValue params_;
    std::string install_time_;
    std::string mini_icon_;
    bool system_app_;
    bool removable_;
    bool default_;
    bool visible_;
    bool unmovable_;
    bool stored_in_db_;
};

class LPDefault: public LaunchPoint {
public:
    LPDefault(const std::string& id, const std::string& lp_id) :
            LaunchPoint(id, lp_id)
    {
    }

    static LaunchPointPtr Create(const std::string& lp_id, const pbnjson::JValue& data, std::string& errText);
    virtual std::string Update(const pbnjson::JValue& data);
};

class LPBookmark: public LaunchPoint {
public:
    LPBookmark(const std::string& id, const std::string& lp_id) :
            LaunchPoint(id, lp_id)
    {
    }

    static LaunchPointPtr Create(const std::string& lp_id, const pbnjson::JValue& data, std::string& errText);
    virtual std::string Update(const pbnjson::JValue& data);
};

typedef std::list<LaunchPointPtr> LaunchPointList;

#endif /* LAUNCH_POINT_H */
