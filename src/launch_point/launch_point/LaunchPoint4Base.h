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

#ifndef LAUNCH_POINT_4_BASE_H
#define LAUNCH_POINT_4_BASE_H

#include <glib.h>
#include <launch_point/ILaunchPointFactory.h>
#include <launch_point/launch_point/LaunchPoint.h>
#include <list>
#include <functional>
#include <memory>
#include <pbnjson.hpp>
#include <string>


class LaunchPoint4Base: public LaunchPoint {
public:
    LaunchPoint4Base(const std::string& id, const std::string& lp_id);
    virtual ~LaunchPoint4Base();

    void SetFavicon(const std::string& favicon);
    void SetIcons(const pbnjson::JValue& icons);
    void SetRelaunch(bool val)
    {
        relaunch_ = val;
    }

    const std::string& Favicon() const
    {
        return favicon_;
    }

    const pbnjson::JValue& Icons() const
    {
        return icons_;
    }

    bool IsRelaunch() const
    {
        return relaunch_;
    }

    virtual pbnjson::JValue ToJValue() const;
    virtual std::string Update(const pbnjson::JValue& data);
    virtual void UpdateIfEmpty(LaunchPointPtr lp);
    virtual void SetAttrWithJson(const pbnjson::JValue& data);

private:
    // prevent object copy
    LaunchPoint4Base(const LaunchPoint4Base&);
    LaunchPoint4Base& operator=(const LaunchPoint4Base&) const;

    std::string favicon_;
    pbnjson::JValue icons_;
    bool relaunch_;
};

typedef std::shared_ptr<const LaunchPoint4Base> LaunchPoint4BaseConstPtr;
typedef std::shared_ptr<LaunchPoint4Base> LaunchPoint4BasePtr;
typedef std::list<LaunchPoint4BasePtr> LaunchPoint4BaseList;

#endif /* LAUNCH_POINT_4_BASE_H */
