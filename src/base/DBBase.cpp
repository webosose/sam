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

#include <base/DBBase.h>
#include <bus/AppMgrService.h>
#include <util/CallChain.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/LSUtils.h>

namespace CallChain_DBConfig {
class CheckKind: public LSCallItem {
public:
    CheckKind(LSHandle *handle, const char *uri, const char *payload)
        : LSCallItem(handle, uri, payload)
    {
    }

protected:
    virtual bool onReceiveCall(pbnjson::JValue message)
    {
        if (message["returnValue"].asBool()) {
            if (!message.hasKey("results") || !message["results"].isArray()) {
                std::string error_text = message["errorText"].asString();

                if (error_text.empty())
                    error_text = "results is not valid";
                setError(DBREGISTER_ERR_KEYFIND, error_text);

                return false;
            }

            pbnjson::JValue chain_data = getChainData();
            chain_data.put("find_results", message["results"]);
            setChainData(chain_data);

            return true;
        }

        return false;
    }
};

class RegKind: public LSCallItem {
public:
    RegKind(LSHandle *handle, const char *uri, const char *payload)
        : LSCallItem(handle, uri, payload)
    {
    }

protected:
    virtual bool onReceiveCall(pbnjson::JValue message)
    {
        if (message["returnValue"].asBool())
            return true;

        std::string error_text = message["errorText"].asString();

        if (error_text.empty())
            error_text = "register kind is failed";
        setError(DBREGISTER_ERR_KINDREG, error_text);

        return false;
    }
};

class RegPermission: public LSCallItem {
public:
    RegPermission(LSHandle *handle, const char *uri, const char *payload)
        : LSCallItem(handle, uri, payload)
    {
    }

protected:
    virtual bool onReceiveCall(pbnjson::JValue message)
    {
        if (message["returnValue"].asBool())
            return true;

        std::string error_text = message["errorText"].asString();

        if (error_text.empty())
            error_text = "register Permission is failed";
        setError(DBREGISTER_ERR_PERMITREG, error_text);

        return false;
    }
};
}

DBBase::DBBase()
{
    m_permissions = pbnjson::Object();
    m_kind = pbnjson::Object();
}

DBBase::~DBBase()
{
}

void DBBase::loadDb()
{
    LOG_INFO(MSGID_DB_LOAD, 2, PMLOGKS("where", "LoadDb"), PMLOGKS("name", m_name.c_str()), "");

    //using call chain
    CallChain& call_chain = CallChain::acquire(std::bind(&DBBase::onReadyToUseDb, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), NULL, NULL);

    std::string qry = "{\"query\":{\"from\":\"" + m_name + "\", \"orderBy\":\"_rev\"}}";
    const char* qry_ptr = qry.c_str();

    //1. check key from DB
    auto check_kind = std::make_shared<CallChain_DBConfig::CheckKind>(AppMgrService::instance().serviceHandle(), "luna://com.webos.service.db/find", qry_ptr);

    call_chain.add(check_kind);

    //2. register kind to DB
    auto reg_kind = std::make_shared<CallChain_DBConfig::RegKind>(AppMgrService::instance().serviceHandle(), "luna://com.webos.service.db/putKind", m_kind.stringify().c_str());

    call_chain.add_if(check_kind, false, reg_kind);

    //3. register permisson
    pbnjson::JValue permission_data = pbnjson::Object();
    permission_data.put("permissions", m_permissions);

    auto reg_permission = std::make_shared<CallChain_DBConfig::RegPermission>(AppMgrService::instance().serviceHandle(), "luna://com.webos.service.db/putPermissions",
            permission_data.stringify().c_str());

    call_chain.add_if(reg_kind, true, reg_permission);

    auto get_kind = std::make_shared<CallChain_DBConfig::CheckKind>(AppMgrService::instance().serviceHandle(), "luna://com.webos.service.db/find", qry_ptr);

    call_chain.add_if(reg_permission, true, get_kind);

    call_chain.run();
}

bool DBBase::onReadyToUseDb(pbnjson::JValue result, ErrorInfo err_info, void *user_data)
{
    if (!result.hasKey("returnValue")) {
        LOG_ERROR(MSGID_DB_LOAD_ERR, 2,
                  PMLOGKS("status", "on_ready_to_use_db"),
                  PMLOGKS("result", "returnValue_does_not_exist"), "%s", err_info.errorText.c_str());
        return false;
    }

    if (!result["returnValue"].asBool()) {
        LOG_ERROR(MSGID_DB_LOAD_ERR, 2,
                  PMLOGKS("status", "on_ready_to_use_db"),
                  PMLOGKS("result", result["returnValue"].asBool()?"success":"fail"),
                  "%s", err_info.errorText.c_str());
        return false;
    }

    if (!result.hasKey("find_results") || !result["find_results"].isArray()) {
        LOG_ERROR(MSGID_DB_LOAD_ERR, 3,
                  PMLOGKS("status", "on_ready_to_use_db"),
                  PMLOGKS("result", result["returnValue"].asBool()?"success":"fail"),
                  PMLOGKS("reason", "loaded_db_results_is_not_valid"), "");
        return false;
    }

    pbnjson::JValue loaded_db_result = result["find_results"];

    LOG_INFO(MSGID_DB_LOAD, 2,
             PMLOGKS("status", "on_ready_to_use_db"),
             PMLOGKS("reason", "received_find_db"), "");
    signalDbLoaded(loaded_db_result);

    return true;
}

bool DBBase::query(const std::string& cmd, const std::string& query)
{

    LOG_INFO(MSGID_DB_LOAD, 2,
             PMLOGKS("api", cmd.c_str()),
             PMLOGKS("name", m_name.c_str()), "");

    LSErrorSafe lserror;
    if (!LSCallOneReply(AppMgrService::instance().serviceHandle(), cmd.c_str(), query.c_str(), onReturnQueryResult, this, NULL, &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "lscall_one_reply"),
                  PMLOGKS("service_name", "com.webos.service.db"),
                  PMLOGKS("api", cmd.c_str()), "err: %s", lserror.message);
        return false;
    }

    return true;
}

bool DBBase::onReturnQueryResult(LSHandle* lshandle, LSMessage* message, void* user_data)
{
    pbnjson::JValue result = JUtil::parse(LSMessageGetPayload(message), std::string(""));

    if (result.isNull()) {
        LOG_ERROR(MSGID_LSCALL_RETURN_FAIL, 1, PMLOGKS("result", "Null"), "");
    }

    if (!result["returnValue"].asBool()) {
        LOG_ERROR(MSGID_LSCALL_RETURN_FAIL, 1,
                  PMLOGKFV("db errorText", "%s", result["errorText"].asString().c_str()), "");
    }

    return true;
}

bool DBBase::insertData(const pbnjson::JValue& json)
{
    return true;
}

bool DBBase::updateData(const pbnjson::JValue& json)
{
    return true;
}

bool DBBase::deleteData(const pbnjson::JValue& json)
{
    return true;
}
