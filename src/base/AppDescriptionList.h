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

#ifndef BASE_APPDESCRIPTIONLIST_H_
#define BASE_APPDESCRIPTIONLIST_H_

#include <iostream>
#include <map>
#include <memory>

#include "AppDescription.h"
#include "interface/IClassName.h"
#include "interface/ISingleton.h"

using namespace std;

class AppDescriptionList : public ISingleton<AppDescriptionList>,
                           public IClassName {
friend class ISingleton<AppDescriptionList>;
public:
    static bool compare(AppDescriptionPtr me, AppDescriptionPtr another);
    static bool compareVersion(AppDescriptionPtr me, AppDescriptionPtr another);

    virtual ~AppDescriptionList();

    void changeLocale();

    void scanFull();
    void scanDir(const string& path, const AppLocation& appLocation);
    void scanApp(const string& appId, const string& folderPath, const AppLocation& appLocation);

    AppDescriptionPtr create(const std::string& appId);
    AppDescriptionPtr add(AppDescriptionPtr appDesc);
    AppDescriptionPtr getById(const std::string& appId);
    void remove(AppDescriptionPtr appDesc);
    void toJson(JValue& json, JValue& properties, bool devmode = false);

private:
    AppDescriptionList();
    string findAppDir(const string& appId);

    map<string, AppDescriptionPtr> m_map;
};

#endif /* BASE_APPDESCRIPTIONLIST_H_ */
