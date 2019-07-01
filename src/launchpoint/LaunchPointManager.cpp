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
#include <bus/service/ApplicationManager.h>
#include <launchpoint/LaunchPointManager.h>
#include <lifecycle/LifecycleManager.h>
#include <package/AppPackageManager.h>
#include <iterator>
#include <sys/time.h>
#include <bus/client/WAM.h>
#include <bus/client/DB8.h>
#include <util/Logging.h>

#include <util/LSUtils.h>

LaunchPointManager::LaunchPointManager()
    : m_lpmReady(false),
      m_dbConnected(false),
      m_dbLoaded(false),
      m_appsLoaded(false),
      m_orderedListReady(false),
      m_listAppsChanges(pbnjson::Array()),
      m_launchPointsDBData(pbnjson::Array()),
      m_DBHandler(),
      m_launchPointFactory(),
      m_orderingHandler()
{
}

LaunchPointManager::~LaunchPointManager()
{
}

void LaunchPointManager::initialize()
{
    m_DBHandler.EventDBLoaded_.connect(boost::bind(&LaunchPointManager::onLPLoaded, this, _1));
    m_DBHandler.init();

    m_orderingHandler.EventLaunchPointsOrdered.connect(boost::bind(&LaunchPointManager::onLPOrdered, this, _1));
    m_orderingHandler.init();

    DB8::getInstance().EventServiceStatusChanged.connect(boost::bind(&LaunchPointManager::onDBConnected, this, _1));

    AppInstallService::getInstance().EventStatusChanged.connect(boost::bind(&LaunchPointManager::onPackageStatusChanged, this, _1, _2));
    AppPackageManager::getInstance().EventListAppsChanged.connect(boost::bind(&LaunchPointManager::onListAppsChanged, this, _1, _2, _3));
    AppPackageManager::getInstance().EventOneAppChanged.connect(boost::bind(&LaunchPointManager::onOneAppChanged, this, _1, _2, _3, _4));
}

void LaunchPointManager::handleLpmState(const LPStateInput& input, const pbnjson::JValue& data)
{
    LOG_INFO(MSGID_LAUNCH_POINT, 6,
             PMLOGKS("status", "handle_lpm_state"),
             PMLOGKFV("state_input", "%d", (int)input),
             PMLOGKS("load_app", m_appsLoaded?"done":"not_ready_yet"),
             PMLOGKS("load_db", m_dbLoaded?"done":"not_ready_yet"),
             PMLOGKS("load_ordered_list", m_orderedListReady?"done":"not_ready_yet"),
             PMLOGKS("lpm_ready", m_lpmReady?"ready":"not_ready_yet"), "");

    if (LPStateInput::LOAD_APPS == input) {
        if (m_appsLoaded && m_dbLoaded) {
            updateApps(data);
            reloadDbData();
        } else if (!m_appsLoaded && m_dbLoaded) {
            updateApps(data);
            syncAppsWithDbData();
            sortLPs();
        } else {
            updateApps(data);
        }
    } else if (LPStateInput::LOAD_DB_DATA == input) {
        if (m_appsLoaded) {
            syncAppsWithDbData();
            sortLPs();
        }
    }

    if (m_appsLoaded && m_dbLoaded && m_orderedListReady) {
        LOG_INFO(MSGID_LAUNCH_POINT, 1,
                 PMLOGKS("status", "lpm_ready"),
                 "all conditions are ready");

        m_lpmReady = true;

        replyLpListToSubscribers();
        if (m_listener)
            m_listener->onReadyLaunchPointManager();
    } else {
        m_lpmReady = false;
    }
}

/***********************************************************/
/** handling DB (connect/load/update) **********************/
/***********************************************************/
void LaunchPointManager::onDBConnected(bool connection)
{
    m_dbConnected = connection;
    if (!m_dbConnected)
        return;

    m_DBHandler.handleDbState(m_dbConnected);
    m_orderingHandler.handleDBState(m_dbConnected);
}

void LaunchPointManager::onLPLoaded(const pbnjson::JValue& loaded_db_result)
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

    m_DBHandler.reloadDbData(m_dbConnected);
    m_orderingHandler.reloadDBData(m_dbConnected);
}

/***********************************************************/
/** handling Ordering (make/update) ************************/
/***********************************************************/
void LaunchPointManager::sortLPs()
{
    m_orderedListReady = false;

    std::vector<LaunchPointPtr> visible_list;
    for (auto it : m_launchPointList) {
        if (it->isVisible())
            visible_list.push_back(it);
    }

    m_orderingHandler.makeLaunchPointsInOrder(visible_list, m_listAppsChanges);
}

void LaunchPointManager::onLPOrdered(const OrderChangeState& change_state)
{
    LOG_INFO(MSGID_LAUNCH_POINT, 3,
             PMLOGKS("status", "received_changed_order_list"),
             PMLOGKFV("change_state", "%d", (int)change_state),
             PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");

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
        removeLPsByAppId(app_id);
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
             PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");

    handleLpmState(LPStateInput::LOAD_APPS, apps);
}

void LaunchPointManager::onOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev)
{

    if (dev)
        return;

    std::string app_id = app["id"].asString();
    std::string err_text;

    LOG_INFO(MSGID_LAUNCH_POINT, 4,
             PMLOGKS("status", "one_app_changed_on_list_apps"),
             PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
             PMLOGKS("change_reason", change.c_str()),
             PMLOGKS(LOG_KEY_FUNC, __FUNCTION__), "");

    if (LP_CHANGE_ADDED == change) {
        addLP(LPMAction::APP_INSTALLED, app, err_text);
    } else if (LP_CHANGE_UPDATED == change) {
        updateLP(LPMAction::APP_UPDATED, app, err_text);
    } else if (LP_CHANGE_REMOVED == change) {
        removeLPsByAppId(app_id);
    }
}

void LaunchPointManager::updateApps(const pbnjson::JValue& apps)
{

    std::string err_text;
    m_launchPointList.clear();

    for (int i = 0; i < apps.arraySize(); ++i) {
        std::string app_id = apps[i]["id"].asString();
        LaunchPointPtr lp = addLP(LPMAction::LOAD_APPS, apps[i], err_text);

        if (lp == nullptr)
            LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 3,
                        PMLOGKS("status", "lp_is_nullptr"),
                        PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
                        PMLOGKS(LOG_KEY_FUNC, __FUNCTION__),
                        "%s", err_text.c_str());
    }

    m_appsLoaded = true;
}

void LaunchPointManager::syncAppsWithDbData()
{

    std::string app_id, lp_id, err_text;
    LPType lp_type;

    int db_data_idx = m_launchPointsDBData.arraySize();

    LOG_INFO(MSGID_LAUNCH_POINT, 2,
             PMLOGKS("status", "sync_apps_with_db_data"),
             PMLOGKS(LOG_KEY_ACTION, "start_sync"), "");

    for (int idx = 0; idx < db_data_idx; ++idx) {
        pbnjson::JValue delete_json = pbnjson::Object();

        app_id = m_launchPointsDBData[idx]["id"].asString();
        lp_id = m_launchPointsDBData[idx]["launchPointId"].asString();
        lp_type = static_cast<LPType>(m_launchPointsDBData[idx]["lptype"].asNumber<int>());

        if (app_id.empty() || lp_type == LPType::UNKNOWN) {
            delete_json.put("launchPointId", lp_id);
            m_DBHandler.deleteData(delete_json);
            continue;
        }

        LaunchPointPtr default_lp = getDefaultLpByAppId(app_id);
        if (default_lp == nullptr) {
            delete_json.put("launchPointId", lp_id);
            m_DBHandler.deleteData(delete_json);
            continue;
        }

        if (LPType::DEFAULT == lp_type) {
            updateLP(LPMAction::DB_SYNC, m_launchPointsDBData[idx], err_text);
        } else if (LPType::BOOKMARK == lp_type) {
            addLP(LPMAction::DB_SYNC, m_launchPointsDBData[idx], err_text);
        }
    }

    LOG_INFO(MSGID_LAUNCH_POINT, 2,
             PMLOGKS("status", "sync_apps_with_db_data"),
             PMLOGKS(LOG_KEY_ACTION, "end_sync"), "");
}

/***********************************************************/
/** add launch point ****************************************/
/** Action Item: load_apps/db_sync/app_installed/api_add ***/
/***********************************************************/
LaunchPointPtr LaunchPointManager::addLP(const LPMAction action, const pbnjson::JValue& data, std::string& err_text)
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
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2,
                    PMLOGKS("status", "add_launch_point"),
                    PMLOGKS(LOG_KEY_ACTION, "unknown_request"), "");
        err_text = "unknown action";
        return nullptr;
    }

    if (lp_id.empty()) {
        err_text = "fail to generate launch point id";
        return nullptr;
    }

    LaunchPointPtr lp = m_launchPointFactory.createLaunchPoint(lp_type, lp_id, data, err_text);
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
        position = m_orderingHandler.insertLPInOrder(lp_id, data);
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

        position = m_orderingHandler.insertLPInOrder(lp_id, data);
        if (position == DEFAULT_POSITION_INVALID) {
            err_text = "fail to add in proper position";
            return nullptr;
        }

        lp_as_json = lp->toJValue().duplicate();
        lp_as_json.put("lptype", static_cast<int>(lp->getLPType()));

        if (m_DBHandler.insertData(lp_as_json))
            lp->setStoredInDb(true);

        lp->updateIfEmptyTitle(default_lp);
        replyLpChangeToSubscribers(lp, LP_CHANGE_ADDED, position);
        break;
    default:
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2,
                    PMLOGKS("status", "add_launch_point"),
                    PMLOGKS(LOG_KEY_ACTION, "unknown_request"), "");
        err_text = "unknown action";
        return nullptr;
    }

    // prevent too much log
    if ((LPMAction::LOAD_APPS != action) && (LPMAction::DB_SYNC != action)) {
        LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 5,
                 PMLOGKS("status", "add_launch_point"),
                 PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
                 PMLOGKFV("lp_type", "%d", (int)lp_type),
                 PMLOGKFV("lp_action", "%d", (int)action),
                 PMLOGKFV("position", "%d", position), "");
    }
    m_launchPointList.push_back(lp);

    return lp;
}

/***********************************************************/
/** update launch point *************************************/
/** Action Item: db_sync/app_updated/api_updated ***********/
/***********************************************************/
LaunchPointPtr LaunchPointManager::updateLP(const LPMAction action, const pbnjson::JValue& data, std::string& errorText)
{
    if (data.objectSize() < 2) {
        errorText = "insufficient parameters";
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
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2,
                    PMLOGKS("status", "update_launch_point"),
                    PMLOGKS(LOG_KEY_ACTION, "unknown_request"), "");
        errorText = "unknown action";
        return nullptr;
    }

    if (lp_id.empty()) {
        errorText = "launch point id is empty";
        return nullptr;
    }

    lp_by_lp_id = getLpByLpId(lp_id);
    if (lp_by_lp_id == nullptr) {
        errorText = "cannot find launch point";
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
            m_DBHandler.deleteData(delete_json);
        }

        updated_lp = m_launchPointFactory.createLaunchPoint(LPType::DEFAULT, lp_id, data, errorText);
        if (updated_lp == nullptr)
            return nullptr;

        m_launchPointList.remove_if([=](LaunchPointPtr p) {return p->getLunchPointId() == lp_id;});
        m_launchPointList.push_back(updated_lp);
        replyLpChangeToSubscribers(updated_lp, LP_CHANGE_UPDATED);

        break;
    case LPMAction::API_UPDATE:
        position = m_orderingHandler.updateLPInOrder(lp_id, data);

        if ((original_json == lp_by_lp_id->toJValue()) && (position == DEFAULT_POSITION_INVALID)) {
            LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2,
                        PMLOGKS("status", "update_launch_point"),
                        PMLOGKS(LOG_KEY_ACTION, "no_difference"), "");
            break;
        }

        lp_as_json = data.duplicate();
        lp_as_json.put("id", lp_by_lp_id->Id());
        lp_as_json.put("launchPointId", lp_by_lp_id->getLunchPointId());
        lp_as_json.put("lptype", static_cast<int>(lp_by_lp_id->getLPType()));

        if (lp_by_lp_id->isStoredInDb()) {
            m_DBHandler.updateData(lp_as_json);
        } else {
            if (m_DBHandler.insertData(lp_as_json))
                lp_by_lp_id->setStoredInDb(true);
        }
        replyLpChangeToSubscribers(lp_by_lp_id, LP_CHANGE_UPDATED, position);
        break;
    default:
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2,
                    PMLOGKS("status", "update_launch_point"),
                    PMLOGKS(LOG_KEY_ACTION, "unknown_request"), "");
        errorText = "unknown action";
        return nullptr;
    }

    // prevent too much log
    if (LPMAction::DB_SYNC != action)
        LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 5,
                 PMLOGKS("status", "update_launch_point"),
                 PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
                 PMLOGKFV("lp_type", "%d", (int)lp_by_lp_id->getLPType()),
                 PMLOGKFV("lp_action", "%d", (int)action),
                 PMLOGKFV("position", "%d", position), "");
    return lp_by_lp_id;
}

/***********************************************************/
/** move launch point ***************************************/
/** Action Item: api_move **********************************/
/***********************************************************/
LaunchPointPtr LaunchPointManager::moveLP(const LPMAction action, const pbnjson::JValue& data, std::string& errorText)
{

    if (data.objectSize() < 2) {
        errorText = "insufficient parameters";
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
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2,
                    PMLOGKS("status", "move_launch_point"),
                    PMLOGKS(LOG_KEY_ACTION, "unknown_request"), "");
        errorText = "unknown action";
        return nullptr;
    }

    if (lp_id.empty()) {
        errorText = "launch point id is empty";
        return nullptr;
    }

    lp_by_lp_id = getLpByLpId(lp_id);
    if (lp_by_lp_id == nullptr) {
        errorText = "cannot find launch point";
        return nullptr;
    }

    if (lp_by_lp_id->isUnmovable() || !(lp_by_lp_id->isVisible())) {
        errorText = "app is unmovable or invisible, cannot change the position of this app";
        return nullptr;
    }

    position = data["position"].asNumber<int>();

    if (position < 0) {
        errorText = "position number should be over 0";
        return nullptr;
    }

    LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 5,
             PMLOGKS("status", "move_launch_point"),
             PMLOGKS(LOG_KEY_APPID, lp_by_lp_id->Id().c_str()),
             PMLOGKFV("lp_type", "%d", (int)lp_by_lp_id->getLPType()),
             PMLOGKFV("lp_action", "%d", (int)action),
             PMLOGKFV("position", "%d", position), "");
    switch (action) {
    case LPMAction::API_MOVE:
        m_orderingHandler.updateLPInOrder(lp_id, data, position);
        replyLpChangeToSubscribers(lp_by_lp_id, LP_CHANGE_UPDATED, position);
        break;
    default:
        LOG_WARNING(MSGID_LAUNCH_POINT_WARNING, 2,
                    PMLOGKS("status", "move_launch_point"),
                    PMLOGKS(LOG_KEY_ACTION, "unknown_request"), "");
        errorText = "unknown action";
        return nullptr;
    }

    return lp_by_lp_id;
}

/***********************************************************/
/** remove launch point *************************************/
/***********************************************************/
void LaunchPointManager::removeLP(const pbnjson::JValue& data, std::string& errorText)
{
    std::string lpId = data["launchPointId"].asString();
    if (lpId.empty()) {
        errorText = "launch point id is empty";
        return;
    }

    LaunchPointPtr lp = getLpByLpId(lpId);
    if (lp == nullptr) {
        errorText = "cannot find launch point";
        return;
    }

    if (!lp->isRemovable()) {
        errorText = "this launch point cannot be removable";
        return;
    }

    LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 3,
             PMLOGKS("status", "remove_launch_point"),
             PMLOGKS(LOG_KEY_APPID, (lp->Id()).c_str()),
             PMLOGKFV("lp_type", "%d", static_cast<int>(lp->getLPType())), "");

    if (LPType::DEFAULT == lp->getLPType()) {
        AppPackageManager::getInstance().uninstallApp(lp->Id(), errorText);
        m_orderingHandler.deleteLPInOrder(lp->getLunchPointId());
    } else if (LPType::BOOKMARK == lp->getLPType()) {
        pbnjson::JValue delete_json = pbnjson::Object();
        delete_json.put("launchPointId", lp->getLunchPointId());

        replyLpChangeToSubscribers(lp, LP_CHANGE_REMOVED);
        m_DBHandler.deleteData(delete_json);
        m_orderingHandler.deleteLPInOrder(lp->getLunchPointId());

        if (isLastVisibleLp(lp)) {
            std::string error_text;
            LOG_INFO(MSGID_LAUNCH_POINT_ACTION, 3,
                     PMLOGKS("status", "remove_launch_point"),
                     PMLOGKS("last_visible_app", (lp->Id()).c_str()),
                     PMLOGKS("status", "closed"), "");
            LifecycleManager::getInstance().closeByAppId(lp->Id(), "", "", error_text, false, true);
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
    convertLPsToJson(launch_points);

    // notify changes
    EventLaunchPointListChanged(launch_points);

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

    EventLaunchPointChanged(change, json);
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

void LaunchPointManager::removeLPsByAppId(const std::string& id)
{
    for (auto it = m_launchPointList.begin(); it != m_launchPointList.end();) {
        if ((*it)->Id() == id) {
            pbnjson::JValue delete_json = pbnjson::Object();
            delete_json.put("launchPointId", (*it)->getLunchPointId());

            replyLpChangeToSubscribers(*it, LP_CHANGE_REMOVED);

            m_DBHandler.deleteData(delete_json);
            m_orderingHandler.deleteLPInOrder((*it)->getLunchPointId());

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

void LaunchPointManager::convertLPsToJson(pbnjson::JValue& array)
{
    std::vector<std::string> ordered_list = m_orderingHandler.getOrderedList();

    for (auto& lp_id : ordered_list) {
        auto it = find_if(m_launchPointList.begin(), m_launchPointList.end(), [&](LaunchPointPtr lp) {return lp->getLunchPointId() == lp_id;});

        if ((it != m_launchPointList.end()) && (*it)->isVisible())
            array.append((*it)->toJValue());
    }
}
