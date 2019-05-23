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

#ifndef APP_LIFE_MANAGER_H_
#define APP_LIFE_MANAGER_H_

#include <luna-service2/lunaservice.h>
#include <tuple>

#include "core/base/singleton.h"
#include "core/lifecycle/app_info_manager.h"
#include "core/lifecycle/app_life_status.h"
#include "core/lifecycle/launching_item.h"
#include "core/lifecycle/life_handler/nativeapp_life_handler.h"
#include "core/lifecycle/life_handler/qmlapp_life_handler.h"
#include "core/lifecycle/life_handler/webapp_life_handler.h"
#include "core/lifecycle/lifecycle_task.h"
#include "core/lifecycle/memory_checker.h"
#include "core/package/application_description.h"
#include "interface/lifecycle/lastapp_handler_interface.h"
#include "interface/lifecycle/launching_item_factory_interface.h"
#include "interface/lifecycle/memory_checker_interface.h"
#include "interface/lifecycle/prelauncher_interface.h"

typedef std::tuple<std::string, AppType, double> LoadingAppItem;

class AppLifeManager: public Singleton<AppLifeManager> {
public:
    AppLifeManager();
    ~AppLifeManager();

    void init();

    void Launch(LifeCycleTaskPtr task);
    void Pause(LifeCycleTaskPtr task);
    void Close(LifeCycleTaskPtr task);
    void CloseByPid(LifeCycleTaskPtr task);
    void CloseAll(LifeCycleTaskPtr task);

    void launch(AppLaunchRequestType rtype, const std::string& app_id, const pbnjson::JValue& params, LSMessage* lsmsg);
    void close_by_app_id(const std::string& app_id, const std::string& caller_id, const std::string& reason, std::string& err_text, bool preload_only = false, bool clear_all_items = false);
    void close_by_pid(const std::string& pid, const std::string& caller_id, std::string& err_text);
    void close_all_loading_apps();
    void close_all_apps(bool clear_all_items = false);
    void close_apps(const std::vector<std::string>& app_ids, bool clear_all_items = false);
    void handle_bridged_launch_request(const pbnjson::JValue& params);
    void RegisterApp(const std::string& app_id, LSMessage* lsmsg, std::string& err_text);
    void connect_nativeapp(const std::string& app_id, LSMessage* lsmsg, std::string& err_text);
    bool change_running_app_id(const std::string& current_id, const std::string& target_id, ErrorInfo& err_info);
    void trigger_to_launch_last_app();

    void set_last_loading_app(const std::string& app_id);

    void get_launching_app_ids(std::vector<std::string>& app_ids);

    void set_applifeitem_factory(AppLaunchingItemFactoryInterface& factory);
    void set_prelauncher_handler(PrelauncherInterface& prelauncher);
    void set_memory_checker_handler(MemoryCheckerInterface& memory_checker);
    void set_lastapp_handler(LastAppHandlerInterface& lastapp_handler);

    boost::signals2::signal<void(const std::string& app_id, const LifeStatus& life_status)> signal_app_life_status_changed;
    boost::signals2::signal<void(const std::string& app_id)> signal_foreground_app_changed;
    boost::signals2::signal<void(const pbnjson::JValue& foreground_info)> signal_foreground_extra_info_changed;
    boost::signals2::signal<void(const pbnjson::JValue& event)> signal_lifecycle_event;
    boost::signals2::signal<void(AppLaunchingItemPtr)> signal_launching_finished;

private:
    friend class Singleton<AppLifeManager> ;

    static bool cb_close_app_via_lsm(LSHandle* handle, LSMessage* lsmsg, void* user_data);

    AppLaunchingItemPtr get_launching_item_by_uid(const std::string& uid);
    AppLaunchingItemPtr get_launching_item_by_app_id(const std::string& app_id);
    void remove_item(const std::string& uid);

    void add_item_into_automatic_pending_list(AppLaunchingItemPtr item);
    void remove_item_from_automatic_pending_list(const std::string& app_id);
    bool is_in_automatic_pending_list(const std::string& app_id);
    void get_automatic_pending_app_ids(std::vector<std::string>& app_ids);

    bool is_fullscreen_app_loading(const std::string& new_launching_app_id, const std::string& new_launching_app_uid);
    void get_loading_app_ids(std::vector<std::string>& app_ids);
    void add_loading_app(const std::string& app_id, const AppType& type);
    void remove_loading_app(const std::string& app_id);
    void stop_all_webapp_item();
    bool is_fullscreen_window_type(const pbnjson::JValue& foreground_info);
    void clear_launching_and_loading_items_by_app_id(const std::string& app_id);
    bool has_only_preloaded_items(const std::string& app_id);
    void handle_automatic_app(const std::string& app_id, bool continue_to_launch = true);
    bool is_launching_item_expired(AppLaunchingItemPtr item);
    bool is_loading_app_expired(const std::string& app_id);

    void add_timer_for_last_loading_app(const std::string& app_id);
    void remove_timer_for_last_loading_app(bool trigger);
    static gboolean run_last_loading_app_timeout_handler(gpointer user_data);

    void add_last_launching_app(const std::string& app_id);
    void remove_last_launching_app(const std::string& app_id);
    bool is_last_launching_app(const std::string& app_id);
    void reset_last_app_candidates();

    // app life cycle interfaces
    void on_prelaunching_done(const std::string& uid);
    void on_memory_checking_done(const std::string& uid);
    void on_launching_done(const std::string& uid);
    void on_running_app_added(const std::string& app_id, const std::string& pid, const std::string& webprocid);
    void on_running_app_removed(const std::string& app_id);
    void on_running_list_changed(const std::string& app_id);
    void on_memory_checking_start(const std::string& uid);
    void on_runtime_status_changed(const std::string& app_id, const std::string& uid, const RuntimeStatus& life_status);
    void SetAppLifeStatus(const std::string& app_id, const std::string& uid, LifeStatus new_status);
    void on_foreground_info_changed(const pbnjson::JValue& jmsg);

    void run_with_prelauncher(AppLaunchingItemPtr item);
    void run_with_memory_checker(AppLaunchingItemPtr item);
    void run_with_launcher(AppLaunchingItemPtr item);
    void run_lastapp_handler();
    void finish_launching(AppLaunchingItemPtr item);

    void launch_app(AppLaunchingItemPtr item);
    void close_app(const std::string& app_id, const std::string& caller_id, const std::string& reason, std::string& err_text, bool clear_all_items = false);
    void pause_app(const std::string& app_id, const pbnjson::JValue& params, std::string& err_text, bool report_event = true);

    void reply_with_result(LSMessage* lsmsg, const std::string& pid, bool result, const int& err_code, const std::string& err_text);
    void reply_subscription_on_launch(const std::string& app_id, bool show_splash);
    void reply_subscription_for_app_life_status(const std::string& app_id, const std::string& uid, const LifeStatus& life_status);
    void reply_subscription_for_running(const pbnjson::JValue& running_list, bool devmode = false);
    void GenerateLifeCycleEvent(const std::string& app_id, const std::string& uid, LifeEvent event);

    AppLifeHandlerInterface* GetLifeHandlerForApp(const std::string& app_id);

private:
    // extendable interface
    AppLaunchingItemFactoryInterface* launch_item_factory_;
    PrelauncherInterface* prelauncher_;
    MemoryCheckerInterface* memory_checker_;
    NativeAppLifeHandler native_lifecycle_handler_;
    WebAppLifeHandler web_lifecycle_handler_;
    QmlAppLifeHandler qml_lifecycle_handler_;
    LastAppHandlerInterface* lastapp_handler_;
    LifeCycleRouter lifecycle_router_;

    // member variables
    std::vector<LifeCycleTaskPtr> lifecycle_tasks_;
    AppLaunchingItemList launch_item_list_;
    AppCloseItemList close_item_list_;
    std::vector<std::string> m_fullscreen_window_types;
    AppLaunchingItemList m_automatic_pending_list;
    std::map<std::string, std::string> close_reason_info_;
    std::vector<LoadingAppItem> m_loading_app_list;
    std::vector<std::string> last_launching_apps_;
    std::pair<guint, std::string> last_loading_app_timer_set_;
};

#endif

