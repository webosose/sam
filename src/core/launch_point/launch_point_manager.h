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
#include <boost/signals.hpp>
#include <core/util/jutil.h>
#include <core/util/singleton.h>
#include <glib.h>
#include <luna-service2/lunaservice.h>

#include "core/base/db_base.h"
#include "core/launch_point/handler/db_handler.h"
#include "core/launch_point/handler/ordering_handler.h"
#include "core/launch_point/launch_point/launch_point.h"
#include "core/launch_point/launch_point/launch_point_factory.h"
#include "core/module/subscriber_of_appinstalld.h"

const std::string LP_CHANGE_ADDED = "added";
const std::string LP_CHANGE_UPDATED = "updated";
const std::string LP_CHANGE_REMOVED = "removed";

enum LPMAction {
    LOAD_APPS = 1, DB_SYNC, APP_INSTALLED, APP_UPDATED, APP_UNINSTALLED, API_ADD, API_UPDATE, API_MOVE
};

class LaunchPointManager: public Singleton<LaunchPointManager> {
public:
    LaunchPointManager();
    ~LaunchPointManager();

    void Init();

    LaunchPointPtr AddLaunchPoint(const LPMAction action, const pbnjson::JValue& data, std::string& err_text);
    LaunchPointPtr UpdateLaunchPoint(const LPMAction action, const pbnjson::JValue& data, std::string& err_text);
    LaunchPointPtr MoveLaunchPoint(const LPMAction action, const pbnjson::JValue& data, std::string& err_text);
    void RemoveLaunchPoint(const pbnjson::JValue& data, std::string& err_text);
    void SearchLaunchPoints(pbnjson::JValue& matchedByTitle, const std::string& searchTerm);

    void RemoveAllLaunchPointsByAppId(const std::string& id);
    void LaunchPointsAsJson(pbnjson::JValue& array);

    void SetDbHandler(DbHandlerInterface& db_handler);
    void SetOrderingHandler(OrderingHandlerInterface& ordering_handler);
    void SetLaunchPointFactory(LaunchPointFactoryInterface& lp_factory);

    bool Ready() const
    {
        return lpm_ready_;
    }
    boost::signal<void()> signal_on_launch_point_ready_;
    void SubscribeListChange(boost::function<void(const pbnjson::JValue&)> func)
    {
        notify_launch_point_list_changed.connect(func);
    }
    void SubscribeLaunchPointChange(boost::function<void(const std::string& change, const pbnjson::JValue&)> func)
    {
        notify_launch_point_changed.connect(func);
    }

private:
    friend class Singleton<LaunchPointManager> ;

    enum class LPStateInput {
        LOAD_APPS = 0, LOAD_DB_DATA, LOAD_ORDERED_LIST
    };

    void HandleDbConnected(bool connection);
    void OnLaunchPointsDbLoaded(const pbnjson::JValue& loaded_result);
    void OnLaunchPointsOrdered(const OrderChangeState& change_state);
    void OnPackageStatusChanged(const std::string& app_id, const PackageStatus& status);

    void OnListAppsChanged(const pbnjson::JValue& apps, const std::vector<std::string>& changes, bool dev);
    void OnOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev);
    void HandleLpmState(const LPStateInput& input, const pbnjson::JValue& data);
    void ReloadDbData();

    void MakeLaunchPointsInOrder();

    void UpdateApps(const pbnjson::JValue& apps);
    void SyncAppsWithDbData();

    void ReplyLpListToSubscribers();
    void ReplyLpChangeToSubscribers(LaunchPointPtr lp, const std::string& change, int position = -1);
    boost::signal<void(const pbnjson::JValue&)> notify_launch_point_list_changed;
    boost::signal<void(const std::string&, const pbnjson::JValue&)> notify_launch_point_changed;

    bool IsLastVisibleLp(LaunchPointPtr lp);
    LaunchPointPtr GetDefaultLpByAppId(const std::string& app_id);
    std::string GenerateLpId();
    LaunchPointPtr GetLpByLpId(const std::string& lp_id);
    bool MatchesTitle(const gchar* keyword, const gchar* title) const;

    bool lpm_ready_;
    bool db_connected_;
    bool db_loaded_;
    bool apps_loaded_;
    bool ordered_list_ready_;

    pbnjson::JValue list_apps_changes_;

    pbnjson::JValue launch_points_db_data_;
    LaunchPointList launch_point_list_;

    DbHandlerInterface* db_handler_;
    LaunchPointFactoryInterface* lp_factory_;
    OrderingHandlerInterface* ordering_handler_;
};

#endif

