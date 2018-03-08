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

#include "core/bus/lifecycle_luna_adapter.h"

#include <boost/bind.hpp>

#include "core/base/logging.h"
#include "core/bus/appmgr_service.h"
#include "core/bus/lunaservice_api.h"
#include "core/lifecycle/app_life_manager.h"
#include "core/package/application_manager.h"
#include "core/package/mime_system.h"

#define SUBSKEY_RUNNING              "running"
#define SUBSKEY_DEV_RUNNING          "dev_running"
#define SUBSKEY_FOREGROUND_INFO      "foregroundAppInfo"
#define SUBSKEY_FOREGROUND_INFO_EX   "foregroundAppInfoEx"
#define SUBSKEY_GET_APP_LIFE_EVENTS  "getAppLifeEvents"
#define SUBSKEY_GET_APP_LIFE_STATUS  "getAppLifeStatus"
#define SUBSKEY_ON_LAUNCH            "onLaunch"

LifeCycleLunaAdapter::LifeCycleLunaAdapter() {
}

LifeCycleLunaAdapter::~LifeCycleLunaAdapter() {
}

void LifeCycleLunaAdapter::Init() {
  InitLunaApiHandler();

  AppMgrService::instance().signalOnServiceReady.connect(
      std::bind(&LifeCycleLunaAdapter::OnReady, this));

  ApplicationManager::instance().appScanner().signalAppScanFinished.connect(
      boost::bind(&LifeCycleLunaAdapter::OnScanFinished, this, _1, _2));

  AppLifeManager::instance().signal_foreground_app_changed.connect(
      boost::bind(&LifeCycleLunaAdapter::OnForegroundAppChanged, this, _1));

  AppLifeManager::instance().signal_foreground_extra_info_changed.connect(
      boost::bind(&LifeCycleLunaAdapter::OnExtraForegroundInfoChanged, this, _1));

  AppLifeManager::instance().signal_lifecycle_event.connect(
      boost::bind(&LifeCycleLunaAdapter::OnLifeCycleEventGenarated, this, _1));
}

void LifeCycleLunaAdapter::InitLunaApiHandler() {
  // general category (/)
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_LAUNCH,
      "applicationManager.launch",
      boost::bind(&LifeCycleLunaAdapter::RequestController, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_OPEN,
      "applicationManager.open",
      boost::bind(&LifeCycleLunaAdapter::RequestController, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_PAUSE, "",
      boost::bind(&LifeCycleLunaAdapter::Pause, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_CLOSE_BY_APPID,
      "applicationManager.closeByAppId",
      boost::bind(&LifeCycleLunaAdapter::CloseByAppId, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_CLOSE_ALL_APPS, "",
      boost::bind(&LifeCycleLunaAdapter::CloseAllApps, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_RUNNING,
      "applicationManager.running",
      boost::bind(&LifeCycleLunaAdapter::Running, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_CHANGE_RUNNING_APPID, "",
      boost::bind(&LifeCycleLunaAdapter::RequestController, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_GET_APP_LIFE_EVENTS, "",
      boost::bind(&LifeCycleLunaAdapter::GetAppLifeEvents, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_GET_APP_LIFE_STATUS, "",
      boost::bind(&LifeCycleLunaAdapter::GetAppLifeStatus, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_GET_FOREGROUND_APPINFO,
      "applicationManager.getForegroundAppInfo",
      boost::bind(&LifeCycleLunaAdapter::GetForegroundAppInfo, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_LOCK_APP,
      "applicationManager.lockApp",
      boost::bind(&LifeCycleLunaAdapter::LockApp, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_REGISTER_APP, "",
      boost::bind(&LifeCycleLunaAdapter::RegisterApp, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_REGISTER_NATIVE_APP, "",
      boost::bind(&LifeCycleLunaAdapter::RegisterNativeApp, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_NOTIFY_ALERT_CLOSED, "",
      boost::bind(&LifeCycleLunaAdapter::NotifyAlertClosed, this, _1));

  // dev category
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_DEV, API_CLOSE_BY_APPID,
      "applicationManager.closeByAppId",
      boost::bind(&LifeCycleLunaAdapter::CloseByAppIdForDev, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_DEV, API_RUNNING,
      "applicationManager.running",
      boost::bind(&LifeCycleLunaAdapter::RunningForDev, this, _1));

  // deperecated api
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_CLOSE, "",
      boost::bind(&LifeCycleLunaAdapter::Close, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_NOTIFY_SPLASH_TIMEOUT, "",
      boost::bind(&LifeCycleLunaAdapter::NotifySplashTimeout, this, _1));
  AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_ON_LAUNCH, "",
      boost::bind(&LifeCycleLunaAdapter::OnLaunch, this, _1));
}

void LifeCycleLunaAdapter::RequestController(LunaTaskPtr task) {
 if(API_LAUNCH == task->method()) {
    std::string app_id = task->jmsg()["id"].asString();
    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if (app_desc == NULL && !AppMgrService::instance().IsServiceReady()) {
      LOG_INFO(MSGID_API_REQUEST, 4, PMLOGKS("category", task->category().c_str()),
                                     PMLOGKS("method", task->method().c_str()),
                                     PMLOGKS("status", "pending"),
                                     PMLOGKS("caller", task->caller().c_str()),
                                     "received message, but will handle later");
      pending_tasks_on_ready_.push_back(task);
      return;
    }
  } else {
    if (!AppMgrService::instance().IsServiceReady() || ApplicationManager::instance().appScanner().isRunning()) {
      pending_tasks_on_scanner_.push_back(task);
      return;
    }
  }

  HandleRequest(task);
}

void LifeCycleLunaAdapter::OnReady() {
  auto it = pending_tasks_on_ready_.begin();
  while(it != pending_tasks_on_ready_.end()) {
    HandleRequest(*it);
    it = pending_tasks_on_ready_.erase(it);
  }
}

void LifeCycleLunaAdapter::OnScanFinished(ScanMode mode, const AppDescMaps& scannced_apps) {
  auto it = pending_tasks_on_scanner_.begin();
  while(it != pending_tasks_on_scanner_.end()) {
    HandleRequest(*it);
    it = pending_tasks_on_scanner_.erase(it);
  }
}

void LifeCycleLunaAdapter::HandleRequest(LunaTaskPtr task) {
  if (API_CATEGORY_GENERAL == task->category()) {
    if (API_LAUNCH == task->method()) Launch(task);
    else if (API_OPEN == task->method()) Open(task);
    else if (API_CHANGE_RUNNING_APPID == task->method()) ChangeRunningAppId(task);
  }
}

void LifeCycleLunaAdapter::Launch(LunaTaskPtr task) {
  const pbnjson::JValue& jmsg = task->jmsg();

  std::string id = jmsg["id"].asString();
  if(id.length() == 0) {
    pbnjson::JValue payload = pbnjson::Object();
    task->ReplyResultWithError(API_ERR_CODE_GENERAL, "App ID is not specified");
    return;
  }

  std::string launch_mode = "normal";
  if(jmsg.hasKey("params") && jmsg["params"].hasKey("reason"))
    launch_mode = jmsg["params"]["reason"].asString();

  LOG_NORMAL(NLID_APP_LAUNCH_BEGIN, 3, PMLOGKS("app_id", id.c_str()),
      PMLOGKS("caller_id", task->caller().c_str()),
      PMLOGKS("mode", launch_mode.c_str()), "");

  LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(LifeCycleTaskType::Launch, task);
  lifecycle_task->SetAppId(id);
  AppLifeManager::instance().Launch(lifecycle_task);
}


// TODO: open api codes might need to be restructured.
void LifeCycleLunaAdapter::Open(LunaTaskPtr task) {
  pbnjson::JValue jmsg = task->jmsg();

  std::string app_id          = jmsg["id"].asString();
  std::string targetUri       = jmsg["target"].asString();
  std::string strMime         = jmsg["mime"].asString();
  std::string ovrHandlerAppId = jmsg["overrideHandlerAppId"].asString();
  std::string trueFileName    = jmsg["fileName"].asString();

  if (!jmsg.hasKey("params")) {
    jmsg.put("params", pbnjson::Object());
  }

  if (!targetUri.empty()) {
    jmsg["params"].put("target", targetUri);
  }

  ResourceHandler resourceHandler;
  RedirectHandler redirectHandler;

  if (!app_id.empty()) {
    LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(LifeCycleTaskType::Launch, task);
    lifecycle_task->SetAppId(app_id);
    AppLifeManager::instance().Launch(lifecycle_task);
    return;
  }

  // Previously, if targetUri was empty, we returned error
  // We just redirect app with parameters now
  // TODO: We need new definition to redirect without URI scheme (e.g: open category)
  if (targetUri.empty()) {
    resourceHandler = MimeSystemImpl::instance().getActiveHandlerForResource(strMime);
    if (resourceHandler.valid()) {
      app_id = resourceHandler.appId();

      LOG_NORMAL(NLID_APP_LAUNCH_BEGIN, 3, PMLOGKS("app_id", app_id.c_str()),
                                            PMLOGKS("caller_id", task->caller().c_str()),
                                            PMLOGKS("mode", "open_mime"), "");

      LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(LifeCycleTaskType::Launch, task);
      lifecycle_task->SetAppId(app_id);
      AppLifeManager::instance().Launch(lifecycle_task);
      return;
    }
  }

  // We have a resource URL to open. Try to find the correct application to launch
  if (!trueFileName.empty()) {
    jmsg["params"].put("fileName", trueFileName);
  }

  //true argument limits to only redirects (and not command (scheme) handlers)
  redirectHandler = MimeSystemImpl::instance().getActiveHandlerForRedirect(targetUri,false,true);
  if (redirectHandler.valid()) {
    app_id = redirectHandler.appId();

    // A redirected URL implies streaming.
    LOG_DEBUG("Launch redirected for app id %s, target URI %s", app_id.c_str(), targetUri.c_str() );

    LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(LifeCycleTaskType::Launch, task);
    lifecycle_task->SetAppId(app_id);
    AppLifeManager::instance().Launch(lifecycle_task);
    return;
  }
  LOG_DEBUG("No redirect handler");

  std::string strExtn;
  if (strMime.empty()) {
    getExtensionFromUrl(targetUri,strExtn);
    //map to mime type
    MimeSystemImpl::instance().getMimeTypeByExtension(strExtn,strMime);
  }

  resourceHandler = MimeSystemImpl::instance().getActiveHandlerForResource(strMime);
  if (resourceHandler.valid()) {
    if (resourceHandler.stream()) {
      // In the appinfo.json file of the application, the "streamable" argument currently means the following:
      // "Don't try to download the resource...just launch the given app and pass { "target":<passed in URL/URI> } as the parameter
      if (ovrHandlerAppId.empty())
        app_id = resourceHandler.appId();
      else
        app_id = ovrHandlerAppId;

      jmsg["params"].put("mimeType",strMime);

      LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(LifeCycleTaskType::Launch, task);
      lifecycle_task->SetAppId(app_id);
      AppLifeManager::instance().Launch(lifecycle_task);
      return;
    } else {
      // We got a local file in URL
      app_id = resourceHandler.appId();

      LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(LifeCycleTaskType::Launch, task);
      lifecycle_task->SetAppId(app_id);
      AppLifeManager::instance().Launch(lifecycle_task);
      return;
    }
  }

  redirectHandler = MimeSystemImpl::instance().getActiveHandlerForRedirect(targetUri, false, false);
  //false parameter allows searching for command (scheme) handlers in the mime system
  // (It will only find command handlers at this point because redirect handlers were already
  //    searched for earlier)
  if(redirectHandler.valid()) {
    LOG_DEBUG("Command handler detected");
    app_id = redirectHandler.appId();

    LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(LifeCycleTaskType::Launch, task);
    lifecycle_task->SetAppId(app_id);
    AppLifeManager::instance().Launch(lifecycle_task);
    return;
  }

  // exhausted all choices
  task->ReplyResultWithError(HANDLER_ERR_NO_HANDLE, "No handler for "+targetUri);

  LOG_WARNING(MSGID_API_REQUEST_ERR, 3, PMLOGKS("request", "open"),
                                        PMLOGKFV("errorCode", "%d", (int)HANDLER_ERR_NO_HANDLE),
                                        PMLOGKS("errorText", std::string("No handler for " + targetUri).c_str()), "");

  return;
}

void LifeCycleLunaAdapter::Pause(LunaTaskPtr task) {
  const pbnjson::JValue& jmsg = task->jmsg();

  std::string id = jmsg["id"].asString();
  if(id.length() == 0) {
    pbnjson::JValue payload = pbnjson::Object();
    task->ReplyResultWithError(API_ERR_CODE_GENERAL, "App ID is not specified");
    return;
  }

  LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(LifeCycleTaskType::Pause, task);
  lifecycle_task->SetAppId(id);
  AppLifeManager::instance().Pause(lifecycle_task);
}

void LifeCycleLunaAdapter::CloseByAppId(LunaTaskPtr task) {
  LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(LifeCycleTaskType::Close, task);
  AppLifeManager::instance().Close(lifecycle_task);
}

void LifeCycleLunaAdapter::CloseByAppIdForDev(LunaTaskPtr task) {
  const pbnjson::JValue& jmsg = task->jmsg();
  std::string app_id = jmsg["id"].asString();
  AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);

  if (AppTypeByDir::Dev != app_desc->getTypeByDir()) {
    LOG_WARNING(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", app_id.c_str()), "only dev apps should be closed in devmode");
    task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Only Dev app should be closed using /dev category_API");
    return;
  }

  LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(LifeCycleTaskType::Close, task);
  AppLifeManager::instance().Close(lifecycle_task);
}

void LifeCycleLunaAdapter::CloseAllApps(LunaTaskPtr task) {
  LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(LifeCycleTaskType::CloseAll, task);
  AppLifeManager::instance().CloseAll(lifecycle_task);
  task->ReplyResult();
}

void LifeCycleLunaAdapter::Running(LunaTaskPtr task) {
  pbnjson::JValue payload = pbnjson::Object();
  pbnjson::JValue running_list = pbnjson::Array();
  AppInfoManager::instance().get_running_list(running_list);

  payload.put("returnValue", true);
  payload.put("running", running_list);
  if(LSMessageIsSubscription(task->lsmsg())) {
      payload.put("subscribed",
          LSSubscriptionAdd(task->lshandle(), SUBSKEY_RUNNING, task->lsmsg(), NULL));
  }

  task->ReplyResult(payload);
}

void LifeCycleLunaAdapter::RunningForDev(LunaTaskPtr task) {
  pbnjson::JValue payload = pbnjson::Object();
  pbnjson::JValue running_list = pbnjson::Array();
  AppInfoManager::instance().get_running_list(running_list, true);

  payload.put("returnValue", true);
  payload.put("running", running_list);
  if(LSMessageIsSubscription(task->lsmsg())) {
      payload.put("subscribed",
          LSSubscriptionAdd(task->lshandle(), SUBSKEY_DEV_RUNNING, task->lsmsg(), NULL));
  }

  task->ReplyResult(payload);
}

void LifeCycleLunaAdapter::ChangeRunningAppId(LunaTaskPtr task) {
  const pbnjson::JValue& jmsg = task->jmsg();

  pbnjson::JValue response = pbnjson::Object();
  std::string caller_id = task->caller();
  std::string target_id = jmsg["id"].asString();
  ErrorInfo err_info;

  if (caller_id != "com.webos.app.inputcommon") {
    task->ReplyResultWithError(PERMISSION_DENIED, "only 'com.webos.app.inputcommon' can call this API");
    return;
  }


  if (NULL == ApplicationManager::instance().getAppById(target_id)) {
    task->ReplyResultWithError(APP_ERR_INVALID_APPID, "Cannot find target appId.");
    return;
  }

  if (!AppLifeManager::instance().change_running_app_id(caller_id, target_id, err_info)) {
    task->ReplyResultWithError(err_info.errorCode, err_info.errorText);
    return;
  }

  task->ReplyResult();
}

void LifeCycleLunaAdapter::GetAppLifeEvents(LunaTaskPtr task) {
  pbnjson::JValue payload = pbnjson::Object();

  if (LSMessageIsSubscription(task->lsmsg())) {
    if (!LSSubscriptionAdd(task->lshandle(), SUBSKEY_GET_APP_LIFE_EVENTS, task->lsmsg(), NULL)) {
      task->SetError(API_ERR_CODE_GENERAL, "Subscription failed");
      payload.put("subscribed", false);
    } else {
      payload.put("subscribed", true);
    }
  } else {
    task->SetError(API_ERR_CODE_GENERAL, "subscription is required");
    payload.put("subscribed", false);
  }

  task->ReplyResult(payload);
}

void LifeCycleLunaAdapter::GetAppLifeStatus(LunaTaskPtr task) {
  pbnjson::JValue payload = pbnjson::Object();

  if(LSMessageIsSubscription(task->lsmsg())) {
    if(!LSSubscriptionAdd(task->lshandle(), SUBSKEY_GET_APP_LIFE_STATUS, task->lsmsg(), NULL)) {
      task->SetError(API_ERR_CODE_GENERAL, "Subscription failed");
      payload.put("subscribed", false);
    } else {
      payload.put("subscribed", true);
    }
  } else {
    task->SetError(API_ERR_CODE_GENERAL, "subscription is required");
    payload.put("subscribed", false);
  }

  task->ReplyResult(payload);
}

void LifeCycleLunaAdapter::GetForegroundAppInfo(LunaTaskPtr task) {
  const pbnjson::JValue& jmsg = task->jmsg();

  pbnjson::JValue payload = pbnjson::Object();
  payload.put("returnValue", true);

  if (jmsg["extraInfo"].asBool() == true) {
    payload.put("foregroundAppInfo", AppInfoManager::instance().get_json_foreground_info());
    if (LSMessageIsSubscription(task->lsmsg())) {
      payload.put("subscribed",
          LSSubscriptionAdd(task->lshandle(), SUBSKEY_FOREGROUND_INFO_EX, task->lsmsg(), NULL));
    }
  } else {
    payload.put("appId", AppInfoManager::instance().get_current_foreground_app_id());
    payload.put("windowId", "");
    payload.put("processId", "");
    if (LSMessageIsSubscription(task->lsmsg())) {
      payload.put("subscribed",
          LSSubscriptionAdd(task->lshandle(), SUBSKEY_FOREGROUND_INFO, task->lsmsg(), NULL));
    }
  }

  task->ReplyResult(payload);
}

void LifeCycleLunaAdapter::LockApp(LunaTaskPtr task) {
  const pbnjson::JValue& jmsg = task->jmsg();

  std::string app_id = jmsg["id"].asString();
  bool lock = jmsg["lock"].asBool();
  std::string error_text;

  // TODO: move this property into app info manager
  (void) ApplicationManager::instance().LockAppForUpdate(app_id, lock , error_text);

  if (!error_text.empty()) {
    task->ReplyResultWithError(API_ERR_CODE_GENERAL, error_text);
    return;
  }

  pbnjson::JValue payload = pbnjson::Object();
  payload.put("id", app_id);
  payload.put("locked", lock);

  task->ReplyResult(payload);
}

void LifeCycleLunaAdapter::RegisterApp(LunaTaskPtr task) {
  if(0 == task->caller().length()) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
        PMLOGKS("reason", "empty_app_id"),
        PMLOGKS("where", "register_app"), "");
    task->ReplyResultWithError(API_ERR_CODE_GENERAL, "cannot find caller id");
    return;
  }

  std::string error_text;
  AppLifeManager::instance().RegisterApp(task->caller(), task->lsmsg(), error_text);

  if(!error_text.empty()) {
    task->ReplyResultWithError(API_ERR_CODE_GENERAL, error_text);
    return;
  }
}

void LifeCycleLunaAdapter::RegisterNativeApp(LunaTaskPtr task) {
  if(0 == task->caller().length()) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
        PMLOGKS("reason", "empty_app_id"),
        PMLOGKS("where", "register_native_app"), "");
    task->ReplyResultWithError(API_ERR_CODE_GENERAL, "cannot find caller id");
    return;
  }

  std::string error_text;
  AppLifeManager::instance().connect_nativeapp(task->caller(), task->lsmsg(), error_text);

  if(!error_text.empty()) {
    task->ReplyResultWithError(API_ERR_CODE_GENERAL, error_text);
    return;
  }
}

void LifeCycleLunaAdapter::NotifyAlertClosed(LunaTaskPtr task) {
  AppLifeManager::instance().handle_bridged_launch_request(task->jmsg());
  task->ReplyResult();
}

// TODO: make below APIs retired
void LifeCycleLunaAdapter::Close(LunaTaskPtr task) {
  LifeCycleTaskPtr lifecycle_task = std::make_shared<LifeCycleTask>(LifeCycleTaskType::Close, task);
  AppLifeManager::instance().CloseByPid(lifecycle_task);
}

void LifeCycleLunaAdapter::NotifySplashTimeout(LunaTaskPtr task) {
  task->ReplyResult();
}

void LifeCycleLunaAdapter::OnLaunch(LunaTaskPtr task) {

  pbnjson::JValue payload = pbnjson::Object();
  if(LSMessageIsSubscription(task->lsmsg())) {
    if(!LSSubscriptionAdd(task->lshandle(), SUBSKEY_ON_LAUNCH, task->lsmsg(), NULL)) {
      task->SetError(API_ERR_CODE_GENERAL, "Subscription failed");
      payload.put("subscribed", false);
    } else {
      payload.put("subscribed", true);
    }
  } else {
    task->SetError(API_ERR_CODE_GENERAL, "subscription is required");
    payload.put("subscribed", false);
  }

  task->ReplyResult(payload);
}

// subscription adapter to reply
void LifeCycleLunaAdapter::OnForegroundAppChanged(const std::string& app_id) {

  pbnjson::JValue payload = pbnjson::Object();
  payload.put("returnValue", true);
  payload.put("subscribed", true);
  payload.put("appId",app_id);
  payload.put("windowId", "");
  payload.put("processId", "");

  LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2, PMLOGKS("skey", SUBSKEY_FOREGROUND_INFO),
                                        PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()),
                                        "");

  if (!LSSubscriptionReply(AppMgrService::instance().ServiceHandle(),
      SUBSKEY_FOREGROUND_INFO, JUtil::jsonToString(payload).c_str(), NULL)) {
    LOG_ERROR(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "subscriptionreply"),
                                   PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()),
                                   PMLOGKS("where", __FUNCTION__), "");
    return;
  }
}

void LifeCycleLunaAdapter::OnExtraForegroundInfoChanged(const pbnjson::JValue& foreground_info) {
  pbnjson::JValue payload = pbnjson::Object();
  payload.put("returnValue", true);
  payload.put("subscribed", true);
  payload.put("foregroundAppInfo", foreground_info);

  LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2, PMLOGKS("skey", SUBSKEY_FOREGROUND_INFO_EX),
                                        PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()),
                                        "");
  if (!LSSubscriptionReply(AppMgrService::instance().ServiceHandle(),
      SUBSKEY_FOREGROUND_INFO_EX, JUtil::jsonToString(payload).c_str(), NULL)) {
    LOG_ERROR(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "subscriptionreply"),
                                   PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()),
                                   PMLOGKS("where", __FUNCTION__), "");
    return;
  }
}

void LifeCycleLunaAdapter::OnLifeCycleEventGenarated(const pbnjson::JValue& event) {

  pbnjson::JValue payload = event.duplicate();
  payload.put("returnValue", true);

  LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2, PMLOGKS("skey", SUBSKEY_GET_APP_LIFE_EVENTS),
                                        PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()),
                                        "");
  if (!LSSubscriptionReply(AppMgrService::instance().ServiceHandle(),
      SUBSKEY_GET_APP_LIFE_EVENTS, JUtil::jsonToString(payload).c_str(), NULL)) {
    LOG_ERROR(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "subscriptionreply"),
                                   PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()),
                                   PMLOGKS("where", __FUNCTION__), "");
    return;
  }
}
