// Copyright (c) 2012-2020 LG Electronics, Inc.
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

#include "base/AppDescription.h"
#include "util/JValueUtil.h"

using namespace std;
using namespace pbnjson;

class LaunchPoint;
class LaunchPointList;

typedef shared_ptr<LaunchPoint> LaunchPointPtr;

enum class LaunchPointType : int8_t {
    LaunchPoint_UNKNOWN = 0,
    LaunchPoint_DEFAULT,
    LaunchPoint_BOOKMARK
};

class LaunchPoint {
friend class LaunchPointList;
public:
    static string toString(const LaunchPointType type);
    static LaunchPointType toEnum(const string& type);

    static bool compareTitle(LaunchPointPtr a, LaunchPointPtr b)
    {
        return a->getTitle() < b->getTitle();
    }

    LaunchPoint(AppDescriptionPtr appDesc, const string& launchPointId);
    virtual ~LaunchPoint();

    LaunchPointType getType() const
    {
        return m_type;
    }
    void setType(const LaunchPointType type)
    {
        m_type = type;
    }

    AppDescriptionPtr getAppDesc() const
    {
        return m_appDesc;
    }
    void setAppDesc(AppDescriptionPtr appDesc)
    {
        m_appDesc = appDesc;
    }

    const string getAppId() const
    {
        return m_appDesc->getAppId();
    }

    const string& getLaunchPointId() const
    {
        return m_launchPointId;
    }

    void syncDatabase();
    void setDatabase(const JValue& database);
    void updateDatabase(const JValue& json);

    const string getAppDescription() const
    {
        string appDescription = "";
        if (JValueUtil::getValue(m_database, "appDescription", appDescription)) {
            return appDescription;
        }
        return m_appDesc->getAppDescription();
    }

    const string getBgColor() const
    {
        string bgColor = "";
        if (JValueUtil::getValue(m_database, "bgColor", bgColor)) {
            return bgColor;
        }
        return m_appDesc->getBgColor();
    }

    const string getBgImage() const
    {
        string bgImage = "";
        if (!JValueUtil::getValue(m_database, "bgImage", bgImage))
            bgImage = m_appDesc->getBgImage();
        m_appDesc->applyFolderPath(bgImage);
        return bgImage;
    }

    const JValue getBgImages() const
    {
        return m_database["bgImages"];
    }

    const string getFavicon() const
    {
        string favicon = "";
        if (JValueUtil::getValue(m_database, "favicon", favicon))
            m_appDesc->applyFolderPath(favicon);
        return favicon;
    }

    const string getIcon() const
    {
        string icon = "";
        if (!JValueUtil::getValue(m_database, "icon", icon))
            icon = m_appDesc->getIcon();
        m_appDesc->applyFolderPath(icon);
        return icon;
    }

    const string getImageForRecents() const
    {
        string imageForRecents = "";
        if (JValueUtil::getValue(m_database, "imageForRecents", imageForRecents))
            m_appDesc->applyFolderPath(imageForRecents);
        return imageForRecents;
    }

    const string getLargeIcon() const
    {
        string largeIcon = "";
        if (!JValueUtil::getValue(m_database, "largeIcon", largeIcon))
            largeIcon = m_appDesc->getLargeIcon();
        m_appDesc->applyFolderPath(largeIcon);
        return largeIcon;
    }

    JValue getParams() const
    {
        JValue params;
        if (JValueUtil::getValue(m_database, "params", params)) {
            return params.duplicate();
        } else {
            return pbnjson::Object();
        }
    }

    const string getTitle() const
    {
        string title = "";
        if (JValueUtil::getValue(m_database, "title", title)) {
            return title;
        }
        return m_appDesc->getTitle();
    }

    bool isRemovable() const
    {
        if (m_type == LaunchPointType::LaunchPoint_BOOKMARK)
            return true;
        return m_appDesc->isRemovable();
    }

    bool isSystemApp() const
    {
        if (m_type == LaunchPointType::LaunchPoint_BOOKMARK)
            return false;
        return m_appDesc->isSystemApp();
    }

    bool isUnmovable() const
    {
        bool unmovable = false;
        if (JValueUtil::getValue(m_database, "unmovable", unmovable)) {
            return unmovable;
        }
        return m_appDesc->isUnmovable();
    }

    bool isVisible() const
    {
        if (m_type == LaunchPointType::LaunchPoint_BOOKMARK)
            return true;
        return m_appDesc->isVisible();
    }

    void toJson(JValue& json) const;

private:
    LaunchPoint(const LaunchPoint&);
    LaunchPoint& operator=(const LaunchPoint&) const;

    LaunchPointType m_type;
    AppDescriptionPtr m_appDesc;
    string m_launchPointId;

    bool m_isDirty;
    JValue m_database;

};

#endif /* LAUNCH_POINT_H */
