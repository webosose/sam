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

#ifndef BASE_SETTINGS_H
#define BASE_SETTINGS_H

#include <map>
#include <string>
#include <vector>
#include <pbnjson.hpp>
#include <util/singleton.h>


class BaseSettings {
public:
    bool Load(const char* file_path);
    bool LoadSAMConf(const char* file_path);

    pbnjson::JValue launch_point_dbkind_;
    pbnjson::JValue launch_point_permissions_;

private:
    friend class Singleton<BaseSettings> ;
    BaseSettings();
    virtual ~BaseSettings();

};

typedef Singleton<BaseSettings> BaseSettingsImpl;

#endif  // BASE_SETTINGS_H
