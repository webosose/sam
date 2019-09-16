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

#include <boost/bind.hpp>
#include <launchpoint/handler/DBHandler.h>
#include <launchpoint/DBBase.h>

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
    DBBase::getInstance().init();
    DBBase::getInstance().EventDBLoaded.connect(boost::bind(&DBHandler::onLaunchPointDbLoaded, this, _1));
}

void DBHandler::handleDbState(bool connection)
{
    Logger::info(getClassName(), __FUNCTION__, "", Logger::format("connection(%B) m_DBLoadCount(%d)", connection, m_DBLoadCount));

    if (!m_isDBLoaded)
        DBBase::getInstance().loadDb();
}

void DBHandler::onLaunchPointDbLoaded(const pbnjson::JValue& loaded_db_result)
{
    m_isDBLoaded = true;
    ++m_DBLoadCount;

    EventDBLoaded_(loaded_db_result);
}

void DBHandler::reloadDbData(bool connection)
{
    Logger::info(getClassName(), __FUNCTION__, "", "reload_db_data");
    m_isDBLoaded = false;
    handleDbState(connection);
}

bool DBHandler::insertData(const pbnjson::JValue& json)
{
    return DBBase::getInstance().insertData(json);
}

bool DBHandler::updateData(const pbnjson::JValue& json)
{
    return DBBase::getInstance().updateData(json);
}

bool DBHandler::deleteData(const pbnjson::JValue& json)
{
    return DBBase::getInstance().deleteData(json);
}
