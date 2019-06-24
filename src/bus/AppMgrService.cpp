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

#include <bus/AppMgrService.h>
#include <bus/LunaserviceAPI.h>
#include <pbnjson.hpp>
#include <setting/Settings.h>
#include <util/Logging.h>
#include <util/LSUtils.h>
#include <string>
#include <vector>


namespace {
static std::map<std::string, std::string> api_schema_map_;
}

AppMgrService::AppMgrService()
    : ServiceBase("com.webos.applicationManager"),
      m_serviceReady(false)
{
}

AppMgrService::~AppMgrService()
{
}

bool AppMgrService::attach(GMainLoop* gml)
{
    addCompatibleNames(
        { "com.webos.service.applicationManager", "com.webos.service.applicationmanager" }
    );

    if (!ServiceBase::attach(gml)) {
        detach();
        return false;
    }

    m_lifecycleLunaAdapter.init();
    m_packageLunaAdapter.init();
    m_launchpointLunaAdapter.init();
    return true;
}

void AppMgrService::detach()
{
    api_schema_map_.clear();
    m_apiHandlerMap.clear();
    ServiceBase::detach();
}

void AppMgrService::getCategories(std::vector<std::string>& categories) const
{
    categories.push_back(API_CATEGORY_GENERAL);
}

LSMethod* AppMgrService::getMethods(std::string category) const
{
    if (category == API_CATEGORY_GENERAL) {
        static LSMethod s_methods[] = {
            // core: lifecycle
            { API_LAUNCH, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_PAUSE, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_CLOSE_BY_APPID, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_RUNNING, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_GET_APP_LIFE_EVENTS, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_GET_APP_LIFE_STATUS, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_GET_FOREGROUND_APPINFO, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_LOCK_APP, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_REGISTER_APP, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },

            // core: package
            { API_LIST_APPS, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_GET_APP_STATUS, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_GET_APP_INFO, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_GET_APP_BASE_PATH, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },

            // core: launchpoint
            { API_ADD_LAUNCHPOINT, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_UPDATE_LAUNCHPOINT, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_REMOVE_LAUNCHPOINT, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_MOVE_LAUNCHPOINT, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_LIST_LAUNCHPOINTS, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { 0, 0, LUNA_METHOD_FLAGS_NONE }
        };
        return s_methods;
    } else if (category == API_CATEGORY_DEV) {
        static LSMethod s_methods[] = {
            { API_CLOSE_BY_APPID, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_LIST_APPS, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { API_RUNNING, AppMgrService::onApiCalled, LUNA_METHOD_FLAGS_NONE },
            { 0, 0, LUNA_METHOD_FLAGS_NONE }
        };
        return s_methods;
    }
    return NULL;
}

void AppMgrService::postAttach()
{
    // TODO: Fix possible timing issue
    //       This lazy registeration causes timing issue,
    //       because we append new category after mainloop is already attached
    LSErrorSafe lse;
    if (SettingsImpl::instance().isDevMode) {
        if (!LSRegisterCategory(serviceHandle(),
                                API_CATEGORY_DEV,
                                getMethods(API_CATEGORY_DEV),
                                NULL,
                                NULL,
                                &lse)) {
            LOG_ERROR(MSGID_SRVC_CATEGORY_FAIL, 2,
                      PMLOGKS("SERVICE", serviceName().c_str()),
                      PMLOGKS("ERROR", lse.message),
                      "failed to register dev api");
            return;
        }

        if (!LSCategorySetData(serviceHandle(), API_CATEGORY_DEV, this, &lse)) {
            LOG_ERROR(MSGID_SRVC_CATEGORY_FAIL, 2,
                      PMLOGKS("SERVICE", serviceName().c_str()),
                      PMLOGKS("ERROR", lse.message),
                      "failed to set category data");
            return;
        }
    }
}

void AppMgrService::onServiceReady()
{
    postAttach();
    signalOnServiceReady();
}

void AppMgrService::registerApiHandler(const std::string& category, const std::string& method, const std::string& schema, LunaApiHandler handler)
{
    std::string api = category + method;
    if (!schema.empty())
        api_schema_map_[api] = schema;
    m_apiHandlerMap[api] = handler;
}

bool AppMgrService::onApiCalled(LSHandle* lshandle, LSMessage* lsmsg, void* ctx)
{
    AppMgrService& service = AppMgrService::instance();
    LSHandle* service_handle = service.serviceHandle();

    std::string category = getCategoryFromMessage(lsmsg);
    std::string method = getMethodFromMessage(lsmsg);
    std::string caller_id = getCallerFromMessage(lsmsg);
    std::string api = category + method;
    std::string schema_path = api_schema_map_.find(api) != api_schema_map_.end() ? api_schema_map_[api] : "";
    LunaApiHandler handler;

    LOG_INFO(MSGID_API_REQUEST, 4,
             PMLOGKS("category", category.c_str()),
             PMLOGKS("method", method.c_str()),
             PMLOGKS("caller", caller_id.c_str()),
             PMLOGKS("status", "received_request"), "");
    LOG_DEBUG("params:%s", LSMessageGetPayload(lsmsg));

    if (service.m_apiHandlerMap.find(api) != service.m_apiHandlerMap.end())
        handler = service.m_apiHandlerMap[api];

    JUtil::Error error;
    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), schema_path, &error);

    LunaTaskPtr task = std::make_shared<LunaTask>(service_handle, category, method, caller_id, lsmsg, jmsg);
    if (!task) {
        LOG_WARNING(MSGID_API_REQUEST, 1, PMLOGKS("status", "bad_alloc_on_new_request"), "");
        (void) LSMessageRespond(lsmsg, "{\"returnValue\":false, \"errorText\":\"memory alloc fail\", \"errorCode\":1}", NULL);
    }

    if (task->jmsg().isNull()) {
        LOG_WARNING(MSGID_API_REQUEST, 1,
                    PMLOGKS("status", "invalid_parameter"),
                    "err: %s, schema: %s, params: %s", error.detail().c_str(), schema_path.c_str(), LSMessageGetPayload(lsmsg));
        task->replyResultWithError(API_ERR_CODE_INVALID_PAYLOAD, "invalid parameters");
        return false;
    }

    if (handler)
        handler(task);
    else {
        LOG_WARNING(MSGID_API_DEPRECATED, 3,
                    PMLOGKS("category", category.c_str()),
                    PMLOGKS("method", method.c_str()),
                    PMLOGKS("caller", caller_id.c_str()),
                    "this method is no more supported");
        task->replyResultWithError(API_ERR_CODE_DEPRECATED, "deprecated method");
    }

    return true;
}
