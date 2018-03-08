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

#include "core/bus/appmgr_service.h"

#include <pbnjson.hpp>
#include <string>
#include <vector>

#include "core/base/lsutils.h"
#include "core/base/logging.h"
#include "core/bus/lunaservice_api.h"
#include "core/setting/settings.h"

namespace {
static std::map<std::string, std::string> api_schema_map_;
}

AppMgrService::AppMgrService()
    : ServiceBase("com.webos.applicationManager"),
      service_ready_(false) {
}

AppMgrService::~AppMgrService() {
}

bool AppMgrService::Attach(GMainLoop* gml) {

  AddCompatibleNames({"com.webos.service.applicationManager", "com.webos.service.applicationmanager"});

  if (!ServiceBase::Attach(gml)) {
    Detach();
    return false;
  }

  lifecycle_luna_adapter_.Init();
  package_luna_adapter_.Init();
  launchpoint_luna_adapter_.Init();
  return true;
}

void AppMgrService::Detach() {
  api_schema_map_.clear();
  api_handler_map_.clear();
  ServiceBase::Detach();
}

void AppMgrService::get_categories(std::vector<std::string>& categories) const {
  categories.push_back(API_CATEGORY_GENERAL);
}

LSMethod* AppMgrService::get_methods(std::string category) const {
  if (category == API_CATEGORY_GENERAL) {
    static LSMethod s_methods[]  = {
      // core: lifecycle
      { API_LAUNCH,                 AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_OPEN,                   AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_PAUSE,                  AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_CLOSE_BY_APPID,         AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_CLOSE_ALL_APPS,         AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_CLOSE,                  AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_RUNNING,                AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_CHANGE_RUNNING_APPID,   AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_GET_APP_LIFE_EVENTS,    AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_GET_APP_LIFE_STATUS,    AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_GET_FOREGROUND_APPINFO, AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_LOCK_APP,               AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_REGISTER_APP,           AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_REGISTER_NATIVE_APP,    AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_NOTIFY_ALERT_CLOSED,    AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_NOTIFY_SPLASH_TIMEOUT,  AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_ON_LAUNCH,              AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },

      // core: package
      { API_LIST_APPS,              AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_GET_APP_STATUS,         AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_GET_APP_INFO,           AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_GET_APP_BASE_PATH,      AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_LAUNCH_VIRTUAL_APP,     AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_ADD_VIRTUAL_APP,        AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_REMOVE_VIRTUAL_APP,     AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },

      { API_REGISTER_VERBS_FOR_REDIRECT,        AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_REGISTER_VERBS_FOR_RESOURCE,        AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_GET_HANDLER_FOR_EXTENSION,          AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_LIST_EXTENSION_MAP,                 AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_MIME_TYPE_FOR_EXTENSION,            AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_GET_HANDLER_FOR_MIME_TYPE,          AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_GET_HANDLER_FOR_MIME_TYPE_BY_VERB,  AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_GET_HANDLER_FOR_URL,                AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_GET_HANDLER_FOR_URL_BY_VERB,        AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_LIST_ALL_HANDLERS_FOR_MIME,         AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_LIST_ALL_HANDLERS_FOR_MIME_BY_VERB, AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_LIST_ALL_HANDLERS_FOR_URL,          AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_LIST_ALL_HANDLERS_FOR_URL_BY_VERB,  AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_LIST_ALL_HANDLERS_FOR_URL_PATTERN,  AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_LIST_ALL_HANDLERS_FOR_MULTIPLE_MIME,AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_LIST_ALL_HANDLERS_FOR_MULTIPLE_URL_PATTERN,AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },

      // core: launchpoint
      { API_ADD_LAUNCHPOINT,        AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_UPDATE_LAUNCHPOINT,     AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_REMOVE_LAUNCHPOINT,     AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_MOVE_LAUNCHPOINT,       AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_LIST_LAUNCHPOINTS,      AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      // TODO: this is API for package. Now make it deprecated temporarily.
      { API_SEARCH_APPS,            AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { 0, 0 , LUNA_METHOD_FLAGS_NONE }
    };
    return s_methods;
  }
  else if (category == API_CATEGORY_DEV) {
    static LSMethod s_methods[] =
    {
      { API_CLOSE_BY_APPID,         AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_LIST_APPS,              AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { API_RUNNING,                AppMgrService::OnApiCalled, LUNA_METHOD_FLAGS_NONE },
      { 0, 0 , LUNA_METHOD_FLAGS_NONE }
    };
    return s_methods;
  }
  return NULL;
}

void AppMgrService::PostAttach() {
  // TODO: Fix possible timing issue
  //       This lazy registeration causes timing issue,
  //       because we append new category after mainloop is already attached
  LSErrorSafe lse;
  if (SettingsImpl::instance().isDevMode) {
    if (!LSRegisterCategory(ServiceHandle(), API_CATEGORY_DEV,
        get_methods(API_CATEGORY_DEV), NULL, NULL, &lse)) {
      LOG_ERROR(MSGID_SRVC_CATEGORY_FAIL, 2, PMLOGKS("SERVICE", ServiceName().c_str()),
                                             PMLOGKS("ERROR", lse.message), "failed to register dev api");
      return;
    }

    if (!LSCategorySetData(ServiceHandle(), API_CATEGORY_DEV, this, &lse)) {
        LOG_ERROR(MSGID_SRVC_CATEGORY_FAIL, 2, PMLOGKS("SERVICE", ServiceName().c_str()),
                                               PMLOGKS("ERROR", lse.message), "failed to set category data");
      return;
    }
  }
}

void AppMgrService::OnServiceReady() {
  PostAttach();
  signalOnServiceReady();
}

void AppMgrService::RegisterApiHandler(const std::string& category, const std::string& method,
    const std::string& schema, LunaApiHandler handler) {
  std::string api = category + method;
  if(!schema.empty()) api_schema_map_[api] = schema;
  api_handler_map_[api] = handler;
}

bool AppMgrService::OnApiCalled(LSHandle* lshandle, LSMessage* lsmsg, void* ctx)
{
  AppMgrService& service = AppMgrService::instance();
  LSHandle* service_handle = service.ServiceHandle();

  std::string category    = GetCategoryFromMessage(lsmsg);
  std::string method      = GetMethodFromMessage(lsmsg);
  std::string caller_id   = GetCallerFromMessage(lsmsg);
  std::string api         = category+method;
  std::string schema_path = api_schema_map_.find(api) != api_schema_map_.end() ? api_schema_map_[api] : "";
  LunaApiHandler handler;

  LOG_INFO(MSGID_API_REQUEST, 4, PMLOGKS("category", category.c_str()),
                                 PMLOGKS("method", method.c_str()),
                                 PMLOGKS("caller", caller_id.c_str()),
                                 PMLOGKS("status", "received_request"), "");
  LOG_DEBUG("params:%s", LSMessageGetPayload(lsmsg));

  if (service.api_handler_map_.find(api) != service.api_handler_map_.end())
    handler = service.api_handler_map_[api];

  JUtil::Error error;
  pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), schema_path, &error);

  LunaTaskPtr task = std::make_shared<LunaTask>(service_handle, category, method, caller_id, lsmsg, jmsg);
  if (!task) {
    LOG_WARNING(MSGID_API_REQUEST, 1, PMLOGKS("status", "bad_alloc_on_new_request"), "");
    (void) LSMessageRespond(lsmsg, "{\"returnValue\":false, \"errorText\":\"memory alloc fail\", \"errorCode\":1}", NULL);
  }

  if (task->jmsg().isNull()) {
      LOG_WARNING(MSGID_API_REQUEST, 1, PMLOGKS("status", "invalid_parameter"),
                                        "err: %s, schema: %s, params: %s",
                                        error.detail().c_str(), schema_path.c_str(), LSMessageGetPayload(lsmsg));
      task->ReplyResultWithError(API_ERR_CODE_INVALID_PAYLOAD, "invalid parameters");
      return false;
  }

  if (handler) handler(task);
  else {
    LOG_WARNING(MSGID_API_DEPRECATED, 3, PMLOGKS("category", category.c_str()),
                                         PMLOGKS("method", method.c_str()),
                                         PMLOGKS("caller", caller_id.c_str()),
                                         "this method is no more supported");
    task->ReplyResultWithError(API_ERR_CODE_DEPRECATED, "deprecated method");
  }

  return true;
}
