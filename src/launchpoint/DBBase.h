// Copyright (c) 2017-2019 LG Electronics, Inc.
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

#ifndef DBBASE_H
#define DBBASE_H

#include <boost/signals2.hpp>
#include <boost/bind.hpp>
#include <lifecycle/ApplicationErrors.h>
#include <luna-service2/lunaservice.h>
#include "interface/IClassName.h"
#include "interface/ISingleton.h"
#include <pbnjson.hpp>
#include <string>
#include "util/Logger.h"

class DBBase : public ISingleton<DBBase>,
               public IClassName {
friend class ISingleton<DBBase>;
public:
    static bool onFind(LSHandle* sh, LSMessage* message, void* context);
    static bool onPutKind(LSHandle* sh, LSMessage* message, void* context);
    static bool onPutPermissions(LSHandle* sh, LSMessage* message, void* context);

    virtual ~DBBase();

    virtual void init();
    virtual bool insertData(const pbnjson::JValue& json);
    virtual bool updateData(const pbnjson::JValue& json);
    virtual bool deleteData(const pbnjson::JValue& json);

    void loadDb();
    bool query(const std::string& cmd, const std::string& query);

    boost::signals2::signal<void(const pbnjson::JValue& loaded_db_result)> EventDBLoaded;

private:
    static bool onReturnQueryResult(LSHandle* sh, LSMessage* message, void* context);

    DBBase();

    std::string m_name;
    pbnjson::JValue m_permissions;
    pbnjson::JValue m_kind;
};

#endif
