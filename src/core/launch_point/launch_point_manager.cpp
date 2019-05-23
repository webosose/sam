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

#include "core/launch_point/launch_point_manager.h"

#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <iterator>
#include <sys/time.h>

#include "core/base/lsutils.h"
#include "core/base/logging.h"
#include "core/bus/appmgr_service.h"
#include "core/lifecycle/app_life_manager.h"
#include "core/module/service_observer.h"
#include "core/package/application_manager.h"

LaunchPointManager::LaunchPointManager() :
        lpm_ready_(false), db_connected_(false), db_loaded_(false), apps_loaded_(false), ordered_list_ready_(false), list_apps_changes_(pbnjson::Array()), launch_points_db_data_(pbnjson::Array()), db_handler_(
                nullptr), lp_factory_(nullptr), ordering_handler_(nullptr)
{
}

LaunchPointManager::~LaunchPointManager()
{
}

void LaunchPointManager::Init()
{

    ServiceObserver::instance().Add(WEBOS_SERVICE_DB, boost::bind(&LaunchPointManager::HandleDbConnected, &LaunchPointManager::instance(), _1));

    AppinstalldSubscriber::instance().SubscribeInstallStatus(boost::bind(&LaunchPointManager::OnPackageStatusChanged, this, _1, _2));

    ApplicationManager::instance().signalListAppsChanged.connect(boost::bind(&LaunchPointManager::OnListAppsChanged, this, _1, _2, _3));

    ApplicationManager::instance().signalOneAppChanged.connect(boost::bind(&LaunchPointManager::OnOneAppChanged, this, _1, _2, _3, _4));
}

/**********************************************************/
/**** Set Extendable Handler ******************************/
/**********************************************************/
void LaunchPointManager::SetDbHandler(DbHandlerInterface& db_handler)
{

    db_handler_ = &db_handler;

    if (db_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "db_handler"), PMLOGKS("where", "SetDbHandler"), "");
        return;
    }

    db_handler_->signal_db_loaded_.connect(boost::bind(&LaunchPointManager::OnLaunchPointsDbLoaded, this, _1));
    db_handler_->Init();
}

void LaunchPointManager::SetOrderingHandler(OrderingHandlerInterface& ordering_handler)
{

    ordering_handler_ = &ordering_handler;

    if (ordering_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "SetOrderingHandler"), "");
        return;
    }

    ordering_handler_->signal_launch_points_ordered_.connect(boost::bind(&LaunchPointManager::OnLaunchPointsOrdered, this, _1));
    ordering_handler_->Init();
}

void LaunchPointManager::SetLaunchPointFactory(LaunchPointFactoryInterface& lp_factory)
{
    lp_factory_ = &lp_factory;
}

/***********************************************************/
/** handling LPM State (listApps, DB, Ordering)*************/
/***********************************************************/
void LaunchPointManager::HandleLpmState(const LPStateInput& input, const pbnjson::JValue& data)
{

    LOG_INFO(MSGID_LAUNCH_POINT, 6, PMLOGKS("status", "handle_lpm_state"), PMLOGKFV("state_input", "%d", (int)input), PMLOGKS("load_app", apps_loaded_?"done":"not_ready_yet"),
            PMLOGKS("load_db", db_loaded_?"done":"not_ready_yet"), PMLOGKS("load_ordered_list", ordered_list_ready_?"done":"not_ready_yet"), PMLOGKS("lpm_ready", lpm_ready_?"ready":"not_ready_yet"),
            "");

    if (LPStateInput::LOAD_APPS == input) {
        if (apps_loaded_ && db_loaded_) {
            UpdateApps(data);
            ReloadDbData();
        } else if (!apps_loaded_ && db_loaded_) {
            UpdateApps(data);
            SyncAppsWithDbData();
            MakeLaunchPointsInOrder();
        } else {
            UpdateApps(data);
        }
    } else if (LPStateInput::LOAD_DB_DATA == input) {
        if (apps_loaded_) {
            SyncAppsWithDbData();
            MakeLaunchPointsInOrder();
        }
    }

    if (apps_loaded_ && db_loaded_ && ordered_list_ready_) {
        LOG_INFO(MSGID_LAUNCH_POINT, 1, PMLOGKS("status", "lpm_ready"), "all conditions are ready");

        lpm_ready_ = true;

        ReplyLpListToSubscribers();
        signal_on_launch_point_ready_();
    } else {
        lpm_ready_ = false;
    }
}

/***********************************************************/
/** handling DB (connect/load/update) **********************/
/***********************************************************/
void LaunchPointManager::HandleDbConnected(bool connection)
{

    db_connected_ = connection;
    if (!db_connected_)
        return;

    if (db_handler_ != nullptr)
        db_handler_->HandleDbState(db_connected_);

    if (ordering_handler_ != nullptr)
        ordering_handler_->HandleDbState(db_connected_);
}

void LaunchPointManager::OnLaunchPointsDbLoaded(const pbnjson::JValue& loaded_db_result)
{

    launch_points_db_data_ = loaded_db_result.duplicate();
    db_loaded_ = true;

    HandleLpmState(LPStateInput::LOAD_DB_DATA, loaded_db_result);
}

void LaunchPointManager::ReloadDbData()
{

    db_loaded_ = false;
    if (!db_connected_)
        return;

    if (db_handler_ != nullptr)
        db_handler_->ReloadDbData(db_connected_);

    if (ordering_handler_ != nullptr)
        ordering_handler_->ReloadDbData(db_connected_);
}

/***********************************************************/
/** handling Ordering (make/update) ************************/
/***********************************************************/
void LaunchPointManager::MakeLaunchPointsInOrder()
{

    ordered_list_ready_ = false;

    std::vector<LaunchPointPtr> visible_list;
    for (auto it : launch_point_list_) {
        if (it->IsVisible())
            visible_list.push_back(it);
    }

    if (ordering_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "MakeLaunchPointsInOrder"), "");
        return;
    }

    ordering_handler_->MakeLaunchPointsInOrder(visible_list, list_apps_changes_);
}

void LaunchPointManager::OnLaunchPointsOrdered(const OrderChangeState& change_state)
{

    LOG_INFO(MSGID_LAUNCH_POINT, 3, PMLOGKS("status", "received_changed_order_list"), PMLOGKFV("change_state", "%d", (int)change_state), PMLOGKS("where", "OnLaunchPointsOrdered"), "");

    if (change_state == OrderChangeState::FULL) {
        ordered_list_ready_ = true;
        HandleLpmState(LPStateInput::LOAD_ORDERED_LIST, pbnjson::Object());
    }
}

/***********************************************************/
/** handling Applist (listApps/update/syncDB) **************/
/***********************************************************/
void LaunchPointManager::OnPackageStatusChanged(const std::string& app_id, const PackageStatus& status)
{

    if (PackageStatus::AboutToUninstall == status)
        RemoveAllLaunchPointsByAppId(app_id);
}

void LaunchPointManager::OnListAppsChanged(const pbnjson::JValue& apps, const std::vector<std::string>& changes, bool dev)
{

    if (dev)
        return;

    list_apps_changes_ = pbnjson::Array();
    for (const auto& c : changes) {
        list_apps_changes_.append(c);
    }

    LOG_INFO(MSGID_LAUNCH_POINT, 3,
             PMLOGKS("status", "full_list_changed_on_list_apps"),
             PMLOGKS("change_reason", list_apps_changes_.stringify().c_str()),
             PMLOGKS("where", "OnListAppsChanged"), "");

    HandleLpmState(LPStateInput::LOAD_APPS, apps);
}

void LaunchPointManager::OnOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev)
{

    if (dev)
        return;

    std::string app_id = app["id"].asString();
    std::string err_text;

    LOG_INFO(MSGID_LAUNCH_POINT, 4, PMLOGKS("status", "one_app_changed_on_list_apps"), PMLOGKS("app_id", app_id.c_str()), PMLOGKS("change_reason", change.c_str()), PMLOGKS("where", "OnOneAppChanged"),
            "");

    if (LP_CHANGE_ADDED == change) {
        AddLaunchPoint(LPMAction::APP_INSTALLED, app, err_text);
    } else if (LP_CHANGE_UPDATED == change) {
        UpdateLaunchPoint(LPMAction::APP_UPDATED, app, err_text);
    } else if (LP_CHANGE_REMOVED == change) {
        RemoveAllLaunchPointsByAppId(app_id);
    }
}

void LaunchPointManager::UpdateApps(const pbnjson::JValue& apps)
{

    std::string err_text;
    launch_point_list_.clear();

    for (int i = 0; i < apps.arraySize(); ++i) {
        std::string app_id = apps[i]["id"].asString();
        LaunchPointPtr lp = AddLaunchPoint(LPMAction::LOAD_APPS, apps[i], err_text);

        if (lp == nullptr)
            LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 3, PMLOGKS("status", "lp_is_nullptr"), PMLOGKS("app_id", app_id.c_str()), PMLOGKS("where", "update_apps"), "%s", err_text.c_str());
    }

    apps_loaded_ = true;
}

void LaunchPointManager::SyncAppsWithDbData()
{

    std::string app_id, lp_id, err_text;
    LPType lp_type;

    int db_data_idx = launch_points_db_data_.arraySize();

    LOG_INFO(MSGID_LAUNCH_POINT, 2, PMLOGKS("status", "sync_apps_with_db_data"), PMLOGKS("action", "start_sync"), "");
    if (db_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "db_handler"), PMLOGKS("where", "SyncAppsWithDbData"), "");
        return;
    }

    for (int idx = 0; idx < db_data_idx; ++idx) {
        pbnjson::JValue delete_json = pbnjson::Object();

        app_id = launch_points_db_data_[idx]["id"].asString();
        lp_id = launch_points_db_data_[idx]["launchPointId"].asString();
        lp_type = static_cast<LPType>(launch_points_db_data_[idx]["lptype"].asNumber<int>());

        if (app_id.empty() || lp_type == LPType::UNKNOWN) {
            delete_json.put("launchPointId", lp_id);
            db_handler_->DeleteData(delete_json);
            continue;
        }

        LaunchPointPtr default_lp = GetDefaultLpByAppId(app_id);
        if (default_lp == nullptr) {
            delete_json.put("launchPointId", lp_id);
            db_handler_->DeleteData(delete_json);
            continue;
        }

        if (LPType::DEFAULT == lp_type) {
            UpdateLaunchPoint(LPMAction::DB_SYNC, launch_points_db_data_[idx], err_text);
        } else if (LPType::BOOKMARK == lp_type) {
            AddLaunchPoint(LPMAction::DB_SYNC, launch_points_db_data_[idx], err_text);
        }
    }

    LOG_INFO(MSGID_LAUNCH_POINT, 2, PMLOGKS("status", "sync_apps_with_db_data"), PMLOGKS("action", "end_sync"), "");
}

/***********************************************************/
/** add launch point ****************************************/
/** Action Item: load_apps/db_sync/app_installed/api_add ***/
/***********************************************************/
LaunchPointPtr LaunchPointManager::AddLaunchPoint(const LPMAction action, const pbnjson::JValue& data, std::string& err_text)
{

    std::string app_id, lp_id;
    int position = DEFAULT_POSITION_INVALID;
    LPType lp_type = LPType::UNKNOWN;
    pbnjson::JValue lp_as_json = pbnjson::Object();

    if (!data.hasKey("id") || data["id"].asString(app_id) != CONV_OK) {
        err_text = "id is empty";
        return nullptr;
    }

    LaunchPointPtr default_lp = GetDefaultLpByAppId(app_id);
    if (default_lp == nullptr && (LPMAction::DB_SYNC == action || LPMAction::API_ADD == action)) {
        err_text = "cannot find default launch point";
        return nullptr;
    }

    if (ordering_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "AddLaunchPoint"), "");
        err_text = "internal error";
        return nullptr;
    }

    if (db_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "db_handler"), PMLOGKS("where", "AddLaunchPoint"), "");
        err_text = "internal error";
        return nullptr;
    }

    if (lp_factory_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "lp_factory"), PMLOGKS("where", "AddLaunchPoint"), "");
        err_text = "internal error";
        return nullptr;
    }

    switch (action) {
    case LPMAction::LOAD_APPS:
    case LPMAction::APP_INSTALLED:
        lp_type = LPType::DEFAULT;
        lp_id = app_id + "_default";
        break;
    case LPMAction::DB_SYNC:
        lp_type = LPType::BOOKMARK;
        lp_id = data["launchPointId"].asString();
        break;
    case LPMAction::API_ADD:
        lp_type = LPType::BOOKMARK;
        lp_id = GenerateLpId();
        break;
    default:
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2, PMLOGKS("status", "add_launch_point"), PMLOGKS("action", "unknown_request"), "");
        err_text = "unknown action";
        return nullptr;
    }

    if (lp_id.empty()) {
        err_text = "fail to generate launch point id";
        return nullptr;
    }

    LaunchPointPtr lp = lp_factory_->CreateLaunchPoint(lp_type, lp_id, data, err_text);
    if (lp == nullptr)
        return nullptr;

    switch (action) {
    case LPMAction::LOAD_APPS:
        break;
    case LPMAction::DB_SYNC:
        lp->SetStoredInDb(true);
        lp->UpdateIfEmptyTitle(default_lp);
        break;
    case LPMAction::APP_INSTALLED:
        position = ordering_handler_->InsertLpInOrder(lp_id, data);
        if (position == DEFAULT_POSITION_INVALID) {
            err_text = "fail to add in proper position";
            return nullptr;
        }

        ReplyLpChangeToSubscribers(lp, LP_CHANGE_ADDED, position);
        break;
    case LPMAction::API_ADD:
        lp->UpdateIfEmpty(default_lp);
        // If there's no need to support i18n, fill in title and put it into DB.
        if (data.hasKey("supportI18nTitle") && !data["supportI18nTitle"].asBool())
            lp->UpdateIfEmptyTitle(default_lp);

        position = ordering_handler_->InsertLpInOrder(lp_id, data);
        if (position == DEFAULT_POSITION_INVALID) {
            err_text = "fail to add in proper position";
            return nullptr;
        }

        lp_as_json = lp->ToJValue().duplicate();
        lp_as_json.put("lptype", static_cast<int>(lp->LpType()));

        if (db_handler_->InsertData(lp_as_json))
            lp->SetStoredInDb(true);

        lp->UpdateIfEmptyTitle(default_lp);
        ReplyLpChangeToSubscribers(lp, LP_CHANGE_ADDED, position);
        break;
    default:
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2, PMLOGKS("status", "add_launch_point"), PMLOGKS("action", "unknown_request"), "");
        err_text = "unknown action";
        return nullptr;
    }

    // prevent too much log
    if ((LPMAction::LOAD_APPS != action) && (LPMAction::DB_SYNC != action)) {
        LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 5, PMLOGKS("status", "add_launch_point"), PMLOGKS("app_id", app_id.c_str()), PMLOGKFV("lp_type", "%d", (int)lp_type),
                PMLOGKFV("lp_action", "%d", (int)action), PMLOGKFV("position", "%d", position), "");
    }
    launch_point_list_.push_back(lp);

    return lp;
}

/***********************************************************/
/** update launch point *************************************/
/** Action Item: db_sync/app_updated/api_updated ***********/
/***********************************************************/
LaunchPointPtr LaunchPointManager::UpdateLaunchPoint(const LPMAction action, const pbnjson::JValue& data, std::string& err_text)
{

    if (data.objectSize() < 2) {
        err_text = "insufficient parameters";
        return nullptr;
    }

    if (ordering_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "UpdateLaunchPoint"), "");
        err_text = "internal error";
        return nullptr;
    }

    if (db_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "db_handler"), PMLOGKS("where", "UpdateLaunchPoint"), "");
        err_text = "internal error";
        return nullptr;
    }

    std::string lp_id, app_id;
    LaunchPointPtr lp_by_app_id = nullptr;
    LaunchPointPtr lp_by_lp_id = nullptr;
    LaunchPointPtr updated_lp = nullptr;
    pbnjson::JValue lp_as_json = pbnjson::Object();
    pbnjson::JValue delete_json = pbnjson::Object();
    pbnjson::JValue original_json = pbnjson::Object();
    int position = DEFAULT_POSITION_INVALID;

    switch (action) {
    case LPMAction::API_UPDATE:
    case LPMAction::DB_SYNC:
        lp_id = data["launchPointId"].asString();
        break;
    case LPMAction::APP_UPDATED:
        app_id = data["id"].asString();
        lp_by_app_id = GetDefaultLpByAppId(app_id);

        if (lp_by_app_id != nullptr) {
            lp_id = lp_by_app_id->LaunchPointId();
        }
        break;
    default:
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2, PMLOGKS("status", "update_launch_point"), PMLOGKS("action", "unknown_request"), "");
        err_text = "unknown action";
        return nullptr;
    }

    if (lp_id.empty()) {
        err_text = "launch point id is empty";
        return nullptr;
    }

    lp_by_lp_id = GetLpByLpId(lp_id);
    if (lp_by_lp_id == nullptr) {
        err_text = "cannot find launch point";
        return nullptr;
    }

    original_json = lp_by_lp_id->ToJValue();
    lp_by_lp_id->Update(data);

    switch (action) {
    case LPMAction::DB_SYNC:
        lp_by_lp_id->SetStoredInDb(true);
        break;
    case LPMAction::APP_UPDATED:
        if (lp_by_lp_id->IsStoredInDb()) {
            delete_json.put("launchPointId", lp_by_lp_id->LaunchPointId());
            db_handler_->DeleteData(delete_json);
        }

        updated_lp = lp_factory_->CreateLaunchPoint(LPType::DEFAULT, lp_id, data, err_text);
        if (updated_lp == nullptr)
            return nullptr;

        launch_point_list_.remove_if([=](LaunchPointPtr p) {return p->LaunchPointId() == lp_id;});
        launch_point_list_.push_back(updated_lp);
        ReplyLpChangeToSubscribers(updated_lp, LP_CHANGE_UPDATED);

        break;
    case LPMAction::API_UPDATE:
        position = ordering_handler_->UpdateLpInOrder(lp_id, data);

        if ((original_json == lp_by_lp_id->ToJValue()) && (position == DEFAULT_POSITION_INVALID)) {
            LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2, PMLOGKS("status", "update_launch_point"), PMLOGKS("action", "no_difference"), "");
            break;
        }

        lp_as_json = data.duplicate();
        lp_as_json.put("id", lp_by_lp_id->Id());
        lp_as_json.put("launchPointId", lp_by_lp_id->LaunchPointId());
        lp_as_json.put("lptype", static_cast<int>(lp_by_lp_id->LpType()));

        if (lp_by_lp_id->IsStoredInDb()) {
            db_handler_->UpdateData(lp_as_json);
        } else {
            if (db_handler_->InsertData(lp_as_json))
                lp_by_lp_id->SetStoredInDb(true);
        }
        ReplyLpChangeToSubscribers(lp_by_lp_id, LP_CHANGE_UPDATED, position);
        break;
    default:
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2, PMLOGKS("status", "update_launch_point"), PMLOGKS("action", "unknown_request"), "");
        err_text = "unknown action";
        return nullptr;
    }

    // prevent too much log
    if (LPMAction::DB_SYNC != action)
        LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 5, PMLOGKS("status", "update_launch_point"), PMLOGKS("app_id", app_id.c_str()), PMLOGKFV("lp_type", "%d", (int)lp_by_lp_id->LpType()),
                PMLOGKFV("lp_action", "%d", (int)action), PMLOGKFV("position", "%d", position), "");
    return lp_by_lp_id;
}

/***********************************************************/
/** move launch point ***************************************/
/** Action Item: api_move **********************************/
/***********************************************************/
LaunchPointPtr LaunchPointManager::MoveLaunchPoint(const LPMAction action, const pbnjson::JValue& data, std::string& err_text)
{

    if (data.objectSize() < 2) {
        err_text = "insufficient parameters";
        return nullptr;
    }

    if (ordering_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "MoveLaunchPoint"), "");
        err_text = "internal error";
        return nullptr;
    }

    std::string lp_id, app_id;
    LaunchPointPtr lp_by_lp_id = nullptr;
    int position = DEFAULT_POSITION_INVALID;

    switch (action) {
    case LPMAction::API_MOVE:
        lp_id = data["launchPointId"].asString();
        break;
    default:
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2, PMLOGKS("status", "move_launch_point"), PMLOGKS("action", "unknown_request"), "");
        err_text = "unknown action";
        return nullptr;
    }

    if (lp_id.empty()) {
        err_text = "launch point id is empty";
        return nullptr;
    }

    lp_by_lp_id = GetLpByLpId(lp_id);
    if (lp_by_lp_id == nullptr) {
        err_text = "cannot find launch point";
        return nullptr;
    }

    if (lp_by_lp_id->IsUnmovable() || !(lp_by_lp_id->IsVisible())) {
        err_text = "app is unmovable or invisible, cannot change the position of this app";
        return nullptr;
    }

    position = data["position"].asNumber<int>();

    if (position < 0) {
        err_text = "position number should be over 0";
        return nullptr;
    }

    LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 5, PMLOGKS("status", "move_launch_point"), PMLOGKS("app_id", lp_by_lp_id->Id().c_str()), PMLOGKFV("lp_type", "%d", (int)lp_by_lp_id->LpType()),
            PMLOGKFV("lp_action", "%d", (int)action), PMLOGKFV("position", "%d", position), "");
    switch (action) {
    case LPMAction::API_MOVE:
        ordering_handler_->UpdateLpInOrder(lp_id, data, position);
        ReplyLpChangeToSubscribers(lp_by_lp_id, LP_CHANGE_UPDATED, position);
        break;
    default:
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2, PMLOGKS("status", "move_launch_point"), PMLOGKS("action", "unknown_request"), "");
        err_text = "unknown action";
        return nullptr;
    }

    return lp_by_lp_id;
}

/***********************************************************/
/** remove launch point *************************************/
/***********************************************************/
void LaunchPointManager::RemoveLaunchPoint(const pbnjson::JValue& data, std::string& err_text)
{

    std::string lp_id = data["launchPointId"].asString();
    if (lp_id.empty()) {
        err_text = "launch point id is empty";
        return;
    }

    if (ordering_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "RemoveLaunchPoint"), "");
        err_text = "internal error";
        return;
    }

    if (db_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "db_handler"), PMLOGKS("where", "RemoveLaunchPoint"), "");
        err_text = "internal error";
        return;
    }

    LaunchPointPtr lp = GetLpByLpId(lp_id);
    if (lp == nullptr) {
        err_text = "cannot find launch point";
        return;
    }

    if (!lp->IsRemovable()) {
        err_text = "this launch point cannot be removable";
        return;
    }

    LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 3, PMLOGKS("status", "remove_launch_point"), PMLOGKS("app_id", (lp->Id()).c_str()), PMLOGKFV("lp_type", "%d", static_cast<int>(lp->LpType())), "");

    if (LPType::DEFAULT == lp->LpType()) {
        ApplicationManager::instance().UninstallApp(lp->Id(), err_text);
        ordering_handler_->DeleteLpInOrder(lp->LaunchPointId());
    } else if (LPType::BOOKMARK == lp->LpType()) {
        pbnjson::JValue delete_json = pbnjson::Object();
        delete_json.put("launchPointId", lp->LaunchPointId());

        ReplyLpChangeToSubscribers(lp, LP_CHANGE_REMOVED);
        db_handler_->DeleteData(delete_json);
        ordering_handler_->DeleteLpInOrder(lp->LaunchPointId());

        if (IsLastVisibleLp(lp)) {
            std::string error_text;
            LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 3, PMLOGKS("status", "remove_launch_point"), PMLOGKS("last_visible_app", (lp->Id()).c_str()), PMLOGKS("status", "closed"), "");
            AppLifeManager::instance().closeByAppId(lp->Id(), "", "", error_text, false, true);
        }

        launch_point_list_.remove_if([=](LaunchPointPtr p) {return p->LaunchPointId() == lp->LaunchPointId();});
    }
}

/***********************************************************/
/** subscription reply *************************************/
/** Action Item: listLaunchPoint (full/partial) ************/
/***********************************************************/
void LaunchPointManager::ReplyLpListToSubscribers()
{

    // payload
    pbnjson::JValue launch_points = pbnjson::Array();
    LaunchPointsAsJson(launch_points);

    // notify changes
    notify_launch_point_list_changed(launch_points);

    // reset change reason
    list_apps_changes_ = pbnjson::Array();
}

void LaunchPointManager::ReplyLpChangeToSubscribers(LaunchPointPtr lp, const std::string& change, int position)
{

    if ((LPType::DEFAULT == lp->LpType()) && !lp->IsVisible())
        return;

    // payload
    pbnjson::JValue json = lp->ToJValue();
    json.put("change", change);
    if (DEFAULT_POSITION_INVALID != position && (LP_CHANGE_ADDED == change || LP_CHANGE_UPDATED == change)) {
        json.put("position", position);
    }

    notify_launch_point_changed(change, json);
}

/***********************************************************/
/** common util function ***********************************/
/***********************************************************/
bool LaunchPointManager::IsLastVisibleLp(LaunchPointPtr lp)
{

    for (auto it : launch_point_list_) {
        if ((it->Id() == lp->Id()) && (it->LaunchPointId() != lp->LaunchPointId()) && (it->IsVisible()))
            return false;
    }
    return true;
}

void LaunchPointManager::RemoveAllLaunchPointsByAppId(const std::string& id)
{

    if (ordering_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "RemoveAllLaunchPointsByAppId"), "");
        return;
    }

    if (db_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "db_handler"), PMLOGKS("where", "RemoveAllLaunchPointsByAppId"), "");
        return;
    }

    for (auto it = launch_point_list_.begin(); it != launch_point_list_.end();) {
        if ((*it)->Id() == id) {
            pbnjson::JValue delete_json = pbnjson::Object();
            delete_json.put("launchPointId", (*it)->LaunchPointId());

            ReplyLpChangeToSubscribers(*it, LP_CHANGE_REMOVED);

            db_handler_->DeleteData(delete_json);
            ordering_handler_->DeleteLpInOrder((*it)->LaunchPointId());

            it = launch_point_list_.erase(it);
        } else
            ++it;
    }
}

std::string LaunchPointManager::GenerateLpId()
{

    while (true) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        double verifier = tv.tv_usec;

        std::string new_lp_id = boost::lexical_cast<std::string>(verifier);
        if (GetLpByLpId(new_lp_id) == nullptr)
            return new_lp_id;
    }

    return std::string("");
}

LaunchPointPtr LaunchPointManager::GetDefaultLpByAppId(const std::string& app_id)
{

    LaunchPointPtr default_lp = nullptr;

    for (auto lp : launch_point_list_) {
        if ((app_id == lp->Id()) && (LPType::DEFAULT == lp->LpType())) {
            default_lp = lp;
            break;
        }
    }

    return default_lp;
}

LaunchPointPtr LaunchPointManager::GetLpByLpId(const std::string& lp_id)
{
    LaunchPointPtr lp_by_lp_id = nullptr;

    for (auto lp : launch_point_list_) {
        if (lp_id == lp->LaunchPointId()) {
            lp_by_lp_id = lp;
            break;
        }
    }

    return lp_by_lp_id;
}

void LaunchPointManager::LaunchPointsAsJson(pbnjson::JValue& array)
{
    if (ordering_handler_ == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "LaunchPointsAsJson"), "");
        return;
    }

    std::vector<std::string> ordered_list = ordering_handler_->GetOrderedList();

    for (auto& lp_id : ordered_list) {
        auto it = find_if(launch_point_list_.begin(), launch_point_list_.end(), [&](LaunchPointPtr lp) {return lp->LaunchPointId() == lp_id;});

        if ((it != launch_point_list_.end()) && (*it)->IsVisible())
            array.append((*it)->ToJValue());
    }
}

void LaunchPointManager::SearchLaunchPoints(pbnjson::JValue& matchedByTitle, const std::string& searchTerm)
{

    if (searchTerm.empty() || !matchedByTitle.isArray())
        return;

    gchar* keyword = g_utf8_strdown(searchTerm.c_str(), -1);

    for (auto lp : launch_point_list_) {
        if (!lp->IsVisible())
            continue;

        gchar* title = g_utf8_strdown(lp->Title().c_str(), -1);

        if (MatchesTitle(keyword, title)) {
            pbnjson::JValue obj = pbnjson::Object();
            obj.put("launchPointId", lp->LaunchPointId());
            obj.put("appId", lp->Id());
            obj.put("title", lp->Title());
            obj.put("icon", lp->Icon());
            matchedByTitle.append(obj);
        }

        if (title)
            g_free(title);
    }

    if (keyword)
        g_free(keyword);
}

bool LaunchPointManager::MatchesTitle(const gchar* keyword, const gchar* title) const
{

    if (!keyword || !title)
        return false;

    if (g_str_has_prefix(title, keyword))
        return true;

    static const gchar* delimiters = " ,._-:;()\\[]{}\"/";
    static size_t len = strlen(delimiters);
    bool matches = false;
    const gchar* start = title;
    while (start != NULL) {
        start = strstr(start, keyword);

        // have we hit the end?
        if (start == NULL || start == title)
            break;

        // is the previous character in our delimiter set?
        const gchar c[] = { *g_utf8_prev_char(start), '\0' };
        if (strcspn(delimiters, c) < len) {
            matches = true;
            break;
        }
        start = g_utf8_find_next_char(start, NULL);
    }
    return matches;
}

