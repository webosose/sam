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

#include <lifecycle/AppInfoManager.h>
#include <lifecycle/AppLifeStatus.h>
#include <lifecycle/handler/NativeAppLifeHandler.h>
#include <lifecycle/handler/QmlAppLifeHandler.h>
#include <lifecycle/handler/WebAppLifeHandler.h>
#include <lifecycle/LifecycleTask.h>
#include <lifecycle/stage/appitem/AppItemFactory.h>
#include <lifecycle/stage/appitem/LaunchAppItem.h>
#include <lifecycle/stage/MemoryChecker.h>
#include <lifecycle/stage/Prelauncher.h>
#include <luna-service2/lunaservice.h>
#include <package/AppDescription.h>
#include <policy/LastAppHandler.h>
#include <util/Singleton.h>
#include <tuple>

typedef std::tuple<std::string, AppType, double> LoadingAppItem;

class LifecycleManager: public Singleton<LifecycleManager> {
friend class Singleton<LifecycleManager> ;
public:
    LifecycleManager();
    virtual ~LifecycleManager();

    void initialize();

    void launch(LifeCycleTaskPtr task);
    void Pause(LifeCycleTaskPtr task);
    void Close(LifeCycleTaskPtr task);
    void CloseByPid(LifeCycleTaskPtr task);
    void CloseAll(LifeCycleTaskPtr task);

    void closeByAppId(const std::string& app_id, const std::string& caller_id, const std::string& reason, std::string& err_text, bool preload_only = false, bool clear_all_items = false);
    void closeByPid(const std::string& pid, const std::string& caller_id, std::string& err_text);
    void closeAllLoadingApps();
    void closeAllApps(bool clear_all_items = false);
    void closeApps(const std::vector<std::string>& app_ids, bool clear_all_items = false);
    void handleBridgedLaunchRequest(const pbnjson::JValue& params);
    void registerApp(const std::string& app_id, LSMessage* lsmsg, std::string& err_text);
    void connectNativeApp(const std::string& app_id, LSMessage* lsmsg, std::string& err_text);
    void triggerToLaunchLastApp();

    void setLastLoadingApp(const std::string& app_id);

    void getLaunchingAppIds(std::vector<std::string>& app_ids);

    boost::signals2::signal<void(const std::string& app_id, const LifeStatus& life_status)> signal_app_life_status_changed;
    boost::signals2::signal<void(const std::string& app_id)> signal_foreground_app_changed;
    boost::signals2::signal<void(const pbnjson::JValue& foreground_info)> signal_foreground_extra_info_changed;
    boost::signals2::signal<void(const pbnjson::JValue& event)> signal_lifecycle_event;
    boost::signals2::signal<void(LaunchAppItemPtr)> signal_launching_finished;

private:
    static bool callbackCloseAppViaLSM(LSHandle* handle, LSMessage* lsmsg, void* user_data);

    LaunchAppItemPtr getLaunchingItemByUid(const std::string& uid);
    LaunchAppItemPtr getLaunchingItemByAppId(const std::string& app_id);
    void removeItem(const std::string& uid);

    void addItemIntoAutomaticPendingList(LaunchAppItemPtr item);
    void removeItemFromAutomaticPendingList(const std::string& app_id);
    bool isInAutomaticPendingList(const std::string& app_id);
    void getAutomaticPendingAppIds(std::vector<std::string>& app_ids);

    bool isFullscreenAppLoading(const std::string& new_launching_app_id, const std::string& new_launching_app_uid);
    void getLoadingAppIds(std::vector<std::string>& app_ids);
    void addLoadingApp(const std::string& app_id, const AppType& type);
    void removeLoadingApp(const std::string& app_id);
    void stopAllWebappItem();
    bool isFullscreenWindowType(const pbnjson::JValue& foreground_info);
    void clearLaunchingAndLoadingItemsByAppId(const std::string& app_id);
    bool hasOnlyPreloadedItems(const std::string& app_id);
    void handleAutomaticApp(const std::string& app_id, bool continue_to_launch = true);
    bool isLaunchingItemExpired(LaunchAppItemPtr item);
    bool isLoadingAppExpired(const std::string& app_id);

    void addTimerForLastLoadingApp(const std::string& app_id);
    void removeTimerForLastLoadingApp(bool trigger);
    static gboolean runLastLoadingAppTimeoutHandler(gpointer user_data);

    void addLastLaunchingApp(const std::string& app_id);
    void removeLastLaunchingApp(const std::string& app_id);
    bool isLastLaunchingApp(const std::string& app_id);
    void resetLastAppCandidates();

    // app life cycle interfaces
    void onPrelaunchingDone(const std::string& uid);
    void onMemoryCheckingDone(const std::string& uid);
    void onLaunchingDone(const std::string& uid);
    void onRunningAppAdded(const std::string& app_id, const std::string& pid, const std::string& webprocid);
    void onRunningAppRemoved(const std::string& app_id);
    void onRunningListChanged(const std::string& app_id);
    void onMemoryCheckingStart(const std::string& uid);
    void onRuntimeStatusChanged(const std::string& app_id, const std::string& uid, const RuntimeStatus& life_status);
    void setAppLifeStatus(const std::string& app_id, const std::string& uid, LifeStatus new_status);
    void onForegroundInfoChanged(const pbnjson::JValue& jmsg);

    void runWithPrelauncher(LaunchAppItemPtr item);
    void runWithMemoryChecker(LaunchAppItemPtr item);
    void runWithLauncher(LaunchAppItemPtr item);
    void runLastappHandler();
    void finishLaunching(LaunchAppItemPtr item);

    void launchApp(LaunchAppItemPtr item);
    void closeApp(const std::string& appId, const std::string& callerId, const std::string& reason, std::string& errText, bool clearAllItems = false);
    void pauseApp(const std::string& app_id, const pbnjson::JValue& params, std::string& err_text, bool report_event = true);

    void replyWithResult(LSMessage* lsmsg, const std::string& pid, bool result, const int& err_code, const std::string& err_text);
    void replySubscriptionOnLaunch(const std::string& app_id, bool show_splash);
    void replySubscriptionForAppLifeStatus(const std::string& app_id, const std::string& uid, const LifeStatus& life_status);
    void replySubscriptionForRunning(const pbnjson::JValue& running_list, bool devmode = false);
    void generateLifeCycleEvent(const std::string& app_id, const std::string& uid, LifeEvent event);

    IAppLifeHandler* getLifeHandlerForApp(const std::string& app_id);

private:
    // extendable interface
    Prelauncher m_prelauncher;
    MemoryChecker m_memoryChecker;
    WebAppLifeHandler m_webLifecycleHandler;
    QmlAppLifeHandler m_qmlLifecycleHandler;
    LastAppHandler m_lastappHandler;
    LifeCycleRouter m_lifecycleRouter;

    // member variables
    std::vector<LifeCycleTaskPtr> m_lifecycleTasks;
    LaunchItemList m_appLaunchingItemList;
    LaunchItemList m_automaticPendingList;
    CloseAppItemList m_closeItemList;
    std::vector<std::string> m_fullscreenWindowTypes;
    std::map<std::string, std::string> m_closeReasonInfo;
    std::vector<LoadingAppItem> m_loadingAppList;
    std::vector<std::string> m_lastLaunchingApps;
    std::pair<guint, std::string> m_lastLoadingAppTimerSet;
};

#endif

