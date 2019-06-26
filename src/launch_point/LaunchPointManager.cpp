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

#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <bus/AppMgrService.h>
#include <launch_point/LaunchPointManager.h>
#include <lifecycle/LifeCycleManager.h>
#include <module/ServiceObserver.h>
#include <package/PackageManager.h>
#include <iterator>
#include <sys/time.h>
#include <util/Logging.h>

#include <util/LSUtils.h>

LaunchPointManager::LaunchPointManager() :
        m_lpmReady(false), m_dbConnected(false), m_dbLoaded(false), m_appsLoaded(false), m_orderedListReady(false), m_listAppsChanges(pbnjson::Array()), m_launchPointsDBData(pbnjson::Array()), m_DBHandler(
                nullptr), m_lpFactory(nullptr), m_orderingHandler(nullptr)
{
}

LaunchPointManager::~LaunchPointManager()
{
}

void LaunchPointManager::initialize()
{

    ServiceObserver::instance().add(WEBOS_SERVICE_DB, boost::bind(&LaunchPointManager::handleDbConnected, &LaunchPointManager::instance(), _1));

    AppinstalldSubscriber::instance().subscribeInstallStatus(boost::bind(&LaunchPointManager::onPackageStatusChanged, this, _1, _2));

    PackageManager::instance().signalListAppsChanged.connect(boost::bind(&LaunchPointManager::onListAppsChanged, this, _1, _2, _3));

    PackageManager::instance().signalOneAppChanged.connect(boost::bind(&LaunchPointManager::onOneAppChanged, this, _1, _2, _3, _4));
}

/**********************************************************/
/**** Set Extendable Handler ******************************/
/**********************************************************/
void LaunchPointManager::setDbHandler(DBHandler& db_handler)
{

    m_DBHandler = &db_handler;

    if (m_DBHandler == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "db_handler"), PMLOGKS("where", "SetDbHandler"), "");
        return;
    }

    m_DBHandler->signal_db_loaded_.connect(boost::bind(&LaunchPointManager::onLaunchPointsDbLoaded, this, _1));
    m_DBHandler->init();
}

void LaunchPointManager::setOrderingHandler(OrderingHandler& ordering_handler)
{
    m_orderingHandler = &ordering_handler;

    if (m_orderingHandler == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "SetOrderingHandler"), "");
        return;
    }

    m_orderingHandler->signal_launch_points_ordered_.connect(boost::bind(&LaunchPointManager::onLaunchPointsOrdered, this, _1));
    m_orderingHandler->init();
}

void LaunchPointManager::setLaunchPointFactory(LaunchPointFactory& lp_factory)
{
    m_lpFactory = &lp_factory;
}

/***********************************************************/
/** handling LPM State (listApps, DB, Ordering)*************/
/***********************************************************/
void LaunchPointManager::handleLpmState(const LPStateInput& input, const pbnjson::JValue& data)
{

    LOG_INFO(MSGID_LAUNCH_POINT, 6, PMLOGKS("status", "handle_lpm_state"), PMLOGKFV("state_input", "%d", (int)input), PMLOGKS("load_app", m_appsLoaded?"done":"not_ready_yet"),
            PMLOGKS("load_db", m_dbLoaded?"done":"not_ready_yet"), PMLOGKS("load_ordered_list", m_orderedListReady?"done":"not_ready_yet"), PMLOGKS("lpm_ready", m_lpmReady?"ready":"not_ready_yet"),
            "");

    if (LPStateInput::LOAD_APPS == input) {
        if (m_appsLoaded && m_dbLoaded) {
            updateApps(data);
            reloadDbData();
        } else if (!m_appsLoaded && m_dbLoaded) {
            updateApps(data);
            syncAppsWithDbData();
            makeLaunchPointsInOrder();
        } else {
            updateApps(data);
        }
    } else if (LPStateInput::LOAD_DB_DATA == input) {
        if (m_appsLoaded) {
            syncAppsWithDbData();
            makeLaunchPointsInOrder();
        }
    }

    if (m_appsLoaded && m_dbLoaded && m_orderedListReady) {
        LOG_INFO(MSGID_LAUNCH_POINT, 1, PMLOGKS("status", "lpm_ready"), "all conditions are ready");

        m_lpmReady = true;

        replyLpListToSubscribers();
        signal_on_launch_point_ready_();
    } else {
        m_lpmReady = false;
    }
}

/***********************************************************/
/** handling DB (connect/load/update) **********************/
/***********************************************************/
void LaunchPointManager::handleDbConnected(bool connection)
{

    m_dbConnected = connection;
    if (!m_dbConnected)
        return;

    if (m_DBHandler != nullptr)
        m_DBHandler->handleDbState(m_dbConnected);

    if (m_orderingHandler != nullptr)
        m_orderingHandler->handleDBState(m_dbConnected);
}

void LaunchPointManager::onLaunchPointsDbLoaded(const pbnjson::JValue& loaded_db_result)
{

    m_launchPointsDBData = loaded_db_result.duplicate();
    m_dbLoaded = true;

    handleLpmState(LPStateInput::LOAD_DB_DATA, loaded_db_result);
}

void LaunchPointManager::reloadDbData()
{

    m_dbLoaded = false;
    if (!m_dbConnected)
        return;

    if (m_DBHandler != nullptr)
        m_DBHandler->reloadDbData(m_dbConnected);

    if (m_orderingHandler != nullptr)
        m_orderingHandler->reloadDBData(m_dbConnected);
}

/***********************************************************/
/** handling Ordering (make/update) ************************/
/***********************************************************/
void LaunchPointManager::makeLaunchPointsInOrder()
{

    m_orderedListReady = false;

    std::vector<LaunchPointPtr> visible_list;
    for (auto it : m_launchPointList) {
        if (it->isVisible())
            visible_list.push_back(it);
    }

    if (m_orderingHandler == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "MakeLaunchPointsInOrder"), "");
        return;
    }

    m_orderingHandler->makeLaunchPointsInOrder(visible_list, m_listAppsChanges);
}

void LaunchPointManager::onLaunchPointsOrdered(const OrderChangeState& change_state)
{

    LOG_INFO(MSGID_LAUNCH_POINT, 3, PMLOGKS("status", "received_changed_order_list"), PMLOGKFV("change_state", "%d", (int)change_state), PMLOGKS("where", "OnLaunchPointsOrdered"), "");

    if (change_state == OrderChangeState::FULL) {
        m_orderedListReady = true;
        handleLpmState(LPStateInput::LOAD_ORDERED_LIST, pbnjson::Object());
    }
}

/***********************************************************/
/** handling Applist (listApps/update/syncDB) **************/
/***********************************************************/
void LaunchPointManager::onPackageStatusChanged(const std::string& app_id, const PackageStatus& status)
{

    if (PackageStatus::AboutToUninstall == status)
        removeAllLaunchPointsByAppId(app_id);
}

void LaunchPointManager::onListAppsChanged(const pbnjson::JValue& apps, const std::vector<std::string>& changes, bool dev)
{

    if (dev)
        return;

    m_listAppsChanges = pbnjson::Array();
    for (const auto& c : changes) {
        m_listAppsChanges.append(c);
    }

    LOG_INFO(MSGID_LAUNCH_POINT, 3,
             PMLOGKS("status", "full_list_changed_on_list_apps"),
             PMLOGKS("change_reason", m_listAppsChanges.stringify().c_str()),
             PMLOGKS("where", "OnListAppsChanged"), "");

    handleLpmState(LPStateInput::LOAD_APPS, apps);
}

void LaunchPointManager::onOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev)
{

    if (dev)
        return;

    std::string app_id = app["id"].asString();
    std::string err_text;

    LOG_INFO(MSGID_LAUNCH_POINT, 4, PMLOGKS("status", "one_app_changed_on_list_apps"), PMLOGKS("app_id", app_id.c_str()), PMLOGKS("change_reason", change.c_str()), PMLOGKS("where", "OnOneAppChanged"),
            "");

    if (LP_CHANGE_ADDED == change) {
        addLaunchPoint(LPMAction::APP_INSTALLED, app, err_text);
    } else if (LP_CHANGE_UPDATED == change) {
        updateLaunchPoint(LPMAction::APP_UPDATED, app, err_text);
    } else if (LP_CHANGE_REMOVED == change) {
        removeAllLaunchPointsByAppId(app_id);
    }
}

void LaunchPointManager::updateApps(const pbnjson::JValue& apps)
{

    std::string err_text;
    m_launchPointList.clear();

    for (int i = 0; i < apps.arraySize(); ++i) {
        std::string app_id = apps[i]["id"].asString();
        LaunchPointPtr lp = addLaunchPoint(LPMAction::LOAD_APPS, apps[i], err_text);

        if (lp == nullptr)
            LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 3, PMLOGKS("status", "lp_is_nullptr"), PMLOGKS("app_id", app_id.c_str()), PMLOGKS("where", "update_apps"), "%s", err_text.c_str());
    }

    m_appsLoaded = true;
}

void LaunchPointManager::syncAppsWithDbData()
{

    std::string app_id, lp_id, err_text;
    LPType lp_type;

    int db_data_idx = m_launchPointsDBData.arraySize();

    LOG_INFO(MSGID_LAUNCH_POINT, 2, PMLOGKS("status", "sync_apps_with_db_data"), PMLOGKS("action", "start_sync"), "");
    if (m_DBHandler == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "db_handler"), PMLOGKS("where", "SyncAppsWithDbData"), "");
        return;
    }

    for (int idx = 0; idx < db_data_idx; ++idx) {
        pbnjson::JValue delete_json = pbnjson::Object();

        app_id = m_launchPointsDBData[idx]["id"].asString();
        lp_id = m_launchPointsDBData[idx]["launchPointId"].asString();
        lp_type = static_cast<LPType>(m_launchPointsDBData[idx]["lptype"].asNumber<int>());

        if (app_id.empty() || lp_type == LPType::UNKNOWN) {
            delete_json.put("launchPointId", lp_id);
            m_DBHandler->deleteData(delete_json);
            continue;
        }

        LaunchPointPtr default_lp = getDefaultLpByAppId(app_id);
        if (default_lp == nullptr) {
            delete_json.put("launchPointId", lp_id);
            m_DBHandler->deleteData(delete_json);
            continue;
        }

        if (LPType::DEFAULT == lp_type) {
            updateLaunchPoint(LPMAction::DB_SYNC, m_launchPointsDBData[idx], err_text);
        } else if (LPType::BOOKMARK == lp_type) {
            addLaunchPoint(LPMAction::DB_SYNC, m_launchPointsDBData[idx], err_text);
        }
    }

    LOG_INFO(MSGID_LAUNCH_POINT, 2, PMLOGKS("status", "sync_apps_with_db_data"), PMLOGKS("action", "end_sync"), "");
}

/***********************************************************/
/** add launch point ****************************************/
/** Action Item: load_apps/db_sync/app_installed/api_add ***/
/***********************************************************/
LaunchPointPtr LaunchPointManager::addLaunchPoint(const LPMAction action, const pbnjson::JValue& data, std::string& err_text)
{

    std::string app_id, lp_id;
    int position = DEFAULT_POSITION_INVALID;
    LPType lp_type = LPType::UNKNOWN;
    pbnjson::JValue lp_as_json = pbnjson::Object();

    if (!data.hasKey("id") || data["id"].asString(app_id) != CONV_OK) {
        err_text = "id is empty";
        return nullptr;
    }

    LaunchPointPtr default_lp = getDefaultLpByAppId(app_id);
    if (default_lp == nullptr && (LPMAction::DB_SYNC == action || LPMAction::API_ADD == action)) {
        err_text = "cannot find default launch point";
        return nullptr;
    }

    if (m_orderingHandler == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "AddLaunchPoint"), "");
        err_text = "internal error";
        return nullptr;
    }

    if (m_DBHandler == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "db_handler"), PMLOGKS("where", "AddLaunchPoint"), "");
        err_text = "internal error";
        return nullptr;
    }

    if (m_lpFactory == nullptr) {
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
        lp_id = generateLpId();
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

    LaunchPointPtr lp = m_lpFactory->createLaunchPoint(lp_type, lp_id, data, err_text);
    if (lp == nullptr)
        return nullptr;

    switch (action) {
    case LPMAction::LOAD_APPS:
        break;
    case LPMAction::DB_SYNC:
        lp->setStoredInDb(true);
        lp->updateIfEmptyTitle(default_lp);
        break;
    case LPMAction::APP_INSTALLED:
        position = m_orderingHandler->insertLPInOrder(lp_id, data);
        if (position == DEFAULT_POSITION_INVALID) {
            err_text = "fail to add in proper position";
            return nullptr;
        }

        replyLpChangeToSubscribers(lp, LP_CHANGE_ADDED, position);
        break;
    case LPMAction::API_ADD:
        lp->updateIfEmpty(default_lp);
        // If there's no need to support i18n, fill in title and put it into DB.
        if (data.hasKey("supportI18nTitle") && !data["supportI18nTitle"].asBool())
            lp->updateIfEmptyTitle(default_lp);

        position = m_orderingHandler->insertLPInOrder(lp_id, data);
        if (position == DEFAULT_POSITION_INVALID) {
            err_text = "fail to add in proper position";
            return nullptr;
        }

        lp_as_json = lp->toJValue().duplicate();
        lp_as_json.put("lptype", static_cast<int>(lp->getLPType()));

        if (m_DBHandler->insertData(lp_as_json))
            lp->setStoredInDb(true);

        lp->updateIfEmptyTitle(default_lp);
        replyLpChangeToSubscribers(lp, LP_CHANGE_ADDED, position);
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
    m_launchPointList.push_back(lp);

    return lp;
}

/***********************************************************/
/** update launch point *************************************/
/** Action Item: db_sync/app_updated/api_updated ***********/
/***********************************************************/
LaunchPointPtr LaunchPointManager::updateLaunchPoint(const LPMAction action, const pbnjson::JValue& data, std::string& err_text)
{

    if (data.objectSize() < 2) {
        err_text = "insufficient parameters";
        return nullptr;
    }

    if (m_orderingHandler == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "UpdateLaunchPoint"), "");
        err_text = "internal error";
        return nullptr;
    }

    if (m_DBHandler == nullptr) {
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
        lp_by_app_id = getDefaultLpByAppId(app_id);

        if (lp_by_app_id != nullptr) {
            lp_id = lp_by_app_id->getLunchPointId();
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

    lp_by_lp_id = getLpByLpId(lp_id);
    if (lp_by_lp_id == nullptr) {
        err_text = "cannot find launch point";
        return nullptr;
    }

    original_json = lp_by_lp_id->toJValue();
    lp_by_lp_id->update(data);

    switch (action) {
    case LPMAction::DB_SYNC:
        lp_by_lp_id->setStoredInDb(true);
        break;
    case LPMAction::APP_UPDATED:
        if (lp_by_lp_id->isStoredInDb()) {
            delete_json.put("launchPointId", lp_by_lp_id->getLunchPointId());
            m_DBHandler->deleteData(delete_json);
        }

        updated_lp = m_lpFactory->createLaunchPoint(LPType::DEFAULT, lp_id, data, err_text);
        if (updated_lp == nullptr)
            return nullptr;

        m_launchPointList.remove_if([=](LaunchPointPtr p) {return p->getLunchPointId() == lp_id;});
        m_launchPointList.push_back(updated_lp);
        replyLpChangeToSubscribers(updated_lp, LP_CHANGE_UPDATED);

        break;
    case LPMAction::API_UPDATE:
        position = m_orderingHandler->updateLPInOrder(lp_id, data);

        if ((original_json == lp_by_lp_id->toJValue()) && (position == DEFAULT_POSITION_INVALID)) {
            LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2, PMLOGKS("status", "update_launch_point"), PMLOGKS("action", "no_difference"), "");
            break;
        }

        lp_as_json = data.duplicate();
        lp_as_json.put("id", lp_by_lp_id->Id());
        lp_as_json.put("launchPointId", lp_by_lp_id->getLunchPointId());
        lp_as_json.put("lptype", static_cast<int>(lp_by_lp_id->getLPType()));

        if (lp_by_lp_id->isStoredInDb()) {
            m_DBHandler->updateData(lp_as_json);
        } else {
            if (m_DBHandler->insertData(lp_as_json))
                lp_by_lp_id->setStoredInDb(true);
        }
        replyLpChangeToSubscribers(lp_by_lp_id, LP_CHANGE_UPDATED, position);
        break;
    default:
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2, PMLOGKS("status", "update_launch_point"), PMLOGKS("action", "unknown_request"), "");
        err_text = "unknown action";
        return nullptr;
    }

    // prevent too much log
    if (LPMAction::DB_SYNC != action)
        LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 5,
                 PMLOGKS("status", "update_launch_point"),
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKFV("lp_type", "%d", (int)lp_by_lp_id->getLPType()),
                 PMLOGKFV("lp_action", "%d", (int)action), PMLOGKFV("position", "%d", position), "");
    return lp_by_lp_id;
}

/***********************************************************/
/** move launch point ***************************************/
/** Action Item: api_move **********************************/
/***********************************************************/
LaunchPointPtr LaunchPointManager::moveLaunchPoint(const LPMAction action, const pbnjson::JValue& data, std::string& err_text)
{

    if (data.objectSize() < 2) {
        err_text = "insufficient parameters";
        return nullptr;
    }

    if (m_orderingHandler == nullptr) {
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

    lp_by_lp_id = getLpByLpId(lp_id);
    if (lp_by_lp_id == nullptr) {
        err_text = "cannot find launch point";
        return nullptr;
    }

    if (lp_by_lp_id->isUnmovable() || !(lp_by_lp_id->isVisible())) {
        err_text = "app is unmovable or invisible, cannot change the position of this app";
        return nullptr;
    }

    position = data["position"].asNumber<int>();

    if (position < 0) {
        err_text = "position number should be over 0";
        return nullptr;
    }

    LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 5,
             PMLOGKS("status", "move_launch_point"),
             PMLOGKS("app_id", lp_by_lp_id->Id().c_str()),
             PMLOGKFV("lp_type", "%d", (int)lp_by_lp_id->getLPType()),
             PMLOGKFV("lp_action", "%d", (int)action),
             PMLOGKFV("position", "%d", position), "");
    switch (action) {
    case LPMAction::API_MOVE:
        m_orderingHandler->updateLPInOrder(lp_id, data, position);
        replyLpChangeToSubscribers(lp_by_lp_id, LP_CHANGE_UPDATED, position);
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
void LaunchPointManager::removeLaunchPoint(const pbnjson::JValue& data, std::string& err_text)
{

    std::string lp_id = data["launchPointId"].asString();
    if (lp_id.empty()) {
        err_text = "launch point id is empty";
        return;
    }

    if (m_orderingHandler == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "RemoveLaunchPoint"), "");
        err_text = "internal error";
        return;
    }

    if (m_DBHandler == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "db_handler"), PMLOGKS("where", "RemoveLaunchPoint"), "");
        err_text = "internal error";
        return;
    }

    LaunchPointPtr lp = getLpByLpId(lp_id);
    if (lp == nullptr) {
        err_text = "cannot find launch point";
        return;
    }

    if (!lp->isRemovable()) {
        err_text = "this launch point cannot be removable";
        return;
    }

    LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 3,
             PMLOGKS("status", "remove_launch_point"),
             PMLOGKS("app_id", (lp->Id()).c_str()),
             PMLOGKFV("lp_type", "%d", static_cast<int>(lp->getLPType())), "");

    if (LPType::DEFAULT == lp->getLPType()) {
        PackageManager::instance().uninstallApp(lp->Id(), err_text);
        m_orderingHandler->deleteLPInOrder(lp->getLunchPointId());
    } else if (LPType::BOOKMARK == lp->getLPType()) {
        pbnjson::JValue delete_json = pbnjson::Object();
        delete_json.put("launchPointId", lp->getLunchPointId());

        replyLpChangeToSubscribers(lp, LP_CHANGE_REMOVED);
        m_DBHandler->deleteData(delete_json);
        m_orderingHandler->deleteLPInOrder(lp->getLunchPointId());

        if (isLastVisibleLp(lp)) {
            std::string error_text;
            LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 3, PMLOGKS("status", "remove_launch_point"), PMLOGKS("last_visible_app", (lp->Id()).c_str()), PMLOGKS("status", "closed"), "");
            LifecycleManager::instance().closeByAppId(lp->Id(), "", "", error_text, false, true);
        }

        m_launchPointList.remove_if([=](LaunchPointPtr p) {return p->getLunchPointId() == lp->getLunchPointId();});
    }
}

/***********************************************************/
/** subscription reply *************************************/
/** Action Item: listLaunchPoint (full/partial) ************/
/***********************************************************/
void LaunchPointManager::replyLpListToSubscribers()
{

    // payload
    pbnjson::JValue launch_points = pbnjson::Array();
    launchPointsAsJson(launch_points);

    // notify changes
    notify_launch_point_list_changed(launch_points);

    // reset change reason
    m_listAppsChanges = pbnjson::Array();
}

void LaunchPointManager::replyLpChangeToSubscribers(LaunchPointPtr lp, const std::string& change, int position)
{

    if ((LPType::DEFAULT == lp->getLPType()) && !lp->isVisible())
        return;

    // payload
    pbnjson::JValue json = lp->toJValue();
    json.put("change", change);
    if (DEFAULT_POSITION_INVALID != position && (LP_CHANGE_ADDED == change || LP_CHANGE_UPDATED == change)) {
        json.put("position", position);
    }

    notify_launch_point_changed(change, json);
}

/***********************************************************/
/** common util function ***********************************/
/***********************************************************/
bool LaunchPointManager::isLastVisibleLp(LaunchPointPtr lp)
{

    for (auto it : m_launchPointList) {
        if ((it->Id() == lp->Id()) && (it->getLunchPointId() != lp->getLunchPointId()) && (it->isVisible()))
            return false;
    }
    return true;
}

void LaunchPointManager::removeAllLaunchPointsByAppId(const std::string& id)
{

    if (m_orderingHandler == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "RemoveAllLaunchPointsByAppId"), "");
        return;
    }

    if (m_DBHandler == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "db_handler"), PMLOGKS("where", "RemoveAllLaunchPointsByAppId"), "");
        return;
    }

    for (auto it = m_launchPointList.begin(); it != m_launchPointList.end();) {
        if ((*it)->Id() == id) {
            pbnjson::JValue delete_json = pbnjson::Object();
            delete_json.put("launchPointId", (*it)->getLunchPointId());

            replyLpChangeToSubscribers(*it, LP_CHANGE_REMOVED);

            m_DBHandler->deleteData(delete_json);
            m_orderingHandler->deleteLPInOrder((*it)->getLunchPointId());

            it = m_launchPointList.erase(it);
        } else
            ++it;
    }
}

std::string LaunchPointManager::generateLpId()
{

    while (true) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        double verifier = tv.tv_usec;

        std::string new_lp_id = boost::lexical_cast<std::string>(verifier);
        if (getLpByLpId(new_lp_id) == nullptr)
            return new_lp_id;
    }

    return std::string("");
}

LaunchPointPtr LaunchPointManager::getDefaultLpByAppId(const std::string& app_id)
{

    LaunchPointPtr default_lp = nullptr;

    for (auto lp : m_launchPointList) {
        if ((app_id == lp->Id()) && (LPType::DEFAULT == lp->getLPType())) {
            default_lp = lp;
            break;
        }
    }

    return default_lp;
}

LaunchPointPtr LaunchPointManager::getLpByLpId(const std::string& lp_id)
{
    LaunchPointPtr lp_by_lp_id = nullptr;

    for (auto lp : m_launchPointList) {
        if (lp_id == lp->getLunchPointId()) {
            lp_by_lp_id = lp;
            break;
        }
    }

    return lp_by_lp_id;
}

void LaunchPointManager::launchPointsAsJson(pbnjson::JValue& array)
{
    if (m_orderingHandler == nullptr) {
        LOG_ERROR(MSGID_LAUNCH_POINT_ERROR, 3, PMLOGKS("error", "handler_is_null"), PMLOGKS("which", "ordering_handler"), PMLOGKS("where", "LaunchPointsAsJson"), "");
        return;
    }

    std::vector<std::string> ordered_list = m_orderingHandler->getOrderedList();

    for (auto& lp_id : ordered_list) {
        auto it = find_if(m_launchPointList.begin(), m_launchPointList.end(), [&](LaunchPointPtr lp) {return lp->getLunchPointId() == lp_id;});

        if ((it != m_launchPointList.end()) && (*it)->isVisible())
            array.append((*it)->toJValue());
    }
}

void LaunchPointManager::searchLaunchPoints(pbnjson::JValue& matchedByTitle, const std::string& searchTerm)
{

    if (searchTerm.empty() || !matchedByTitle.isArray())
        return;

    gchar* keyword = g_utf8_strdown(searchTerm.c_str(), -1);

    for (auto lp : m_launchPointList) {
        if (!lp->isVisible())
            continue;

        gchar* title = g_utf8_strdown(lp->getTitle().c_str(), -1);

        if (matchesTitle(keyword, title)) {
            pbnjson::JValue obj = pbnjson::Object();
            obj.put("launchPointId", lp->getLunchPointId());
            obj.put("appId", lp->Id());
            obj.put("title", lp->getTitle());
            obj.put("icon", lp->getIcon());
            matchedByTitle.append(obj);
        }

        if (title)
            g_free(title);
    }

    if (keyword)
        g_free(keyword);
}

bool LaunchPointManager::matchesTitle(const gchar* keyword, const gchar* title) const
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

