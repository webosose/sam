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

#ifndef DBBASE_H
#define DBBASE_H

#include <boost/signals2.hpp>
#include <boost/bind.hpp>
#include <lifecycle/ApplicationErrors.h>
#include <luna-service2/lunaservice.h>
#include <pbnjson.hpp>
#include <string>

class DBBase {
public:
    DBBase();
    virtual ~DBBase();

    virtual void init() = 0;
    virtual bool insertData(const pbnjson::JValue& json);
    virtual bool updateData(const pbnjson::JValue& json);
    virtual bool deleteData(const pbnjson::JValue& json);

    void loadDb();
    bool query(const std::string& cmd, const std::string& query);

    boost::signals2::signal<void(const pbnjson::JValue& loaded_db_result)> EventDBLoaded;

protected:
    std::string m_name;
    pbnjson::JValue m_permissions;
    pbnjson::JValue m_kind;

private:
    static bool onReturnQueryResult(LSHandle* lshandle, LSMessage* message, void* user_data);

    bool onReadyToUseDb(pbnjson::JValue result, ErrorInfo err_info, void *user_data);
};

#endif
