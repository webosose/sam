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

#include <bus/service/ApplicationManager.h>
#include <bus/client/DB8.h>
#include <launchpoint/DBBase.h>
#include <util/LSUtils.h>
#include <setting/Settings.h>
#include <util/JValueUtil.h>

DBBase::DBBase()
{
    m_name = "com.webos.applicationManager.launchPoints:1";
    m_kind = pbnjson::Object();
    m_permissions = pbnjson::Object();
}

DBBase::~DBBase()
{
}

bool DBBase::onFind(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = pbnjson::JDomParser::fromString(LSMessageGetPayload(message));
    bool returnValue = false;
    JValue results;
    string errorText;

    JValueUtil::getValue(responsePayload, "returnValue", returnValue);
    JValueUtil::getValue(responsePayload, "errorText", errorText);
    JValueUtil::getValue(responsePayload, "results", results);

    if (!returnValue || !results.isArray()) {
        if (!errorText.empty())
            Logger::warning("DBBase", __FUNCTION__, errorText);
        else
            Logger::warning("DBBase", __FUNCTION__, "results is not valid");
        Call call = DB8::getInstance().putKind(onPutKind, getInstance().m_kind);
        return true;
    }
    Logger::info(DBBase::getInstance().getClassName(), __FUNCTION__, DBBase::getInstance().m_name, "received_find_db");
    DBBase::getInstance().EventDBLoaded(results);

    return true;
}

bool DBBase::onPutKind(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = pbnjson::JDomParser::fromString(LSMessageGetPayload(message));
    bool returnValue = false;
    string errorText;

    JValueUtil::getValue(responsePayload, "returnValue", returnValue);
    JValueUtil::getValue(responsePayload, "errorText", errorText);

    if (!returnValue) {
        Logger::error(DBBase::getInstance().getClassName(), __FUNCTION__, errorText);
        return true;
    } else {
        Call call = DB8::getInstance().putPermissions(onPutPermissions, getInstance().m_permissions);
    }
    return true;
}

bool DBBase::onPutPermissions(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = pbnjson::JDomParser::fromString(LSMessageGetPayload(message));
    bool returnValue = false;
    string errorText;

    JValueUtil::getValue(responsePayload, "returnValue", returnValue);
    JValueUtil::getValue(responsePayload, "errorText", errorText);

    if (!returnValue) {
        Logger::error(DBBase::getInstance().getClassName(), __FUNCTION__, errorText);
        return true;
    } else {
        Call call = DB8::getInstance().find(onPutPermissions, getInstance().m_name);
    }
    return true;
}

void DBBase::loadDb()
{
    Logger::info(getClassName(), __FUNCTION__, m_name, "start load database");
    Call call = DB8::getInstance().find(onFind, m_name);
}

bool DBBase::query(const std::string& cmd, const std::string& query)
{
    Logger::info(getClassName(), __FUNCTION__, m_name, cmd);

    LSErrorSafe lserror;
    if (!LSCallOneReply(ApplicationManager::getInstance().get(),
                        cmd.c_str(),
                        query.c_str(),
                        onReturnQueryResult,
                        this,
                        NULL,
                        &lserror)) {
        Logger::error(getClassName(), __FUNCTION__, "LS2", cmd, lserror.message);
        return false;
    }

    return true;
}

bool DBBase::onReturnQueryResult(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue result = JDomParser::fromString(LSMessageGetPayload(message));

    if (result.isNull()) {
        // TODO
        Logger::error("DBBase", __FUNCTION__, "LS2", "response is null");
    }

    if (!result["returnValue"].asBool()) {
        // TODO
        Logger::error("DBBase", __FUNCTION__, "LS2", "'returnValue' is false", result.stringify());
    }

    return true;
}

void DBBase::init()
{
    m_kind = SettingsImpl::getInstance().m_launchPointDbkind;
    m_permissions = SettingsImpl::getInstance().m_launchPointPermissions;
}

bool DBBase::insertData(const pbnjson::JValue& json)
{
    std::string db8_api = "luna://com.webos.service.db/put";
    std::string db8_qry;
    pbnjson::JValue json_db_qry = pbnjson::Object();

    if (json.hasKey("appDescription"))
        json_db_qry.put("appDescription", json["appDescription"]);
    if (json.hasKey("bgColor"))
        json_db_qry.put("bgColor", json["bgColor"]);
    if (json.hasKey("bgImage"))
        json_db_qry.put("bgImage", json["bgImage"]);
    if (json.hasKey("bgImages"))
        json_db_qry.put("bgImages", json["bgImages"]);
    if (json.hasKey("favicon"))
        json_db_qry.put("favicon", json["favicon"]);
    if (json.hasKey("icon"))
        json_db_qry.put("icon", json["icon"]);
    if (json.hasKey("iconColor"))
        json_db_qry.put("iconColor", json["iconColor"]);
    if (json.hasKey("imageForRecents"))
        json_db_qry.put("imageForRecents", json["imageForRecents"]);
    if (json.hasKey("largeIcon"))
        json_db_qry.put("largeIcon", json["largeIcon"]);
    if (json.hasKey("unmovable"))
        json_db_qry.put("unmovable", json["unmovable"]);
    if (json.hasKey("params"))
        json_db_qry.put("params", json["params"]);
    if (json.hasKey("relaunch"))
        json_db_qry.put("relaunch", json["relaunch"]);
    if (json.hasKey("title"))
        json_db_qry.put("title", json["title"]);
    if (json.hasKey("miniicon"))
        json_db_qry.put("miniicon", json["miniicon"]);

    if (json_db_qry.isNull())
        return false;

    json_db_qry.put("_kind", m_name);
    json_db_qry.put("id", json["id"]);
    json_db_qry.put("lptype", json["lptype"]);
    json_db_qry.put("launchPointId", json["launchPointId"]);

    db8_qry = "{\"objects\":[" + json_db_qry.stringify() + "]}";

    return query(db8_api, db8_qry);
}

bool DBBase::updateData(const pbnjson::JValue& json)
{
    std::string db8_api = "luna://com.webos.service.db/merge";
    std::string db8_qry;
    pbnjson::JValue json_db_qry = pbnjson::Object();
    pbnjson::JValue json_where = pbnjson::Object();

    json_db_qry.put("props", pbnjson::Object());

    if (json.hasKey("appDescription"))
        json_db_qry["props"].put("appDescription", json["appDescription"]);
    if (json.hasKey("bgColor"))
        json_db_qry["props"].put("bgColor", json["bgColor"]);
    if (json.hasKey("bgImage"))
        json_db_qry["props"].put("bgImage", json["bgImage"]);
    if (json.hasKey("bgImages"))
        json_db_qry["props"].put("bgImages", json["bgImages"]);
    if (json.hasKey("favicon"))
        json_db_qry["props"].put("favicon", json["favicon"]);
    if (json.hasKey("icon"))
        json_db_qry["props"].put("icon", json["icon"]);
    if (json.hasKey("iconColor"))
        json_db_qry["props"].put("iconColor", json["iconColor"]);
    if (json.hasKey("imageForRecents"))
        json_db_qry["props"].put("imageForRecents", json["imageForRecents"]);
    if (json.hasKey("largeIcon"))
        json_db_qry["props"].put("largeIcon", json["largeIcon"]);
    if (json.hasKey("unmovable"))
        json_db_qry["props"].put("unmovable", json["unmovable"]);
    if (json.hasKey("params"))
        json_db_qry["props"].put("params", json["params"]);
    if (json.hasKey("relaunch"))
        json_db_qry["props"].put("relaunch", json["relaunch"]);
    if (json.hasKey("title"))
        json_db_qry["props"].put("title", json["title"]);
    if (json.hasKey("userData"))
        json_db_qry["props"].put("userData", json["userData"]);
    if (json.hasKey("miniicon"))
        json_db_qry["props"].put("miniicon", json["miniicon"]);

    if (json_db_qry["props"].isNull())
        return false;

    json_db_qry.put("query", pbnjson::Object());
    json_db_qry["query"].put("from", m_name);
    json_db_qry["query"].put("where", pbnjson::Array());

    json_where.put("prop", "launchPointId");
    json_where.put("op", "=");
    json_where.put("val", json["launchPointId"]);
    json_db_qry["query"]["where"].append(json_where);

    db8_qry = json_db_qry.stringify();

    return query(db8_api, db8_qry);
}

bool DBBase::deleteData(const pbnjson::JValue& json)
{
    std::string db8_api = "luna://com.webos.service.db/del";
    std::string db8_qry;
    pbnjson::JValue json_db_qry = pbnjson::Object();
    pbnjson::JValue json_where = pbnjson::Object();

    json_db_qry.put("query", pbnjson::Object());
    json_db_qry["query"].put("from", m_name);
    json_db_qry["query"].put("where", pbnjson::Array());

    json_where.put("prop", "launchPointId");
    json_where.put("op", "=");
    json_where.put("val", json["launchPointId"]);
    json_db_qry["query"]["where"].append(json_where);

    db8_qry = json_db_qry.stringify();

    return query(db8_api, db8_qry);
}
