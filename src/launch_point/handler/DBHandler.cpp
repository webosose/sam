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

#include <boost/bind.hpp>
#include <launch_point/handler/DBHandler.h>
#include <util/Logging.h>
#include <util/Logging.h>

DBHandler::DBHandler()
    : m_isDBLoaded(false),
      m_DBLoadCount(0)
{
}

DBHandler::~DBHandler()
{
}

void DBHandler::init()
{
    m_launchPointDB.init();
    m_launchPointDB.signalDbLoaded.connect(boost::bind(&DBHandler::onLaunchPointDbLoaded, this, _1));
}

void DBHandler::handleDbState(bool connection)
{
    LOG_INFO(MSGID_LAUNCH_POINT_DB_HANDLE, 3,
             PMLOGKS("status", "handle_launch_points_db_state"),
             PMLOGKS("db_connected", connection?"done":"not_ready_yet"),
             PMLOGKFV("db_load_count", "%d", m_DBLoadCount), "");

    if (!m_isDBLoaded)
        m_launchPointDB.loadDb();
}

void DBHandler::onLaunchPointDbLoaded(const pbnjson::JValue& loaded_db_result)
{
    m_isDBLoaded = true;
    ++m_DBLoadCount;

    signal_db_loaded_(loaded_db_result);
}

void DBHandler::reloadDbData(bool connection)
{
    LOG_INFO(MSGID_LAUNCH_POINT_DB_HANDLE, 2,
             PMLOGKS("status", "reload_db_data"),
             PMLOGKS("db_connected", connection ? "done" : "not_ready_yet"), "");

    m_isDBLoaded = false;
    handleDbState(connection);
}

bool DBHandler::insertData(const pbnjson::JValue& json)
{
    return m_launchPointDB.insertData(json);
}

bool DBHandler::updateData(const pbnjson::JValue& json)
{
    return m_launchPointDB.updateData(json);
}

bool DBHandler::deleteData(const pbnjson::JValue& json)
{
    return m_launchPointDB.deleteData(json);
}
