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

#ifndef LAUNCH_POINT_MANAGER_H
#define LAUNCH_POINT_MANAGER_H

#include <boost/bind.hpp>
#include <boost/signals2.hpp>
#include <bus/client/AppInstallService.h>
#include <glib.h>
#include <launchpoint/DBBase.h>
#include <launchpoint/handler/DBHandler.h>
#include <launchpoint/handler/OrderingHandler.h>
#include <launchpoint/launch_point/LaunchPoint.h>
#include <launchpoint/launch_point/LaunchPointFactory.h>
#include <luna-service2/lunaservice.h>
#include "interface/IListener.h"
#include <util/JUtil.h>
#include <util/Singleton.h>

const std::string LP_CHANGE_ADDED = "added";
const std::string LP_CHANGE_UPDATED = "updated";
const std::string LP_CHANGE_REMOVED = "removed";

enum LPMAction {
    LOAD_APPS = 1,
    DB_SYNC,
    APP_INSTALLED,
    APP_UPDATED,
    APP_UNINSTALLED,
    API_ADD,
    API_UPDATE,
    API_MOVE
};

enum class LPStateInput {
    LOAD_APPS = 0,
    LOAD_DB_DATA,
    LOAD_ORDERED_LIST
};

class LaunchPointManagerListener {
public:
    LaunchPointManagerListener() {}
    virtual ~LaunchPointManagerListener() {}

    virtual void onReadyLaunchPointManager() = 0;
};

class LaunchPointManager: public Singleton<LaunchPointManager>,
                          public IListener<LaunchPointManagerListener> {
friend class Singleton<LaunchPointManager> ;
public:
    LaunchPointManager();
    virtual ~LaunchPointManager();

    void initialize();

    LaunchPointPtr addLP(const LPMAction action, const pbnjson::JValue& data, std::string& err_text);
    LaunchPointPtr updateLP(const LPMAction action, const pbnjson::JValue& data, std::string& err_text);
    LaunchPointPtr moveLP(const LPMAction action, const pbnjson::JValue& data, std::string& err_text);
    void removeLP(const pbnjson::JValue& data, std::string& err_text);

    void removeLPsByAppId(const std::string& id);
    void convertLPsToJson(pbnjson::JValue& array);

    bool ready() const
    {
        return m_lpmReady;
    }

    boost::signals2::signal<void(const pbnjson::JValue&)> EventLaunchPointListChanged;
    boost::signals2::signal<void(const std::string&, const pbnjson::JValue&)> EventLaunchPointChanged;

private:
    void onDBConnected(bool connection);
    void onLPLoaded(const pbnjson::JValue& loaded_result);
    void onLPOrdered(const OrderChangeState& change_state);
    void onPackageStatusChanged(const std::string& app_id, const PackageStatus& status);

    void onListAppsChanged(const pbnjson::JValue& apps, const std::vector<std::string>& changes, bool dev);
    void onOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev);
    void handleLpmState(const LPStateInput& input, const pbnjson::JValue& data);
    void reloadDbData();

    void sortLPs();

    void updateApps(const pbnjson::JValue& apps);
    void syncAppsWithDbData();

    void replyLpListToSubscribers();
    void replyLpChangeToSubscribers(LaunchPointPtr lp, const std::string& change, int position = -1);

    bool isLastVisibleLp(LaunchPointPtr lp);
    LaunchPointPtr getDefaultLpByAppId(const std::string& app_id);
    std::string generateLpId();
    LaunchPointPtr getLpByLpId(const std::string& lp_id);

    bool m_lpmReady;
    bool m_dbConnected;
    bool m_dbLoaded;
    bool m_appsLoaded;
    bool m_orderedListReady;

    pbnjson::JValue m_listAppsChanges;

    pbnjson::JValue m_launchPointsDBData;
    LaunchPointList m_launchPointList;

    DBHandler m_DBHandler;
    LaunchPointFactory m_launchPointFactory;
    OrderingHandler m_orderingHandler;
};

#endif

