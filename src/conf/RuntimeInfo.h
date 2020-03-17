// Copyright (c) 2020 LG Electronics, Inc.
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

#ifndef CONF_RUNTIMEINFO_H_
#define CONF_RUNTIMEINFO_H_

#include <iostream>

#include <pbnjson.hpp>

#include "Environment.h"
#include "interface/IClassName.h"
#include "interface/ISingleton.h"
#include "util/JValueUtil.h"
#include "util/Logger.h"
#include "util/File.h"

using namespace std;
using namespace pbnjson;

class RuntimeInfo : public ISingleton<RuntimeInfo>,
                    public IClassName {
friend class ISingleton<RuntimeInfo> ;
public:
    virtual ~RuntimeInfo();

    void initialize();

    bool getValue(const string& key, JValue& value);
    bool setValue(const string& key, JValue& value);

    int getDisplayId()
    {
        return m_displayId;
    }

    bool isInContainer()
    {
        return false;
        // TODO this should be enabled to support multiple profiles
        // return m_isInContainer;
    }

private:
    RuntimeInfo();

    bool save();
    bool load();

    JValue m_database;

    int m_displayId;
    bool m_isInContainer;

};

#endif /* CONF_RUNTIMEINFO_H_ */
