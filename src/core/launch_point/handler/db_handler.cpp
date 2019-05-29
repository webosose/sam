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

#include "core/launch_point/handler/db_handler.h"

#include <boost/bind.hpp>
#include <core/util/logging.h>

#include "core/bus/appmgr_service.h"

DbHandler::DbHandler() :
        db_loaded_(false), db_load_count_(0)
{
}

DbHandler::~DbHandler()
{
}

/********************************************************/
/*** Init Handler ***************************************/
/********************************************************/
void DbHandler::Init()
{
    launch_point_db_.signal_db_loaded_.connect(boost::bind(&DbHandler::OnLaunchPointDbLoaded, this, _1));
    launch_point_db_.init();
}

/***********************************************************/
/** Handling DB (handle/reload/update) *********************/
/***********************************************************/
void DbHandler::HandleDbState(bool connection)
{
    LOG_INFO(MSGID_LAUNCH_POINT_DB_HANDLE, 3, PMLOGKS("status", "handle_launch_points_db_state"), PMLOGKS("db_connected", connection?"done":"not_ready_yet"),
            PMLOGKFV("db_load_count", "%d", db_load_count_), "");

    if (!db_loaded_)
        launch_point_db_.loadDb();
}

void DbHandler::OnLaunchPointDbLoaded(const pbnjson::JValue& loaded_db_result)
{
    db_loaded_ = true;
    ++db_load_count_;

    signal_db_loaded_(loaded_db_result);
}

void DbHandler::ReloadDbData(bool connection)
{
    LOG_INFO(MSGID_LAUNCH_POINT_DB_HANDLE, 2, PMLOGKS("status", "reload_db_data"), PMLOGKS("connection", connection?"true":"false"), "");

    db_loaded_ = false;
    HandleDbState(connection);
}

/***********************************************************/
/** Insert/Update/Delete DB ********************************/
/***********************************************************/
bool DbHandler::InsertData(const pbnjson::JValue& json)
{
    return launch_point_db_.insertData(json);
}

bool DbHandler::UpdateData(const pbnjson::JValue& json)
{
    return launch_point_db_.updateData(json);
}

bool DbHandler::DeleteData(const pbnjson::JValue& json)
{
    return launch_point_db_.deleteData(json);
}
