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

#ifndef APP_LIFE_MANAGER_H_
#define APP_LIFE_MANAGER_H_

#include <lifecycle/handler/NativeAppLifeHandler.h>
#include <lifecycle/handler/QmlAppLifeHandler.h>
#include <lifecycle/handler/WebAppLifeHandler.h>
#include <lifecycle/LifecycleRouter.h>
#include <lifecycle/LifecycleTask.h>
#include <lifecycle/RunningInfoManager.h>
#include <lifecycle/stage/appitem/AppItemFactory.h>
#include <lifecycle/stage/appitem/LaunchAppItem.h>
#include <lifecycle/stage/MemoryChecker.h>
#include <lifecycle/stage/Prelauncher.h>
#include <luna-service2/lunaservice.h>
#include <package/AppPackage.h>
#include <policy/LastAppHandler.h>
#include "interface/IClassName.h"
#include "interface/ISingleton.h"
#include <tuple>

typedef std::tuple<std::string, AppType, double> LoadingAppItem;

class LifecycleManager : public ISingleton<LifecycleManager>,
                         public IClassName {
friend class ISingleton<LifecycleManager> ;
public:
    LifecycleManager();
    virtual ~LifecycleManager();

    void initialize();

    void launch(LifecycleTaskPtr task);
    void pause(LifecycleTaskPtr task);
    void close(LifecycleTaskPtr task);
    void closeAll(LifecycleTaskPtr task);

    void closeByAppId(const std::string& appId, const std::string& caller_id, const std::string& reason, std::string& errorText, bool preload_only = false, bool clear_all_items = false);
    void closeAllLoadingApps();
    void closeAllApps(bool clear_all_items = false);
    void closeApps(const std::vector<std::string>& appIds, bool clear_all_items = false);
    void handleBridgedLaunchRequest(const pbnjson::JValue& params);
    void registerApp(const std::string& appId, LSMessage* message, std::string& errorText);
    void connectNativeApp(const std::string& appId, LSMessage* message, std::string& errorText);
    void triggerToLaunchLastApp();

    void setLastLoadingApp(const std::string& appId);

    void getLaunchingAppIds(std::vector<std::string>& appIds);

    boost::signals2::signal<void(const std::string& appId, const LifeStatus& life_status)> signal_app_life_status_changed;
    boost::signals2::signal<void(const std::string& appId)> EventForegroundAppChanged;
    boost::signals2::signal<void(const pbnjson::JValue& foreground_info)> EventForegroundExtraInfoChanged;
    boost::signals2::signal<void(const pbnjson::JValue& event)> EventLifecycle;
    boost::signals2::signal<void(LaunchAppItemPtr)> EventLaunchingFinished;

private:
    static bool callbackCloseAppViaLSM(LSHandle* sh, LSMessage* message, void* context);

    LaunchAppItemPtr getLaunchingItemByUid(const std::string& uid);
    LaunchAppItemPtr getLaunchingItemByAppId(const std::string& appId);
    void removeItem(const std::string& uid);

    void addItemIntoAutomaticPendingList(LaunchAppItemPtr item);
    void removeItemFromAutomaticPendingList(const std::string& appId);
    bool isInAutomaticPendingList(const std::string& appId);
    void getAutomaticPendingAppIds(std::vector<std::string>& appIds);

    bool isFullscreenAppLoading(const std::string& new_launching_appId, const std::string& new_launching_app_uid);
    void getLoadingAppIds(std::vector<std::string>& appIds);
    void addLoadingApp(const std::string& appId, const AppType& type);
    void removeLoadingApp(const std::string& appId);
    void onWAMStatusChanged(bool isConnected);
    bool isFullscreenWindowType(const pbnjson::JValue& foreground_info);
    void clearLaunchingAndLoadingItemsByAppId(const std::string& appId);
    bool hasOnlyPreloadedItems(const std::string& appId);
    void handleAutomaticApp(const std::string& appId, bool continue_to_launch = true);
    bool isLaunchingItemExpired(LaunchAppItemPtr item);
    bool isLoadingAppExpired(const std::string& appId);

    void addTimerForLastLoadingApp(const std::string& appId);
    void removeTimerForLastLoadingApp(bool trigger);
    static gboolean runLastLoadingAppTimeoutHandler(gpointer context);

    void addLastLaunchingApp(const std::string& appId);
    void removeLastLaunchingApp(const std::string& appId);
    bool isLastLaunchingApp(const std::string& appId);
    void resetLastAppCandidates();

    // app life cycle interfaces
    void onPrelaunchingDone(const std::string& uid);
    void onMemoryCheckingDone(const std::string& uid);
    void onLaunchingDone(const std::string& uid);
    void onRunningAppAdded(const std::string& appId, const std::string& pid, const std::string& webprocid);
    void onRunningAppRemoved(const std::string& appId);
    void onRunningListChanged(const std::string& appId);
    void onMemoryCheckingStart(const std::string& uid);
    void onRuntimeStatusChanged(const std::string& appId, const std::string& uid, const RuntimeStatus& life_status);
    void setAppLifeStatus(const std::string& appId, const std::string& uid, LifeStatus new_status);
    void onForegroundInfoChanged(const pbnjson::JValue& responsePayload);

    void runWithPrelauncher(LaunchAppItemPtr item);
    void runWithMemoryChecker(LaunchAppItemPtr item);
    void runWithLauncher(LaunchAppItemPtr item);
    void runLastappHandler();
    void finishLaunching(LaunchAppItemPtr item);

    void launchApp(LaunchAppItemPtr item);
    void closeApp(const std::string& appId, const std::string& callerId, const std::string& reason, std::string& errorText, bool clearAllItems = false);
    void pauseApp(const std::string& appId, const pbnjson::JValue& params, std::string& errorText, bool report_event = true);

    void replyWithResult(LSMessage* message, const std::string& pid, bool result, const int& errorCode, const std::string& errorText);
    void replySubscriptionOnLaunch(const std::string& appId, bool show_splash);
    void replySubscriptionForAppLifeStatus(const std::string& appId, const std::string& uid, const LifeStatus& life_status);
    void postRunning(const pbnjson::JValue& running, bool devmode = false);
    void generateLifeCycleEvent(const std::string& appId, const std::string& uid, LifeEvent event);

    IAppLifeHandler* getLifeHandlerForApp(const std::string& appId);

private:
    MemoryChecker m_memoryChecker;
    LastAppHandler m_lastappHandler;
    LifecycleRouter m_lifecycleRouter;

    // member variables
    std::vector<LifecycleTaskPtr> m_lifecycleTasks;
    LaunchItemList m_appLaunchingItemList;
    LaunchItemList m_automaticPendingList;
    CloseAppItemList m_closeItemList;
    std::map<std::string, std::string> m_closeReasonInfo;
    std::vector<LoadingAppItem> m_loadingAppList;
    std::vector<std::string> m_lastLaunchingApps;
    std::pair<guint, std::string> m_lastLoadingAppTimerSet;
};

#endif

