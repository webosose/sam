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

#include <bus/service/SchemaChecker.h>
#include "ApplicationManager.h"

SchemaChecker::SchemaChecker()
{
    m_APISchemaFiles[ApplicationManager::API_LAUNCH] = "applicationManager.launch";
    m_APISchemaFiles[ApplicationManager::API_PAUSE] = "";
    m_APISchemaFiles[ApplicationManager::API_CLOSE_BY_APPID] = "applicationManager.closeByAppId";
    m_APISchemaFiles[ApplicationManager::API_RUNNING] = "applicationManager.running";
    m_APISchemaFiles[ApplicationManager::API_GET_APP_LIFE_EVENTS] = "";
    m_APISchemaFiles[ApplicationManager::API_GET_APP_LIFE_STATUS] = "";
    m_APISchemaFiles[ApplicationManager::API_GET_FOREGROUND_APPINFO] = "applicationManager.getForegroundAppInfo";
    m_APISchemaFiles[ApplicationManager::API_LOCK_APP] = "applicationManager.lockApp";
    m_APISchemaFiles[ApplicationManager::API_REGISTER_APP] = "";
    m_APISchemaFiles[ApplicationManager::API_LIST_APPS] = "applicationManager.listApps";
    m_APISchemaFiles[ApplicationManager::API_GET_APP_STATUS] = "applicationManager.getAppStatus";
    m_APISchemaFiles[ApplicationManager::API_GET_APP_INFO] = "applicationManager.getAppInfo";
    m_APISchemaFiles[ApplicationManager::API_GET_APP_BASE_PATH] = "applicationManager.getAppBasePath";
    m_APISchemaFiles[ApplicationManager::API_ADD_LAUNCHPOINT] = "applicationManager.addLaunchPoint";
    m_APISchemaFiles[ApplicationManager::API_UPDATE_LAUNCHPOINT] = "applicationManager.updateLaunchPoint";
    m_APISchemaFiles[ApplicationManager::API_REMOVE_LAUNCHPOINT] = "applicationManager.removeLaunchPoint";
    m_APISchemaFiles[ApplicationManager::API_MOVE_LAUNCHPOINT] = "applicationManager.moveLaunchPoint";
    m_APISchemaFiles[ApplicationManager::API_LIST_LAUNCHPOINTS] = "applicationManager.listLaunchPoints";
}

SchemaChecker::~SchemaChecker()
{
    m_APISchemaFiles.clear();
}

bool SchemaChecker::checkRequest(Message& request)
{
    string method = request.getMethod();
    if (m_APISchemaFiles.find(method) == m_APISchemaFiles.end())
        return true;

    if (m_APISchemaFiles[method].empty())
        return true;

    JValue requestPayload = JDomParser::fromString(request.getPayload(), JSchema::fromFile(m_APISchemaFiles[method].c_str()));
    if (requestPayload.isNull())
        return false;
    return true;
}
