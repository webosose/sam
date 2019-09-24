// Copyright (c) 2019 LG Electronics, Inc.
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

#ifndef BASE_LAUNCHPOINTLIST_H_
#define BASE_LAUNCHPOINTLIST_H_

#include <iostream>
#include <list>

#include "interface/ISingleton.h"
#include "LaunchPoint.h"

using namespace std;

class LaunchPointList : public ISingleton<LaunchPointList> {
friend class ISingleton<LaunchPointList>;
public:
    virtual ~LaunchPointList();

    void clear();
    void sort();

    LaunchPointPtr createBootmark(AppDescriptionPtr appDesc, const JValue& database);
    LaunchPointPtr createDefault(AppDescriptionPtr appDesc);

    LaunchPointPtr add(LaunchPointPtr launchPoint);

    LaunchPointPtr getByAppId(const string& appId);
    LaunchPointPtr getByLaunchPointId(const string& launchPointId);

    void remove(AppDescriptionPtr appDesc);
    void remove(LaunchPointPtr launchPoint);
    void removeByAppId(const string& appId);
    void removeByLaunchPointId(const string& launchPointId);

    void toJson(JValue& json);
    string generateLaunchPointId(LaunchPointType type, const string& appId);

private:
    LaunchPointList();

    list<LaunchPointPtr> m_list;
};

#endif /* BASE_LAUNCHPOINTLIST_H_ */
