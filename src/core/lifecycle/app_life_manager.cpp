// Copyright (c) 2012-2018 LG Electronics, Inc.
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

#include "core/lifecycle/app_life_manager.h"

#include <boost/signals2.hpp>
#include <unistd.h>

#include "core/base/jutil.h"
#include "core/base/logging.h"
#include "core/base/lsutils.h"
#include "core/base/utils.h"
#include "core/bus/appmgr_service.h"
#include "core/bus/lunaservice_api.h"
#include "core/module/subscriber_of_lsm.h"
#include "core/package/application_manager.h"
#include "core/setting/settings.h"

#define SAM_INTERNAL_ID  "com.webos.applicationManager"
#define SLEEP_TIME_TO_CLOSE_FULLSCREEN_APP 500000

AppLifeManager::AppLifeManager()
    : launch_item_factory_(NULL),
      prelauncher_(NULL),
      memory_checker_(NULL),
      lastapp_handler_(NULL) {
}

AppLifeManager::~AppLifeManager() {
}

void AppLifeManager::init() {

  lifecycle_router_.Init();
  AppInfoManager::instance().Init();

  // receive signal on service disconnected
  web_lifecycle_handler_.signal_service_disconnected.connect(
    boost::bind(&AppLifeManager::stop_all_webapp_item, this) );

  // receive signal on life status change
  native_lifecycle_handler_.signal_app_life_status_changed.connect(
    boost::bind(&AppLifeManager::on_runtime_status_changed, this, _1, _2, _3) );
  web_lifecycle_handler_.signal_app_life_status_changed.connect(
    boost::bind(&AppLifeManager::on_runtime_status_changed, this, _1, _2, _3) );
  qml_lifecycle_handler_.signal_app_life_status_changed.connect(
    boost::bind(&AppLifeManager::on_runtime_status_changed, this, _1, _2, _3) );

  // receive signal on running list change
  native_lifecycle_handler_.signal_running_app_added.connect(
    boost::bind(&AppLifeManager::on_running_app_added, this, _1, _2, _3) );
  web_lifecycle_handler_.signal_running_app_added.connect(
    boost::bind(&AppLifeManager::on_running_app_added, this, _1, _2, _3) );
  qml_lifecycle_handler_.signal_running_app_added.connect(
    boost::bind(&AppLifeManager::on_running_app_added, this, _1, _2, _3) );
  native_lifecycle_handler_.signal_running_app_removed.connect(
    boost::bind(&AppLifeManager::on_running_app_removed, this, _1) );
  web_lifecycle_handler_.signal_running_app_removed.connect(
    boost::bind(&AppLifeManager::on_running_app_removed, this, _1) );
  qml_lifecycle_handler_.signal_running_app_removed.connect(
    boost::bind(&AppLifeManager::on_running_app_removed, this, _1) );

  // receive signal on launching done
  native_lifecycle_handler_.signal_launching_done.connect(
    boost::bind(&AppLifeManager::on_launching_done, this, _1) );
  web_lifecycle_handler_.signal_launching_done.connect(
    boost::bind(&AppLifeManager::on_launching_done, this, _1) );
  qml_lifecycle_handler_.signal_launching_done.connect(
    boost::bind(&AppLifeManager::on_launching_done, this, _1) );

  // subscriber lsm's foreground info
  LSMSubscriber::instance().signal_foreground_info.connect(
    boost::bind(&AppLifeManager::on_foreground_info_changed, this, _1) );

  // set fullscreen window type
  m_fullscreen_window_types = SettingsImpl::instance().fullscreen_window_types;
}

///////////////////////////////////////////////////////////////
void AppLifeManager::Launch(LifeCycleTaskPtr task) {
  launch(AppLaunchRequestType::EXTERNAL, task->app_id(), task->LunaTask()->jmsg(), task->LunaTask()->lsmsg());
}

void AppLifeManager::Pause(LifeCycleTaskPtr task) {
  const pbnjson::JValue& jmsg = task->LunaTask()->jmsg();

  const pbnjson::JValue& params = jmsg.hasKey("params") && jmsg["params"].isObject() ?
                                    jmsg["params"] : pbnjson::Object();
  std::string err_text;
  pause_app(task->app_id(), params, err_text);

  if (!err_text.empty()) {
    task->FinalizeWithError(API_ERR_CODE_GENERAL, err_text);
    return;
  }
  task->Finalize();
}

void AppLifeManager::Close(LifeCycleTaskPtr task) {
  const pbnjson::JValue& jmsg = task->LunaTask()->jmsg();

  std::string app_id = jmsg["id"].asString();
  std::string caller_id = task->LunaTask()->caller();
  std::string err_text;
  bool preload_only = false;
  bool handle_pause_app_self = false;
  std::string reason;

  LOG_INFO(MSGID_APPCLOSE, 2, PMLOGKS("app_id", app_id.c_str()),
                              PMLOGKS("caller", caller_id.c_str()),
                              "params: %s", JUtil::jsonToString(jmsg).c_str());

  if (jmsg.hasKey("preloadOnly")) preload_only = jmsg["preloadOnly"].asBool();
  if (jmsg.hasKey("reason")) reason = jmsg["reason"].asString();
  if (jmsg.hasKey("letAppHandle")) handle_pause_app_self = jmsg["letAppHandle"].asBool();

  if (handle_pause_app_self) {
    pause_app(app_id, pbnjson::Object(), err_text, false);
  } else {
    AppLifeManager::instance().close_by_app_id(app_id, caller_id, reason, err_text, preload_only, false);
  }

  if (!err_text.empty()) {
    LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", app_id.c_str()), "err_text: %s", err_text.c_str());
    task->FinalizeWithError(API_ERR_CODE_GENERAL, err_text);
    return;
  }

  pbnjson::JValue payload = pbnjson::Object();
  payload.put("appId", app_id);
  task->Finalize(payload);
}

void AppLifeManager::CloseByPid(LifeCycleTaskPtr task) {
  const pbnjson::JValue& jmsg = task->LunaTask()->jmsg();

  std::string pid, err_text;
  std::string caller_id = task->LunaTask()->caller();

  pid = jmsg["processId"].asString();

  AppLifeManager::instance().close_by_pid(pid, caller_id, err_text);

  if (!err_text.empty()) {
    LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("pid", pid.c_str()), "err_text: %s", err_text.c_str());
    task->FinalizeWithError(API_ERR_CODE_GENERAL, err_text);
    return;
  }

  pbnjson::JValue payload = pbnjson::Object();
  payload.put("processId", pid);
  task->Finalize(payload);
}

void AppLifeManager::CloseAll(LifeCycleTaskPtr task)  {
  close_all_apps();
}

///////////////////////////////////////////////////////////////////////
// set extendable handler
///////////////////////////////////////////////////////////////////////
void AppLifeManager::set_applifeitem_factory(AppLaunchingItemFactoryInterface& factory) {
  launch_item_factory_ = &factory;
}

void AppLifeManager::set_prelauncher_handler(PrelauncherInterface& prelauncher) {
  prelauncher_ = &prelauncher;
  prelauncher.signal_prelaunching_done.connect( boost::bind(&AppLifeManager::on_prelaunching_done, this, _1) );
}

void AppLifeManager::set_memory_checker_handler(MemoryCheckerInterface& memory_checker) {
  memory_checker_ = &memory_checker;
  memory_checker.signal_memory_checking_start.connect( boost::bind(&AppLifeManager::on_memory_checking_start, this, _1) );
  memory_checker.signal_memory_checking_done.connect( boost::bind(&AppLifeManager::on_memory_checking_done, this, _1) );
}

void AppLifeManager::set_lastapp_handler(LastAppHandlerInterface& lastapp_handler) {
  lastapp_handler_ = &lastapp_handler;
}

///////////////////////////////////////////////////////////////////////
/// Stage Clear Event Handler
///////////////////////////////////////////////////////////////////////
void AppLifeManager::on_prelaunching_done(const std::string& uid) {
  AppLaunchingItemPtr item = get_launching_item_by_uid(uid);
  if (item == NULL) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("uid", uid.c_str()),
                                      PMLOGKS("reason", "null_pointer"),
                                      PMLOGKS("where", "on_prelaunching_done"), "");
    return;
  }

  LOG_INFO(MSGID_APPLAUNCH, 4, PMLOGKS("app_id", item->app_id().c_str()),
                               PMLOGKS("uid", uid.c_str()),
                               PMLOGKS("status", "prelaunching_done"),
                               PMLOGKFV("launching_stage_in_detail", "%d", item->sub_stage()), "");

  if (AppLaunchingStage::PRELAUNCH != item->stage()) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 4, PMLOGKS("app_id", item->app_id().c_str()),
                                      PMLOGKS("uid", uid.c_str()),
                                      PMLOGKS("reason", "not_in_prelaunching_stage"),
                                      PMLOGKS("where", "on_prelaunching_done"), "");
    return;
  }

  // just finish launch if error occurs
  if (item->err_text().empty() == false) {
    finish_launching(item);
    return;
  }

  // go next stage
  run_with_memory_checker(item);
}

void AppLifeManager::on_memory_checking_start(const std::string& uid) {
  AppLaunchingItemPtr item = get_launching_item_by_uid(uid);
  if (item == NULL) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("uid", uid.c_str()),
                                      PMLOGKS("reason", "null_pointer"),
                                      PMLOGKS("where", "on_memory_checking_start"), "");
    return;
  }
  GenerateLifeCycleEvent(item->app_id(), uid, LifeEvent::SPLASH);
}

void AppLifeManager::on_memory_checking_done(const std::string& uid) {
  AppLaunchingItemPtr item = get_launching_item_by_uid(uid);
  if (item == NULL) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("uid", uid.c_str()),
                                      PMLOGKS("reason", "null_pointer"),
                                      PMLOGKS("where", "on_memory_checking_done"), "");
    return;
  }

  LOG_INFO(MSGID_APPLAUNCH, 4, PMLOGKS("app_id", item->app_id().c_str()),
                               PMLOGKS("uid", uid.c_str()),
                               PMLOGKS("status", "memory_checking_done"),
                               PMLOGKFV("launching_stage_in_detail", "%d", item->sub_stage()), "");

  if (AppLaunchingStage::MEMORY_CHECK != item->stage()) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 4, PMLOGKS("app_id", item->app_id().c_str()),
                                      PMLOGKS("uid", uid.c_str()),
                                      PMLOGKS("reason", "not_in_memory_checking_stage"),
                                      PMLOGKS("where", "on_memory_checking_done"), "");
    return;
  }

  // just finish launch if error occurs
  if (item->err_text().empty() == false) {
    finish_launching(item);
    return;
  }

  // go next stage
  run_with_launcher(item);
}

void AppLifeManager::on_launching_done(const std::string& uid) {
  AppLaunchingItemPtr item = get_launching_item_by_uid(uid);
  if (item == NULL) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("uid", uid.c_str()),
                                      PMLOGKS("reason", "null_pointer"),
                                      PMLOGKS("where", "on_launching_done"), "");
    return;
  }

  LOG_INFO(MSGID_APPLAUNCH, 3, PMLOGKS("app_id", item->app_id().c_str()),
                               PMLOGKS("uid", uid.c_str()),
                               PMLOGKS("status", "launching_done"), "");

  finish_launching(item);
}

///////////////////////////////////////////////////////////////////////
/// stage handler
///////////////////////////////////////////////////////////////////////
void AppLifeManager::run_with_prelauncher(AppLaunchingItemPtr item) {
  LOG_INFO_WITH_CLOCK(MSGID_APPLAUNCH, 4, PMLOGKS("app_id", item->app_id().c_str()),
                                          PMLOGKS("status", "start_prelaunching"),
                                          PMLOGKS("PerfType", "AppLaunch"),
                                          PMLOGKS("PerfGroup", item->app_id().c_str()), "");

  if (prelauncher_ == NULL) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("app_id", item->app_id().c_str()),
                                      PMLOGKS("reason", "no_prelauncher_handler"),
                                      PMLOGKS("where", "run_with_prelauncher"), "");
    item->set_err_code_text(APP_LAUNCH_ERR_GENERAL, "internal error");
    finish_launching(item);
    return;
  }

  item->set_stage(AppLaunchingStage::PRELAUNCH);

  prelauncher_->add_item(item);
}

void AppLifeManager::run_with_memory_checker(AppLaunchingItemPtr item) {
  LOG_INFO_WITH_CLOCK(MSGID_APPLAUNCH, 4, PMLOGKS("app_id", item->app_id().c_str()),
                                          PMLOGKS("status", "start_memory_checking"),
                                          PMLOGKS("PerfType", "AppLaunch"),
                                          PMLOGKS("PerfGroup", item->app_id().c_str()), "");

  if (memory_checker_ == NULL) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 1, PMLOGKS("app_id", item->app_id().c_str()),
                                      "memorychecker is not registered");
    return;
  }
  item->set_stage(AppLaunchingStage::MEMORY_CHECK);
  memory_checker_->add_item(item);
  memory_checker_->run();
}

void AppLifeManager::run_with_launcher(AppLaunchingItemPtr item) {
  LOG_INFO_WITH_CLOCK(MSGID_APPLAUNCH, 4, PMLOGKS("app_id", item->app_id().c_str()),
                                          PMLOGKS("status", "start_launching"),
                                          PMLOGKS("PerfType", "AppLaunch"),
                                          PMLOGKS("PerfGroup", item->app_id().c_str()), "");

  item->set_stage(AppLaunchingStage::LAUNCH);
  launch_app(item);
}

void AppLifeManager::run_lastapp_handler() {
  if (lastapp_handler_ == NULL) {
    LOG_ERROR(MSGID_LAUNCH_LASTAPP_ERR, 2, PMLOGKS("reason", "no_lastapp_handler"),
                                           PMLOGKS("where", "run_lastapp_handler"), "");
    return;
  }

  if (is_fullscreen_app_loading("", "")) {
    LOG_INFO(MSGID_LAUNCH_LASTAPP, 1, PMLOGKS("status", "skip_launching_last_input_app"), "");
    return;
  }

  lastapp_handler_->launch();
}

void AppLifeManager::finish_launching(AppLaunchingItemPtr item) {
  bool is_automatic_launch = item->automatic_launch();
  bool is_last_app_candidate = is_last_launching_app(item->app_id()) && (last_launching_apps_.size() == 1);

  bool redirect_to_lastapplaunch = (is_automatic_launch || item->is_last_input_app() || is_last_app_candidate) && !item->err_text().empty();

  LOG_INFO(MSGID_APPLAUNCH, 3, PMLOGKS("app_id", item->app_id().c_str()),
                               PMLOGKS("status", "finish_launching"),
                               PMLOGKS("mode", (is_automatic_launch ? "automatic_launch":"normal")), "");

  signal_launching_finished(item);
  reply_with_result(item->lsmsg(), item->pid(), item->err_text().empty(), item->err_code(), item->err_text());
  remove_last_launching_app(item->app_id());

  std::string app_uid = item->uid();
  remove_item(app_uid);

  // TODO: decide if this is tv specific or not
  //       make tv handler if it's tv specific
  if (redirect_to_lastapplaunch) {
    LOG_INFO(MSGID_LAUNCH_LASTAPP, 1, PMLOGKS("action", "trigger_launch_lastapp"), "");
    run_lastapp_handler();
  }
}

///////////////////////////////////////////////////////////////////////
/// App life event handler
///////////////////////////////////////////////////////////////////////

void AppLifeManager::on_runtime_status_changed(
    const std::string& app_id, const std::string& uid, const RuntimeStatus& new_status) {

  if (app_id.empty()) {
    LOG_ERROR(MSGID_LIFE_STATUS_ERR, 2, PMLOGKS("reason", "empty_app_id"),
                                        PMLOGKS("where", __FUNCTION__), "");
    return;
  }

  lifecycle_router_.SetRuntimeStatus(app_id, new_status);
  LifeStatus new_life_status = lifecycle_router_.GetLifeStatusFromRuntimeStatus(new_status);

  SetAppLifeStatus(app_id, uid, new_life_status);
}

void AppLifeManager::SetAppLifeStatus(
    const std::string& app_id, const std::string& uid, LifeStatus new_status) {

  AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
  LifeStatus current_status = AppInfoManager::instance().life_status(app_id);

  const LifeCycleRoutePolicy& route_policy = lifecycle_router_.GetLifeCycleRoutePolicy(
                                                current_status, new_status);
  LifeStatus next_status;
  RouteAction route_action;
  RouteLog route_log;
  std::tie(next_status, route_action, route_log) = route_policy;

  LifeEvent life_event = lifecycle_router_.GetLifeEventFromLifeStatus(next_status);

  // generate lifecycle event
  GenerateLifeCycleEvent(app_id, uid, life_event);

  if (RouteLog::CHECK == route_log) {
    LOG_INFO(MSGID_LIFE_STATUS, 3, PMLOGKS("app_id", app_id.c_str()),
                                   PMLOGKFV("current", "%d", (int)current_status),
                                   PMLOGKFV("next", "%d", (int)next_status),
                                   "just to check it");
  } else if (RouteLog::WARN == route_log) {
    LOG_WARNING(MSGID_LIFE_STATUS, 3, PMLOGKS("app_id", app_id.c_str()),
                                      PMLOGKFV("current", "%d", (int)current_status),
                                      PMLOGKFV("next", "%d", (int)next_status),
                                      "handle exception");
  } else if (RouteLog::ERROR == route_log) {
    LOG_ERROR(MSGID_LIFE_STATUS, 3, PMLOGKS("app_id", app_id.c_str()),
                                    PMLOGKFV("current", "%d", (int)current_status),
                                    PMLOGKFV("next", "%d", (int)next_status),
                                    "unexpected transition");
  }

  if (RouteAction::IGNORE == route_action) return;

  switch (next_status) {
    case LifeStatus::LAUNCHING:
    case LifeStatus::RELAUNCHING:
      if (app_desc) add_loading_app(app_id, app_desc->type());
      AppInfoManager::instance().set_preload_mode(app_id, false);
      break;
    case LifeStatus::PRELOADING:
      AppInfoManager::instance().set_preload_mode(app_id, true);
      break;
    case LifeStatus::STOP:
    case LifeStatus::FOREGROUND:
      AppInfoManager::instance().set_preload_mode(app_id, false);
    case LifeStatus::PAUSING:
      remove_loading_app(app_id);
      break;
    default:
      break;
  }

  LOG_INFO(MSGID_LIFE_STATUS, 4, PMLOGKS("app_id", app_id.c_str()),
                                 PMLOGKS("uid", uid.c_str()),
                                 PMLOGKFV("current", "%d", (int)current_status),
                                 PMLOGKFV("new", "%d", (int)next_status), "life_status_changed");

  // set life status
  AppInfoManager::instance().set_life_status(app_id, next_status);
  signal_app_life_status_changed(app_id, next_status);
  reply_subscription_for_app_life_status(app_id, uid, next_status);
}

void AppLifeManager::stop_all_webapp_item() {
  LOG_INFO(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("status", "remove_webapp_in_loading_list"),
                                   PMLOGKS("reason", "WAM_disconnected"), "");

  std::vector<LoadingAppItem> loading_apps = m_loading_app_list;
  for (const auto& app: loading_apps) {
    if (std::get<1>(app) == AppType::Web)
      on_runtime_status_changed(std::get<0>(app), "", RuntimeStatus::STOP);
  }
}

///////////////////////////////////////////////////////////////////////
/// running list changed
///////////////////////////////////////////////////////////////////////
void AppLifeManager::on_running_app_added(const std::string& app_id, const std::string& pid,
    const std::string& webprocid) {
  AppInfoManager::instance().add_running_info(app_id, pid, webprocid);
  on_running_list_changed(app_id);
}

void AppLifeManager::on_running_app_removed(const std::string& app_id) {

  AppInfoManager::instance().remove_running_info(app_id);
  on_running_list_changed(app_id);

  if (is_in_automatic_pending_list(app_id))
    handle_automatic_app(app_id);
}

void AppLifeManager::on_running_list_changed(const std::string& app_id) {
  pbnjson::JValue running_list = pbnjson::Array();
  AppInfoManager::instance().get_running_list(running_list);
  reply_subscription_for_running(running_list);

  AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
  if (app_desc != NULL && AppTypeByDir::Dev == app_desc->getTypeByDir()) {
    running_list = pbnjson::Array();
    AppInfoManager::instance().get_running_list(running_list, true);
    reply_subscription_for_running(running_list, true);
  }
}

///////////////////////////////////////////////////////////////////////
/// handle reply
///////////////////////////////////////////////////////////////////////
void AppLifeManager::reply_with_result(LSMessage* lsmsg, const std::string& pid, bool result,
    const int& err_code, const std::string& err_text) {

  pbnjson::JValue payload = pbnjson::Object();
  payload.put("returnValue", result);

  if (!result) {
    payload.put("errorCode", err_code);
    payload.put("errorText", err_text);
  }

  if (lsmsg == NULL) {
    LOG_INFO(MSGID_APPLAUNCH, 3, PMLOGKS("reason", "null_lsmessage"),
                                 PMLOGKS("where", "reply_with_result_in_app_life_manager"),
                                 PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()), "");
    return;
  }

  LOG_INFO(MSGID_APPLAUNCH, 2, PMLOGKS("status", "reply_launch_request"),
                               PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()), "");

  LSErrorSafe lserror;
  if (!LSMessageRespond(lsmsg, JUtil::jsonToString(payload).c_str(), &lserror )) {
    LOG_ERROR(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "lsrespond"),
                                   PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()),
                                   PMLOGKS("where", "respond_in_app_life_manager"), "err: %s", lserror.message);
  }
}

void AppLifeManager::reply_subscription_for_app_life_status(
    const std::string& app_id, const std::string& uid, const LifeStatus& life_status) {

  if (app_id.empty()) {
    LOG_WARNING(MSGID_APP_LIFESTATUS_REPLY_ERROR, 1, PMLOGKS("reason", "null app_id"), "");
    return;
  }

  pbnjson::JValue payload = pbnjson::Object();
  if (payload.isNull()) {
    LOG_WARNING(MSGID_APP_LIFESTATUS_REPLY_ERROR, 1, PMLOGKS("reason", "make pbnjson failed"), "");
    return;
  }

  AppLaunchingItemPtr launch_item = uid.empty() ? NULL : get_launching_item_by_uid(uid);
  std::string str_status= AppInfoManager::instance().life_status_to_string(life_status);
  bool preloaded = AppInfoManager::instance().preload_mode_on(app_id);

  payload.put("status", str_status);    // change enum to string
  payload.put("appId", app_id);

  std::string pid = AppInfoManager::instance().pid(app_id);
  if (!pid.empty())
    payload.put("processId", pid);

  AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);

  if (app_desc != NULL)
      payload.put("type", ApplicationDescription::appTypeToString(app_desc->type()));

  if (LifeStatus::LAUNCHING == life_status || LifeStatus::RELAUNCHING == life_status) {
    if (launch_item) {
      payload.put("reason", launch_item->launch_reason());
    }
  } else if (LifeStatus::FOREGROUND == life_status) {
    pbnjson::JValue fg_info = pbnjson::JValue();
    AppInfoManager::instance().get_json_foreground_info_by_id(app_id, fg_info);
    if (!fg_info.isNull() && fg_info.isObject()) {
      for (auto it: fg_info.children()) {
        const std::string key = it.first.asString();
        if ("windowType" == key || "windowGroup" == key ||
            "windowGroupOwner" == key || "windowGroupOwnerId" == key) {
          payload.put(key, fg_info[key]);
        }
      }
    }
  } else if (LifeStatus::BACKGROUND == life_status) {
    if (preloaded)
      payload.put("backgroundStatus", "preload");
    else
      payload.put("backgroundStatus", "normal");
  } else if (LifeStatus::STOP == life_status || LifeStatus::CLOSING == life_status) {
    payload.put("reason",
        (close_reason_info_.count(app_id) == 0) ? "undefined" : close_reason_info_[app_id]);

    if (LifeStatus::STOP == life_status)
      close_reason_info_.erase(app_id);
  }

  LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2, PMLOGKS("skey", "getAppLifeStatus"),
                                        PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()), "");

  LSErrorSafe lserror;
  if (!LSSubscriptionReply(AppMgrService::instance().ServiceHandle(), "getAppLifeStatus",
                          JUtil::jsonToString(payload).c_str(), &lserror)) {
    LOG_ERROR(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "subscriptionreply"),
                                   PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()),
                                   PMLOGKS("where", "reply_get_app_life_status"),
                                   "err: %s", lserror.message);
    return;
  }
}

void AppLifeManager::GenerateLifeCycleEvent(const std::string& app_id, const std::string& uid, LifeEvent event) {

  AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
  AppLaunchingItemPtr launch_item = uid.empty() ? NULL : get_launching_item_by_uid(uid);
  LifeStatus current_status = AppInfoManager::instance().life_status(app_id);
  bool preloaded = AppInfoManager::instance().preload_mode_on(app_id);

  pbnjson::JValue payload = pbnjson::Object();
  payload.put("appId", app_id);

  if (LifeEvent::SPLASH == event) {
    // generate splash event only for fresh launch case
    if (launch_item && !launch_item->show_splash() && !launch_item->show_spinner()) return;
    if (LifeStatus::BACKGROUND == current_status && preloaded == false) return;
    if (LifeStatus::STOP != current_status &&
        LifeStatus::PRELOADING != current_status &&
        LifeStatus::BACKGROUND != current_status) return;

    payload.put("event", "splash");
    payload.put("title", (app_desc?app_desc->title():""));
    payload.put("showSplash", (launch_item && launch_item->show_splash()));
    payload.put("showSpinner", (launch_item && launch_item->show_spinner()));

    if (launch_item && launch_item->show_splash())
      payload.put("splashBackground", (app_desc?app_desc->splashBackground():""));
  }
  else if (LifeEvent::PRELOAD == event) {
    payload.put("event", "preload");
    if (launch_item)
      payload.put("preload", launch_item->preload());
  }
  else if (LifeEvent::LAUNCH == event) {
    payload.put("event", "launch");
    payload.put("reason", launch_item->launch_reason());
  }
  else if (LifeEvent::FOREGROUND == event) {
    payload.put("event", "foreground");

    pbnjson::JValue fg_info = pbnjson::JValue();
    AppInfoManager::instance().get_json_foreground_info_by_id(app_id, fg_info);
    if (!fg_info.isNull() && fg_info.isObject()) {
      for (auto it: fg_info.children()) {
        const std::string key = it.first.asString();
        if ("windowType" == key || "windowGroup" == key ||
            "windowGroupOwner" == key || "windowGroupOwnerId" == key) {
          payload.put(key, fg_info[key]);
        }
      }
    }
  }
  else if (LifeEvent::BACKGROUND == event) {
    payload.put("event", "background");
    if (preloaded)
      payload.put("status", "preload");
    else
      payload.put("status", "normal");
  }
  else if (LifeEvent::PAUSE == event) {
    payload.put("event", "pause");
  }
  else if (LifeEvent::CLOSE == event) {
    payload.put("event", "close");
    payload.put("reason", (close_reason_info_.count(app_id) == 0) ? "undefined" : close_reason_info_[app_id]);
  }
  else if (LifeEvent::STOP == event) {
    payload.put("event", "stop");
    payload.put("reason", (close_reason_info_.count(app_id) == 0) ? "undefined" : close_reason_info_[app_id]);
  }
  else {
    return;
  }

  signal_lifecycle_event(payload);
}

void AppLifeManager::reply_subscription_for_running(const pbnjson::JValue& running_list, bool devmode) {
  pbnjson::JValue payload = pbnjson::Object();
  std::string subscription_key = devmode ? "dev_running" : "running";

  payload.put("running", running_list);
  payload.put("returnValue", true);

  LOG_INFO(MSGID_SUBSCRIPTION_REPLY, 2, PMLOGKS("skey", subscription_key.c_str()),
                                        PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()), "");

  LSErrorSafe lserror;
  if (!LSSubscriptionReply(AppMgrService::instance().ServiceHandle(), subscription_key.c_str(),
                          JUtil::jsonToString(payload).c_str(), &lserror)) {
    LOG_ERROR(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "subscriptionreply"),
                                   PMLOGJSON("payload", JUtil::jsonToString(payload).c_str()),
                                   PMLOGKS("where", "reply_running_in_app_life_manager"),
                                   "err: %s", lserror.message);
    return;
  }
}

///////////////////////////////////////////////////////////////////////
/// API: launch
///////////////////////////////////////////////////////////////////////
void AppLifeManager::launch(AppLaunchRequestType rtype, const std::string& app_id,
                            const pbnjson::JValue& params, LSMessage* lsmsg) {

  // check launching item factory
  if (launch_item_factory_ == NULL) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 1, PMLOGKS("status", "no_launch_item_factory_registered"), "");
    reply_with_result(lsmsg, "", false, (int)APP_LAUNCH_ERR_GENERAL, "internal error");
    return;
  }

  int err_code = 0;
  std::string err_text = "";
  AppLaunchingItemPtr new_item = launch_item_factory_->Create(app_id, rtype, params,
                                                              lsmsg, err_code, err_text);
  if(new_item == NULL) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("app_id", app_id.c_str()),
                                      PMLOGKS("reason", "creating_item_fail"),
                                      PMLOGKS("where", "launch_in_app_life_manager"), "");
    if (err_text.empty())
      reply_with_result(lsmsg, "", false, (int)APP_LAUNCH_ERR_GENERAL, "internal error");
    else
      reply_with_result(lsmsg, "", false, err_code, err_text);
    return;
  }

  AppDescPtr app_desc = ApplicationManager::instance().getAppById(new_item->app_id());

  // set start time
  new_item->set_launch_start_time(get_current_time());

  // put new request into launching queue
  launch_item_list_.push_back(new_item);

  if (new_item->automatic_launch()) {

    if (is_fullscreen_app_loading(new_item->app_id(), new_item->uid())) {
      LOG_INFO(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("app_id", new_item->app_id().c_str()),
                                        PMLOGKS("status", "skip_launching_last_app"), "");
      finish_launching(new_item);
      return;
    }

    if (new_item->launch_reason() == "autoReload" && AppInfoManager::instance().is_running(new_item->app_id())) {
      LOG_INFO(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("app_id", new_item->app_id().c_str()),
                                        PMLOGKS("status", "waiting_for_foreground_app_removed"), "");
      add_item_into_automatic_pending_list(new_item);
      return;
    }
  }

  // start prelaunching
  run_with_prelauncher(new_item);
}

void AppLifeManager::handle_bridged_launch_request(const pbnjson::JValue& params) {
  std::string item_uid;
  if (!params.hasKey(SYS_LAUNCHING_UID) || params[SYS_LAUNCHING_UID].asString(item_uid) != CONV_OK) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("reason", "uid_not_found"),
                                      PMLOGKS("where", "handle_bridged_launch"), "");
    return;
  }

  AppLaunchingItemPtr launching_item = get_launching_item_by_uid(item_uid);
  if (launching_item == NULL) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("uid", item_uid.c_str()),
                                      PMLOGKS("reason", "launching_item_not_found"),
                                      PMLOGKS("where", "handle_bridged_launch"), "");
    return;
  }

  // set return payload
  launching_item->set_call_return_jmsg(params);

  if (prelauncher_)
    prelauncher_->input_bridged_return(launching_item, params);
}

///////////////////////////////////////////////////////////////////////
/// API: registerNativeApp
///////////////////////////////////////////////////////////////////////
void AppLifeManager::RegisterApp(const std::string& app_id, LSMessage* lsmsg, std::string& err_text) {

  AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
  if (!app_desc) {
    err_text = "not existing app";
    return;
  }

  if (app_desc->NativeInterfaceVersion() != 2) {
    err_text = "trying to register via unmatched method with nativeLifeCycleInterfaceVersion";
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("app_id", app_id.c_str()),
                                      PMLOGKS("reason", "wrong_version_interface_on_register"),
                                      PMLOGKS("method", "registerNativeApp"),
                                      "nativeLifeCycleInterfaceVersion: %d", app_desc->NativeInterfaceVersion());
    return;
  }

  RuntimeStatus runtime_status = AppInfoManager::instance().runtime_status(app_id);

  if(RuntimeStatus::RUNNING != runtime_status &&
     RuntimeStatus::REGISTERED != runtime_status) {
    err_text ="invalid status";
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("app_id", app_id.c_str()),
                                      PMLOGKS("reason", "invalid_life_status_to_connect_nativeapp"),
                                      PMLOGKFV("runtime_status", "%d", (int) runtime_status), "");
    return;
  }

  native_lifecycle_handler_.RegisterApp(app_id, lsmsg, err_text);
}


void AppLifeManager::connect_nativeapp(const std::string& app_id, LSMessage* lsmsg, std::string& err_text) {

  AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
  if (!app_desc) {
    err_text = "not existing app";
    return;
  }

  if (app_desc->NativeInterfaceVersion() != 1) {
    err_text = "trying to register via unmatched method with nativeLifeCycleInterfaceVersion";
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("app_id", app_id.c_str()),
                                      PMLOGKS("reason", "wrong_version_interface_on_register"),
                                      PMLOGKS("method", "registerNativeApp"),
                                      "nativeLifeCycleInterfaceVersion: %d", app_desc->NativeInterfaceVersion());
    return;
  }

  RuntimeStatus runtime_status = AppInfoManager::instance().runtime_status(app_id);

  if(RuntimeStatus::RUNNING != runtime_status &&
      RuntimeStatus::REGISTERED != runtime_status) {
    err_text = "invalid_status";
      LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("app_id", app_id.c_str()),
                                        PMLOGKS("reason", "invalid_life_status_to_connect_nativeapp"),
                                        PMLOGKFV("runtime_status", "%d", (int) runtime_status), "");
      return;
  }

  native_lifecycle_handler_.RegisterApp(app_id, lsmsg, err_text);
}

///////////////////////////////////////////////////////////////////////
/// API: changeRunningAppId
///////////////////////////////////////////////////////////////////////
bool AppLifeManager::change_running_app_id(const std::string& current_id, const std::string& target_id,
    ErrorInfo& err_info) {
  return native_lifecycle_handler_.ChangeRunningAppId(current_id, target_id, err_info);
}

void AppLifeManager::trigger_to_launch_last_app() {

  std::string fg_app_id = AppInfoManager::instance().get_current_foreground_app_id();
  if (!fg_app_id.empty() && AppInfoManager::instance().is_running(fg_app_id))
    return;

  LOG_INFO(MSGID_LAUNCH_LASTAPP, 1, PMLOGKS("status", "trigger_to_launch_last_app"), "");

  run_lastapp_handler();
}

///////////////////////////////////////////////////////////////////////
/// close
///////////////////////////////////////////////////////////////////////
void AppLifeManager::close_by_app_id(const std::string& app_id, const std::string& caller_id,
    const std::string& reason, std::string& err_text,
    bool preload_only, bool clear_all_items) {

  if (preload_only && !has_only_preloaded_items(app_id)) {
    err_text = "app is being launched by user";
    return;
  }

  // keepAlive app policy
  // swith close request to pause request
  // except for below cases
  if (SettingsImpl::instance().IsKeepAliveApp(app_id) &&
      caller_id != "com.webos.memorymanager" &&
      caller_id != "com.webos.appInstallService" &&
      (caller_id != "com.webos.surfacemanager.windowext" || reason != "recent") &&
      caller_id != SAM_INTERNAL_ID) {
    pause_app(app_id, pbnjson::Object(), err_text);
    return;
  }

  close_app(app_id, caller_id, reason, err_text, clear_all_items);
}

void AppLifeManager::close_by_pid(const std::string& pid, const std::string& caller_id, std::string& err_text) {
  const std::string& app_id = AppInfoManager::instance().get_app_id_by_pid(pid);
  if (app_id.empty()) {
    LOG_ERROR(MSGID_APPCLOSE_ERR, 2, PMLOGKS("pid", pid.c_str()),
                                     PMLOGKS("mode", "by_pid"), "no appid matched by pid");
    err_text = "no app matched by pid";
    return;
  }

  close_app(app_id, caller_id, "", err_text);
}

void AppLifeManager::close_all_loading_apps() {
  reset_last_app_candidates();

  prelauncher_->cancel_all();
  memory_checker_->cancel_all();

  std::vector<std::string> automatic_pending_list;
  get_automatic_pending_app_ids(automatic_pending_list);

  for (auto& app_id : automatic_pending_list)
    handle_automatic_app(app_id, false);

  std::vector<LoadingAppItem> loading_apps = m_loading_app_list;
  for (const auto& app: loading_apps) {
    std::string err_text = "";
    close_by_app_id(std::get<0>(app), SAM_INTERNAL_ID, "", err_text, false, true);

    if (err_text.empty() == false)
      LOG_WARNING(MSGID_APPCLOSE, 2, PMLOGKS("app_id", (std::get<0>(app)).c_str()), PMLOGKS("mode", "close_loading_app"), "err: %s", err_text.c_str());
    else
      LOG_INFO(MSGID_APPCLOSE, 2, PMLOGKS("app_id", (std::get<0>(app)).c_str()), PMLOGKS("mode", "close_loading_app"), "ok");
  }
}

void AppLifeManager::close_all_apps(bool clear_all_items) {
  LOG_INFO(MSGID_APPCLOSE, 2, PMLOGKS("action", "close_all_apps"), PMLOGKS("status", "start"), "");
  std::vector<std::string> running_apps;
  AppInfoManager::instance().get_running_app_ids(running_apps);
  close_apps(running_apps, clear_all_items);
  reset_last_app_candidates();
}

void AppLifeManager::close_apps(const std::vector<std::string>& app_ids, bool clear_all_items) {
  if (app_ids.size() < 1) {
    LOG_INFO(MSGID_APPCLOSE, 2, PMLOGKS("action", "close_all_apps"), PMLOGKS("status", "no_apps_to_close"), "");
    return;
  }

  std::string fullscreen_app_id;
  bool performed_closing_apps = false;
  for (auto& app_id: app_ids) {
    // kill app on fullscreen later
    if (AppInfoManager::instance().is_app_on_fullscreen(app_id)) {
      fullscreen_app_id = app_id;
      continue;
    }

    std::string err_text = "";
    close_by_app_id(app_id, SAM_INTERNAL_ID, "", err_text, false, clear_all_items);
    performed_closing_apps = true;
  }

  // now kill app on fullscreen
  if (!fullscreen_app_id.empty()) {
    // we don't guarantee all apps running in background are closed before closing app running on foreground
    // we just raise possibility by adding sleep time
    if (performed_closing_apps) usleep(SLEEP_TIME_TO_CLOSE_FULLSCREEN_APP);
    std::string err_text = "";
    close_by_app_id(fullscreen_app_id, SAM_INTERNAL_ID, "", err_text, false, clear_all_items);
  }
}



///////////////////////////////////////////////////////////////////////
/// app life handlers
///////////////////////////////////////////////////////////////////////
void AppLifeManager::launch_app(AppLaunchingItemPtr item)
{
  AppLifeHandlerInterface* life_handler = GetLifeHandlerForApp(item->app_id());
  if(nullptr == life_handler) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("app_id", item->app_id().c_str()),
                                      PMLOGKS("reason", "null_description"),
                                      PMLOGKS("where", "launch_app_in_app_life_manager"), "");
    return;
  }

  life_handler->launch(item);
}

void AppLifeManager::close_app(const std::string& app_id, const std::string& caller_id,
                               const std::string& reason, std::string& err_text, bool clear_all_items) {
  if (clear_all_items)
    clear_launching_and_loading_items_by_app_id(app_id);

  AppLifeHandlerInterface* life_handler = GetLifeHandlerForApp(app_id);
  if (nullptr == life_handler) {
    LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", app_id.c_str()), "no valid life handler");
    err_text = "no valid life handler";
    return;
  }

  std::string close_reason = SettingsImpl::instance().GetCloseReason(caller_id, reason);

  if (AppInfoManager::instance().is_running(app_id) && close_reason_info_.count(app_id) == 0) {
    close_reason_info_[app_id] = close_reason;
  }

  const std::string& pid = AppInfoManager::instance().pid(app_id);
  AppCloseItemPtr new_item = std::make_shared<AppCloseItem>(app_id, pid, caller_id, close_reason);
  if (nullptr == new_item) {
    LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", app_id.c_str()), "memory alloc fail");
    err_text = "memory alloc fail";
    return;
  }

  LOG_INFO(MSGID_APPCLOSE, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("pid", pid.c_str()), "");

  life_handler->close(new_item, err_text);
}

void AppLifeManager::pause_app(const std::string& app_id, const pbnjson::JValue& params,
    std::string& err_text, bool report_event) {

  if (!AppInfoManager::instance().is_running(app_id)) {
    err_text = "app is not running";
    return;
  }

  AppLifeHandlerInterface* life_handler = GetLifeHandlerForApp(app_id);
  if (nullptr == life_handler) {
    LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", app_id.c_str()), "no valid life handler");
    err_text = "no valid life handler";
    return;
  }

  life_handler->pause(app_id, params, err_text, report_event);
}

///////////////////////////////////////////////////////////////////////
/// handle foreground appinfo
///////////////////////////////////////////////////////////////////////

/*
{
    "windowGroup": true,
    "windowGroupOwner": false,
    "windowGroupOwnerId": "com.webos.surfacegroup.owner",
    "appId": "com.webos.surfacegroup.client",
    "windowType": "_WEBOS_WINDOW_TYPE_CARD",
    "windowId": "",
    "processId": "3019"
}
*/

bool AppLifeManager::is_fullscreen_window_type(const pbnjson::JValue& foreground_info)
{
    bool window_group = foreground_info["windowGroup"].asBool();
    bool window_group_owner = (window_group == false ? true : foreground_info["windowGroupOwner"].asBool());
    std::string window_type = foreground_info["windowType"].asString();

    for(auto& win_type: m_fullscreen_window_types) {
        if(win_type == window_type && window_group_owner)
            return true;
    }
    return false;
}

/*
{
    "foregroundAppInfo": [
        {
            "appId": "com.yourdomain.app",
            "windowType": "_WEBOS_WINDOW_TYPE_CARD",
            "processId": "1159",
            "windowId": ""
        },
        {
            "appId": "com.palm.app.settings",
            "windowType": "_WEBOS_WINDOW_TYPE_OVERLAY",
            "processId": "1159",
            "windowId": ""
        }
    ],
    "reason":"forceMinimize",
    "returnValue": true
}
*/

void AppLifeManager::on_foreground_info_changed(const pbnjson::JValue& jmsg)
{
    if(jmsg.isNull() ||
      !jmsg["returnValue"].asBool() ||
      !jmsg.hasKey("foregroundAppInfo") ||
      !jmsg["foregroundAppInfo"].isArray())
    {
        LOG_ERROR(MSGID_GET_FOREGROUND_APP_ERR, 1, PMLOGJSON("payload", JUtil::jsonToString(jmsg).c_str()), "invalid message from LSM");
        return;
    }

    pbnjson::JValue lsm_foreground_info = jmsg["foregroundAppInfo"];

    LOG_INFO(MSGID_GET_FOREGROUND_APPINFO, 1, PMLOGJSON("ForegroundApps", JUtil::jsonToString(lsm_foreground_info).c_str()), "");

    std::string prev_foreground_app_id = AppInfoManager::instance().get_current_foreground_app_id();
    std::vector<std::string> prev_foreground_apps = AppInfoManager::instance().get_foreground_apps();
    std::vector<std::string> new_foreground_apps;
    pbnjson::JValue prev_foreground_info = AppInfoManager::instance().get_json_foreground_info();
    std::string new_foreground_app_id = "";
    bool found_fullscreen_window = false;

    pbnjson::JValue new_foreground_info = pbnjson::Array();
    int array_size = lsm_foreground_info.arraySize();
    for (int i = 0 ; i < array_size ; ++i)
    {
        std::string app_id;
        if (!lsm_foreground_info[i].hasKey("appId") ||
            !lsm_foreground_info[i]["appId"].isString() ||
            lsm_foreground_info[i]["appId"].asString(app_id) != CONV_OK ||
           app_id.empty()) {
            continue;
        }

        LifeStatus current_status = AppInfoManager::instance().life_status(app_id);
        if (LifeStatus::STOP == current_status) {
          LOG_INFO(MSGID_FOREGROUND_INFO, 1, PMLOGKS("filter_out", app_id.c_str()),
                                             "remove not running app");
          continue;
        }

        LOG_INFO_WITH_CLOCK(MSGID_GET_FOREGROUND_APPINFO, 2, PMLOGKS("PerfType", "AppLaunch"),
                                                             PMLOGKS("PerfGroup", app_id.c_str()), "");

        new_foreground_info.append(lsm_foreground_info[i].duplicate());
        new_foreground_apps.push_back(app_id);

        if (is_fullscreen_window_type(lsm_foreground_info[i])) {
            found_fullscreen_window = true;
            new_foreground_app_id = app_id;
        }
    }

    LOG_INFO(MSGID_GET_FOREGROUND_APPINFO, 1, PMLOGJSON("FilteredForegroundApps", JUtil::jsonToString(new_foreground_info).c_str()), "");

    // update foreground info into app info manager
    if(found_fullscreen_window) {
        AppInfoManager::instance().set_last_foreground_app_id(new_foreground_app_id);
        reset_last_app_candidates();
    }
    AppInfoManager::instance().set_current_foreground_app_id(new_foreground_app_id);
    AppInfoManager::instance().set_foreground_apps(new_foreground_apps);
    AppInfoManager::instance().set_foreground_info(new_foreground_info);

    LOG_INFO(MSGID_FOREGROUND_INFO, 2, PMLOGKS("current_foreground_app", new_foreground_app_id.c_str()),
                                       PMLOGKS("prev_foreground_app", prev_foreground_app_id.c_str()), "");

    // set background
    for(auto& prev_app_id: prev_foreground_apps)
    {
        bool found = false;
        for(auto& current_app_id: new_foreground_apps) {
            if((prev_app_id) == (current_app_id)) { found = true; break; }
        }

        if(found == false) {
          switch(AppInfoManager::instance().life_status(prev_app_id)) {
            case LifeStatus::FOREGROUND:
            case LifeStatus::PAUSING:
              SetAppLifeStatus(prev_app_id, "", LifeStatus::BACKGROUND);
              break;
            default:
              break;
          }
        }
    }

    // set foreground
    for(auto& current_app_id: new_foreground_apps)
    {
        SetAppLifeStatus(current_app_id, "", LifeStatus::FOREGROUND);

        if(!AppInfoManager::instance().is_running(current_app_id))
        {
            LOG_INFO(MSGID_FOREGROUND_INFO, 1, PMLOGKS("app_id", current_app_id.c_str()), "no running info, but received foreground info");
        }
    }

    // this is TV specific scenario related to TV POWER (instant booting)
    // improve this tv dependent structure later
    if(jmsg.hasKey("reason") && jmsg["reason"].asString() == "forceMinimize")
    {
        LOG_INFO(MSGID_FOREGROUND_INFO, 1, PMLOGKS("reason", "forceMinimize"), "no trigger last input handler");
        reset_last_app_candidates();
    }
    // run last input handler
    else if(found_fullscreen_window == false)
    {
        run_lastapp_handler();
    }

    // signal foreground app changed
    if(prev_foreground_app_id != new_foreground_app_id)
    {
        signal_foreground_app_changed(new_foreground_app_id);
    }

    // reply subscription foreground with extraInfo
    if (prev_foreground_info != new_foreground_info) {
      signal_foreground_extra_info_changed(new_foreground_info);
    }
}


///////////////////////////////////////////////////////////////////////
/// common util functions
///////////////////////////////////////////////////////////////////////
bool AppLifeManager::is_fullscreen_app_loading(const std::string& new_launching_app_id, const std::string& new_launching_app_uid)
{
    bool result = false;

    for(auto& launching_item: launch_item_list_)
    {
        if(new_launching_app_id == launching_item->app_id() &&
           launching_item->uid() == new_launching_app_uid)
            continue;

        AppDescPtr app_desc = ApplicationManager::instance().getAppById(launching_item->app_id());
        if(app_desc == NULL) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("app_id", launching_item->app_id().c_str()),
                                              PMLOGKS("status", "not_candidate_checking_launching"),
                                              "null description");
            continue;
        }

        if(("com.webos.app.container" == launching_item->app_id()) || ("com.webos.app.inputcommon" == launching_item->app_id()))
            continue;

        if(app_desc->is_child_window()) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("app_id", launching_item->app_id().c_str()),
                                              PMLOGKS("status", "not_candidate_checking_launching"),
                                              "child window type");
            continue;
        }
        if(!(launching_item->preload().empty())) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("app_id", launching_item->app_id().c_str()),
                                              PMLOGKS("status", "not_candidate_checking_launching"),
                                              "preloading app");
            continue;
        }

        if(app_desc->defaultWindowType() == "card" || app_desc->defaultWindowType() == "minimal")
        {
            if(is_launching_item_expired(launching_item))
            {
                LOG_INFO(MSGID_LAUNCH_LASTAPP, 3, PMLOGKS("app_id", launching_item->app_id().c_str()),
                                                  PMLOGKS("status", "not_candidate_checking_launching"),
                                                  PMLOGKFV("launching_stage_in_detail", "%d", launching_item->sub_stage()),
                                                  "fullscreen app, but expired");
                continue;
            }

            LOG_INFO(MSGID_LAUNCH_LASTAPP, 3, PMLOGKS("status", "autolaunch_condition_check"),
                                              PMLOGKS("launching_app_id", launching_item->app_id().c_str()),
                                              PMLOGKFV("launching_stage_in_detail", "%d", launching_item->sub_stage()),
                                              "fullscreen app is already launching");

            add_last_launching_app(launching_item->app_id());
            result = true;
        }
        else
        {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("app_id", launching_item->app_id().c_str()),
                                              PMLOGKS("status", "not_candidate_checking_launching"),
                                              "not fullscreen type: %s", app_desc->defaultWindowType().c_str());
        }
    }

    std::vector<std::string> app_ids;
    get_loading_app_ids(app_ids);

    for(auto& app_id: app_ids)
    {
        AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
        if(app_desc == NULL) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("app_id", app_id.c_str()),
                                              PMLOGKS("status", "not_candidate_checking_loading"),
                                              "null description");
            continue;
        }
        if(app_desc->is_child_window()) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("app_id", app_id.c_str()),
                                              PMLOGKS("status", "not_candidate_checking_loading"),
                                              "child window type");
            continue;
        }
        if(AppInfoManager::instance().preload_mode_on(app_id)) {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("app_id", app_id.c_str()),
                                              PMLOGKS("status", "not_candidate_checking_loading"),
                                              "preloading app");
            continue;
        }

        if(app_desc->defaultWindowType() == "card" || app_desc->defaultWindowType() == "minimal")
        {
            if(is_loading_app_expired(app_id))
            {
                LOG_INFO(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("app_id", app_id.c_str()),
                                                  PMLOGKS("status", "not_candidate_checking_loading"),
                                                  "fullscreen app, but expired");
                continue;
            }

            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("status", "autolaunch_condition_check"),
                                              PMLOGKS("loading_app_id", app_id.c_str()),
                                              "fullscreen app is already loading");

            set_last_loading_app(app_id);
            result = true;
            break;
        }
        else
        {
            LOG_INFO(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("app_id", app_id.c_str()),
                                              PMLOGKS("status", "not_candidate_checking_loading"),
                                              "not fullscreen type: %s", app_desc->defaultWindowType().c_str());
        }
    }

    return result;
}

void AppLifeManager::clear_launching_and_loading_items_by_app_id(const std::string& app_id)
{
    bool found = false;

    while(true)
    {
        auto it = std::find_if(launch_item_list_.begin(), launch_item_list_.end(),
                  [&app_id](AppLaunchingItemPtr item) { return (item->app_id() == app_id); });

        if(it == launch_item_list_.end())
            break;

        AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
        if(app_desc != NULL)
        {
            switch(app_desc->handler_type())
            {
                case LifeHandlerType::Web:
                    web_lifecycle_handler_.clear_handling_item(app_id);
                    break;
                case LifeHandlerType::Qml:
                    qml_lifecycle_handler_.clear_handling_item(app_id);
                    break;
                case LifeHandlerType::Native:
                    native_lifecycle_handler_.clear_handling_item(app_id);
                    break;
                default:
                    break;
            }
        }

        std::string err_text = "stopped launching";
        (*it)->set_err_code_text(APP_LAUNCH_ERR_GENERAL, err_text);
        finish_launching(*it);
        found = true;
    }

    for(auto& loading_app: m_loading_app_list)
    {
        if(std::get<0>(loading_app) == app_id)
        {
            found = true;
            break;
        }
    }

    if(found)
        SetAppLifeStatus(app_id, "", LifeStatus::STOP);
}

void AppLifeManager::handle_automatic_app(const std::string& app_id, bool continue_to_launch)
{
    AppLaunchingItemPtr item = get_launching_item_by_app_id(app_id);
    if(item == NULL)
    {
        LOG_ERROR(MSGID_LAUNCH_LASTAPP_ERR, 3, PMLOGKS("app_id", app_id.c_str()),
                                               PMLOGKS("reason", "launching_item_not_found"),
                                               PMLOGKS("where", "handle_automatic_app"), "");
        remove_item_from_automatic_pending_list(app_id);

        return;
    }

    remove_item_from_automatic_pending_list(app_id);

    if(continue_to_launch)
    {
        run_with_prelauncher(item);
    }
    else
    {
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 3, PMLOGKS("app_id", app_id.c_str()),
                                          PMLOGKS("status", "cancel_to_launch_last_app"),
                                          PMLOGKS("where", "handle_automatic_app"), "");
        finish_launching(item);
        return;
    }
}

void AppLifeManager::get_launching_app_ids(std::vector<std::string>& app_ids)
{
    for(auto& launching_item: launch_item_list_)
        app_ids.push_back(launching_item->app_id());
}

AppLaunchingItemPtr AppLifeManager::get_launching_item_by_uid(const std::string& uid)
{
    for(auto& launching_item: launch_item_list_)
    {
        if(launching_item->uid() == uid)
            return launching_item;
    }
    return NULL;
}

AppLaunchingItemPtr AppLifeManager::get_launching_item_by_app_id(const std::string& app_id)
{
    for(auto& launching_item: launch_item_list_)
    {
        if(launching_item->app_id() == app_id)
            return launching_item;
    }
    return NULL;
}

void AppLifeManager::remove_item(const std::string& uid)
{
    auto it = std::find_if(launch_item_list_.begin(), launch_item_list_.end(), [&uid](AppLaunchingItemPtr item){return (item->uid() == uid);});
    if(it == launch_item_list_.end())
        return;
    launch_item_list_.erase(it);
}

void AppLifeManager::add_item_into_automatic_pending_list(AppLaunchingItemPtr item)
{
    m_automatic_pending_list.push_back(item);
    LOG_INFO(MSGID_LAUNCH_LASTAPP, 3, PMLOGKS("app_id", item->app_id().c_str()),
                                      PMLOGKS("mode", "pending_automatic_app"),
                                      PMLOGKS("status", "added_into_list"), "");
}

void AppLifeManager::remove_item_from_automatic_pending_list(const std::string& app_id)
{
    auto it = std::find_if(m_automatic_pending_list.begin(), m_automatic_pending_list.end(),
                           [&app_id](AppLaunchingItemPtr item){ return (item->app_id() == app_id); });
    if(it != m_automatic_pending_list.end())
    {
        m_automatic_pending_list.erase(it);
        LOG_INFO(MSGID_LAUNCH_LASTAPP, 3, PMLOGKS("app_id", app_id.c_str()),
                                          PMLOGKS("mode", "pending_automatic_app"),
                                          PMLOGKS("status", "removed_from_list"), "");
    }
}

bool AppLifeManager::is_in_automatic_pending_list(const std::string& app_id)
{
    auto it = std::find_if(m_automatic_pending_list.begin(), m_automatic_pending_list.end(),
                           [&app_id](AppLaunchingItemPtr item){ return (item->app_id() == app_id); });

    if(it != m_automatic_pending_list.end()) return true;
    else return false;
}

void AppLifeManager::get_automatic_pending_app_ids(std::vector<std::string>& app_ids)
{
    for(auto& launching_item : m_automatic_pending_list)
        app_ids.push_back(launching_item->app_id());
}

void AppLifeManager::get_loading_app_ids(std::vector<std::string>& app_ids)
{
    for(auto& app: m_loading_app_list)
        app_ids.push_back(std::get<0>(app));
}

void AppLifeManager::add_loading_app(const std::string& app_id, const AppType& type)
{
    for(auto& loading_app: m_loading_app_list)
        if(std::get<0>(loading_app) == app_id) return;
    // exception
    if("com.webos.app.container" == app_id || "com.webos.app.inputcommon" == app_id)
    {
        LOG_INFO(MSGID_LOADING_LIST, 1, PMLOGKS("app_id", app_id.c_str()), "apply exception (skip)");
        return;
    }

    LoadingAppItem new_loading_app = std::make_tuple(app_id, type, static_cast<double>(get_current_time()));
    m_loading_app_list.push_back(new_loading_app);

    LOG_INFO(MSGID_LOADING_LIST, 1, PMLOGKS("app_id", app_id.c_str()), "added");

    if (is_last_launching_app(app_id))
        set_last_loading_app(app_id);
}

void AppLifeManager::remove_loading_app(const std::string& app_id)
{
    auto it = std::find_if(m_loading_app_list.begin(), m_loading_app_list.end(),
                           [&app_id](const LoadingAppItem& loading_app){ return (std::get<0>(loading_app) == app_id); });
    if(it == m_loading_app_list.end())
        return;
    m_loading_app_list.erase(it);
    LOG_INFO(MSGID_LOADING_LIST, 1, PMLOGKS("app_id", app_id.c_str()), "removed");

    if (last_loading_app_timer_set_.second == app_id)
        remove_timer_for_last_loading_app(true);
}

void AppLifeManager::set_last_loading_app(const std::string& app_id)
{
    auto it = std::find_if(m_loading_app_list.begin(), m_loading_app_list.end(),
                           [&app_id](const LoadingAppItem& loading_app){ return (std::get<0>(loading_app) == app_id); });
    if(it == m_loading_app_list.end())
        return;

    add_timer_for_last_loading_app(app_id);
    remove_last_launching_app(app_id);
}

void AppLifeManager::add_timer_for_last_loading_app(const std::string& app_id)
{
    if ((last_loading_app_timer_set_.first != 0) && (last_loading_app_timer_set_.second == app_id))
        return;

    auto it = std::find_if(m_loading_app_list.begin(), m_loading_app_list.end(),
                           [&app_id](const LoadingAppItem& loading_app){ return (std::get<0>(loading_app) == app_id); });
    if(it == m_loading_app_list.end())
        return;

    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if(app_desc == NULL)
    {
        LOG_ERROR(MSGID_LAUNCH_LASTAPP_ERR, 3, PMLOGKS("app_id", app_id.c_str()),
                                               PMLOGKS("status", "cannot_add_timer_for_last_loading_app"),
                                               PMLOGKS("reason", "null description"), "");
        return;
    }

    remove_timer_for_last_loading_app(false);

    guint timer = g_timeout_add(SettingsImpl::instance().GetLastLoadingAppTimeout(), run_last_loading_app_timeout_handler, NULL);
    last_loading_app_timer_set_ = std::make_pair(timer, app_id);
}

void AppLifeManager::remove_timer_for_last_loading_app(bool trigger)
{
    if (last_loading_app_timer_set_.first == 0) return;

    g_source_remove(last_loading_app_timer_set_.first);
    last_loading_app_timer_set_ = std::make_pair(0, "");

    if (trigger)
        trigger_to_launch_last_app();
}

gboolean AppLifeManager::run_last_loading_app_timeout_handler(gpointer user_data)
{
    AppLifeManager::instance().remove_timer_for_last_loading_app(true);
    return FALSE;
}

void AppLifeManager::add_last_launching_app(const std::string& app_id)
{
    auto it = std::find(last_launching_apps_.begin(), last_launching_apps_.end(), app_id);
    if(it != last_launching_apps_.end())
        return;

    last_launching_apps_.push_back(app_id);
}

void AppLifeManager::remove_last_launching_app(const std::string& app_id)
{
    auto it = std::find(last_launching_apps_.begin(), last_launching_apps_.end(), app_id);
    if(it == last_launching_apps_.end())
        return;

    last_launching_apps_.erase(it);
}

bool AppLifeManager::is_last_launching_app(const std::string& app_id)
{
    auto it = std::find(last_launching_apps_.begin(), last_launching_apps_.end(), app_id);
    return { (it != last_launching_apps_.end()) ? true : false };
}

void AppLifeManager::reset_last_app_candidates()
{
    last_launching_apps_.clear();
    remove_timer_for_last_loading_app(false);
}

bool AppLifeManager::has_only_preloaded_items(const std::string& app_id)
{
    for(auto& launching_item: launch_item_list_)
    {
        if(launching_item->app_id() == app_id && launching_item->preload().empty())
            return false;
    }

    for(auto& loading_item: m_loading_app_list)
    {
        if(std::get<0>(loading_item) == app_id)
            return false;
    }

    if(AppInfoManager::instance().is_running(app_id) &&
       !AppInfoManager::instance().preload_mode_on(app_id))
    {
        return false;
    }

    return true;
}

bool AppLifeManager::is_launching_item_expired(AppLaunchingItemPtr item)
{
    double current_time = get_current_time();
    double elapsed_time = current_time - item->launch_start_time();

    if(elapsed_time > SettingsImpl::instance().GetLaunchExpiredTimeout())
        return true;

    return false;
}

bool AppLifeManager::is_loading_app_expired(const std::string& app_id)
{
    double loading_start_time = 0;
    for(auto& loading_app: m_loading_app_list)
    {
        if(std::get<0>(loading_app) == app_id)
        {
          loading_start_time = std::get<2>(loading_app);
          break;
        }
    }

    if (loading_start_time == 0)
    {
        LOG_WARNING(MSGID_LAUNCH_LASTAPP, 2, PMLOGKS("status", "invalid_loading_start_time"),
                                             PMLOGKS("app_id", app_id.c_str()), "");
        return true;
    }

    double current_time = get_current_time();
    double elapsed_time = current_time - loading_start_time;

    if(elapsed_time > SettingsImpl::instance().GetLoadingExpiredTimeout())
        return true;

    return false;
}

AppLifeHandlerInterface* AppLifeManager::GetLifeHandlerForApp(const std::string& app_id) {
  AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
  if(app_desc == NULL) {
    LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("app_id", app_id.c_str()),
                                      PMLOGKS("reason", "null_description"),
                                      PMLOGKS("where", __FUNCTION__), "");
    return nullptr;
  }

  switch(app_desc->handler_type()) {
    case LifeHandlerType::Web:
      return &web_lifecycle_handler_;
      break;
    case LifeHandlerType::Qml:
      return &qml_lifecycle_handler_;
      break;
    case LifeHandlerType::Native:
      return &native_lifecycle_handler_;
      break;
    default:
      break;
  }

  return nullptr;
}
