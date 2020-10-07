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

#include "base/LaunchPoint.h"
#include "bus/client/DB8.h"
#include "util/JValueUtil.h"

string LaunchPoint::toString(const LaunchPointType type)
{
    switch (type) {
    case LaunchPointType::LaunchPoint_DEFAULT:
        return string("default");

    case LaunchPointType::LaunchPoint_BOOKMARK:
        return string("bookmark");

    default:
        break;
    }

    return string("");
}

LaunchPointType LaunchPoint::toEnum(const string& type)
{
    if (type == "default")
        return LaunchPointType::LaunchPoint_DEFAULT;
    else if (type == "bookmark")
        return LaunchPointType::LaunchPoint_BOOKMARK;
    return LaunchPointType::LaunchPoint_UNKNOWN;
}

LaunchPoint::LaunchPoint(AppDescriptionPtr appDesc, const string& launchPointId)
    : m_type(LaunchPointType::LaunchPoint_UNKNOWN),
      m_appDesc(appDesc),
      m_launchPointId(launchPointId),
      m_isDirty(false)
{
    m_database = pbnjson::Object();
}

LaunchPoint::~LaunchPoint()
{
}

void LaunchPoint::syncDatabase()
{
    if (m_isDirty == false) {
        return;
    }

    JValue json = m_database.duplicate();

    json.put("id", m_appDesc->getAppId());
    json.put("type", toString(getType()));
    json.put("launchPointId", m_launchPointId);

    bool isOld = (json.hasKey("_id") && json.hasKey("_rev") && json.hasKey("_kind"));

    if (isOld) {
        if (DB8::getInstance().updateLaunchPoint(json)) {
            m_isDirty = false;
        }
    } else {
        if (DB8::getInstance().insertLaunchPoint(json)) {
            m_isDirty = false;
        }
    }
}

void LaunchPoint::setDatabase(const JValue& database)
{
    // This method should be called by DB8 instance
    m_database = database.duplicate();
}

void LaunchPoint::updateDatabase(const JValue& json)
{
    for (JValue::KeyValue obj : json.children()) {
        string key = obj.first.asString();

        if (!m_database.hasKey(key)) {
            m_database.put(key, obj.second);
            m_isDirty = true;
            continue;
        }

        if (m_database[key] != obj.second) {
            m_database.put(key, obj.second);
            m_isDirty = true;
        }
    }
}

void LaunchPoint::toJson(JValue& json) const
{
    m_appDesc->toJson(json);
    for (JValue::KeyValue obj : m_database.children()) {
        string key = obj.first.asString();

        if (key == "_id" || key == "_rev" || key == "_kind")
            continue;

        if (!json.hasKey(key)) {
            json.put(key, obj.second);
            continue;
        }

        if (json[key] != obj.second) {
            json.put(key, obj.second);
        }
    }

    json.put("launchPointId", getLaunchPointId());
    json.put("lptype", toString(m_type));

    // Need to convert fullpath
    json.put("favicon", getFavicon());
    json.put("icon", getIcon());
    json.put("bgImage", getBgImage());
    json.put("imageForRecents", getImageForRecents());
    json.put("largeIcon", getLargeIcon());
}
