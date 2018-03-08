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

#include "core/lifecycle/application_errors.h"

#include <boost/signals2.hpp>
#include <boost/bind.hpp>
#include <luna-service2/lunaservice.h>
#include <pbnjson.hpp>
#include <string>

class DBBase
{
public:
    DBBase();
    virtual ~DBBase();

    virtual void Init();
    virtual bool InsertData(const pbnjson::JValue& json);
    virtual bool UpdateData(const pbnjson::JValue& json);
    virtual bool DeleteData(const pbnjson::JValue& json);

    virtual std::string Name() const { return db_name_; }

    void LoadDb();
    bool Db8Query(const std::string& cmd, const std::string& query);

    boost::signals2::signal<void (const pbnjson::JValue& loaded_db_result)> signal_db_loaded_;

protected:
    std::string db_name_;
    pbnjson::JValue db_permissions_;
    pbnjson::JValue db_kind_;

private:
    static bool CB_ReturnQueryResult(LSHandle* lshandle, LSMessage* message, void* user_data);

    bool OnReadyToUseDb(pbnjson::JValue result, ErrorInfo err_info, void *user_data);
};

#endif
