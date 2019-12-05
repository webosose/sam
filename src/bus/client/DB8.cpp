// Copyright (c) 2012-2019 LG Electronics, Inc.
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

#include "DB8.h"

#include "base/LaunchPointList.h"
#include "bus/service/ApplicationManager.h"
#include "conf/SAMConf.h"
#include "util/JValueUtil.h"
#include "util/Logger.h"

const char* DB8::KIND_NAME = "com.webos.applicationManager.launchpoints:2";

bool DB8::onResponse(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    if (responsePayload.isNull())
        return true;
    return true;
}

DB8::DB8()
    : AbsLunaClient("com.webos.service.db")
{
    setClassName("DB8");
}

DB8::~DB8()
{

}

bool DB8::insertLaunchPoint(JValue& json)
{
    static string method = string("luna://") + getName() + string("/put");

    if (json.isNull())
        return false;

    json.put("_kind", KIND_NAME);

    JValue requestPayload = pbnjson::Object();
    requestPayload.put("objects", pbnjson::Array());
    requestPayload["objects"].append(json);

    Logger::logCallRequest(getClassName(), __FUNCTION__, method, requestPayload);
    LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onResponse,
        nullptr,
        nullptr,
        nullptr
    );
    return true;
}

bool DB8::updateLaunchPoint(const JValue& props)
{
    static string method = string("luna://") + getName() + string("/merge");
    JValue requestPayload = pbnjson::Object();
    JValue where = pbnjson::Array();
    JValue query = pbnjson::Object();

    if (props.isNull())
        return false;

    where.put("prop", "launchPointId");
    where.put("op", "=");
    where.put("val", props["launchPointId"]);

    query.put("where", where);
    query.put("from", KIND_NAME);

    requestPayload.put("props", props);
    requestPayload.put("query", query);

    Logger::logCallRequest(getClassName(), __FUNCTION__, method, requestPayload);
    LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onResponse,
        nullptr,
        nullptr,
        nullptr
    );
    return true;
}

void DB8::deleteLaunchPoint(const string& launchPointId)
{
    static string method = string("luna://") + getName() + string("/del");
    JValue requestPayload = pbnjson::Object();
    JValue where = pbnjson::Object();

    requestPayload.put("query", pbnjson::Object());
    requestPayload["query"].put("from", KIND_NAME);
    requestPayload["query"].put("where", pbnjson::Array());

    where.put("prop", "launchPointId");
    where.put("op", "=");
    where.put("val", launchPointId);
    requestPayload["query"]["where"].append(where);

    Logger::logCallRequest(getClassName(), __FUNCTION__, method, requestPayload);
    LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onResponse,
        nullptr,
        nullptr,
        nullptr
    );
}

void DB8::onInitialzed()
{
    // nothing
}

void DB8::onFinalized()
{

}

void DB8::onServerStatusChanged(bool isConnected)
{
    if (isConnected) {
        Logger::info(getClassName(), __FUNCTION__, "DB8 is connected. Start loading launchPoints");
        find();
    }
}

bool DB8::onFind(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    if (responsePayload.isNull())
        return true;

    bool returnValue = false;
    JValue results;
    string errorText;

    JValueUtil::getValue(responsePayload, "returnValue", returnValue);
    JValueUtil::getValue(responsePayload, "errorText", errorText);
    JValueUtil::getValue(responsePayload, "results", results);

    if (!returnValue || !results.isArray()) {
        if (!errorText.empty())
            Logger::warning(getInstance().getClassName(), __FUNCTION__, errorText);
        else
            Logger::warning(getInstance().getClassName(), __FUNCTION__, "results is not valid");
        getInstance().putKind();
        return true;
    }
    Logger::info(getInstance().getClassName(), __FUNCTION__, "Start to sync DB8");

    string appId;
    string launchPointId;
    string type;
    AppDescriptionPtr appDesc = nullptr;
    LaunchPointPtr launchPoint = nullptr;
    int size = results.arraySize();
    for (int i = 0; i < size; ++i) {
        if (!JValueUtil::getValue(results[i], "id", appId) ||
            !JValueUtil::getValue(results[i], "launchPointId", launchPointId) ||
            !JValueUtil::getValue(results[i], "type", type)) {
            Logger::warning(getInstance().getClassName(), __FUNCTION__, "Invalid data in DB8");
            continue;
        }

        if (appId.empty() || type.empty()) {
            DB8::getInstance().deleteLaunchPoint(launchPointId);
            continue;
        }

        appDesc = AppDescriptionList::getInstance().getByAppId(appId);
        if (appDesc == nullptr) {
            Logger::warning(getInstance().getClassName(), __FUNCTION__, "The app is already uninstalled");
            DB8::getInstance().deleteLaunchPoint(launchPointId);
            continue;
        }

        launchPoint = LaunchPointList::getInstance().getByLaunchPointId(launchPointId);
        if (type == "default") {
            launchPoint->setDatabase(results[i]);
        } else if (type == "bookmark") {
            if (launchPoint == nullptr) {
                launchPoint = LaunchPointList::getInstance().createBootmarkByDB(appDesc, results[i]);
                LaunchPointList::getInstance().add(launchPoint);
            }
        }
    }
    Logger::info(getInstance().getClassName(), __FUNCTION__, "Complete to sync DB8");

    // 여기서 LaunchPoints를 만들어 넣어야 함.
    return true;
}

void DB8::find()
{
    static string method = string("luna://") + getName() + string("/find");

    JValue requestPayload = pbnjson::Object();
    requestPayload.put("query", pbnjson::Object());
    requestPayload["query"].put("from", KIND_NAME);
    requestPayload["query"].put("orderBy", "_rev");

    Logger::logCallRequest(getClassName(), __FUNCTION__, method, requestPayload);
    LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onFind,
        nullptr,
        nullptr,
        nullptr
    );
}

bool DB8::onPutKind(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = pbnjson::JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    if (responsePayload.isNull())
        return true;

    bool returnValue = false;
    string errorText;

    JValueUtil::getValue(responsePayload, "returnValue", returnValue);
    JValueUtil::getValue(responsePayload, "errorText", errorText);

    if (!returnValue) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, errorText);
        return true;
    } else {
        getInstance().putPermissions();
    }
    return true;
}

void DB8::putKind()
{
    static string method = string("luna://") + getName() + string("/putKind");

    JValue requestPayload = SAMConf::getInstance().getDBSchema();
    Logger::logCallRequest(getClassName(), __FUNCTION__, method, requestPayload);
    LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onPutKind,
        nullptr,
        nullptr,
        nullptr
    );
}

bool DB8::onPutPermissions(LSHandle* sh, LSMessage* message, void* context)
{
    Message response(message);
    JValue responsePayload = JDomParser::fromString(response.getPayload());
    Logger::logCallResponse(getInstance().getClassName(), __FUNCTION__, response, responsePayload);

    bool returnValue = false;
    string errorText;

    JValueUtil::getValue(responsePayload, "returnValue", returnValue);
    JValueUtil::getValue(responsePayload, "errorText", errorText);

    if (!returnValue) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, errorText);
        return true;
    } else {
        getInstance().find();
    }
    return true;
}

void DB8::putPermissions()
{
    static string method = string("luna://") + getName() + string("/putPermissions");

    JValue requestPayload = SAMConf::getInstance().getDBPermission();
    Logger::logCallRequest(getClassName(), __FUNCTION__, method, requestPayload);
    LSCallOneReply(
        ApplicationManager::getInstance().get(),
        method.c_str(),
        requestPayload.stringify().c_str(),
        onPutPermissions,
        nullptr,
        nullptr,
        nullptr
    );
}
