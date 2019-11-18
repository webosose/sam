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
#include "Environment.h"

SchemaChecker::SchemaChecker()
{
    m_APISchemaFiles[ApplicationManager::METHOD_LAUNCH] = "applicationManager.launch";
    m_APISchemaFiles[ApplicationManager::METHOD_PAUSE] = "";
    m_APISchemaFiles[ApplicationManager::METHOD_CLOSE_BY_APPID] = "applicationManager.closeByAppId";
    m_APISchemaFiles[ApplicationManager::METHOD_RUNNING] = "applicationManager.running";
    m_APISchemaFiles[ApplicationManager::METHOD_GET_APP_LIFE_EVENTS] = "";
    m_APISchemaFiles[ApplicationManager::METHOD_GET_APP_LIFE_STATUS] = "";
    m_APISchemaFiles[ApplicationManager::METHOD_GET_FOREGROUND_APPINFO] = "applicationManager.getForegroundAppInfo";
    m_APISchemaFiles[ApplicationManager::METHOD_LOCK_APP] = "applicationManager.lockApp";
    m_APISchemaFiles[ApplicationManager::METHOD_REGISTER_APP] = "";
    m_APISchemaFiles[ApplicationManager::METHOD_LIST_APPS] = "applicationManager.listApps";
    m_APISchemaFiles[ApplicationManager::METHOD_GET_APP_STATUS] = "applicationManager.getAppStatus";
    m_APISchemaFiles[ApplicationManager::METHOD_GET_APP_INFO] = "applicationManager.getAppInfo";
    m_APISchemaFiles[ApplicationManager::METHOD_GET_APP_BASE_PATH] = "applicationManager.getAppBasePath";
    m_APISchemaFiles[ApplicationManager::METHOD_ADD_LAUNCHPOINT] = "applicationManager.addLaunchPoint";
    m_APISchemaFiles[ApplicationManager::METHOD_UPDATE_LAUNCHPOINT] = "applicationManager.updateLaunchPoint";
    m_APISchemaFiles[ApplicationManager::METHOD_REMOVE_LAUNCHPOINT] = "applicationManager.removeLaunchPoint";
    m_APISchemaFiles[ApplicationManager::METHOD_MOVE_LAUNCHPOINT] = "applicationManager.moveLaunchPoint";
    m_APISchemaFiles[ApplicationManager::METHOD_LIST_LAUNCHPOINTS] = "applicationManager.listLaunchPoints";
}

SchemaChecker::~SchemaChecker()
{
    m_APISchemaFiles.clear();
}

JValue SchemaChecker::getRequestPayloadWithSchema(Message& request)
{
    string method = request.getMethod();
    JValue requestPayload;
    if (m_APISchemaFiles.find(method) == m_APISchemaFiles.end() || m_APISchemaFiles[method].empty()) {
        requestPayload = JDomParser::fromString(request.getPayload());
        return requestPayload;
    }

    string path = PATH_SAM_SCHEMAS + m_APISchemaFiles[method] + ".schema";
    requestPayload = JDomParser::fromString(request.getPayload(), JSchema::fromFile(path.c_str()));
    return requestPayload;
}
