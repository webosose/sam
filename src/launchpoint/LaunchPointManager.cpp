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
#include <util/LSUtils.h>

LaunchPointManager::LaunchPointManager()
    : m_isLPMReady(false),
      m_isDBConnected(false),
      m_isDBLoaded(false),
      m_isAppsLoaded(false),
      m_isOrderedListReady(false),
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
    Logger::info(getClassName(), __FUNCTION__, "", Logger::format("input(%d) m_isAppsLoaded(%B) m_isDBLoaded(%B) m_isOrderedListReady(%B) m_isLPMReady(%B)",
                 (int)input, m_isAppsLoaded, m_isDBLoaded, m_isOrderedListReady, m_isLPMReady));

    if (LPStateInput::LOAD_APPS == input) {
        if (m_isAppsLoaded && m_isDBLoaded) {
            updateApps(data);
            reloadDbData();
        } else if (!m_isAppsLoaded && m_isDBLoaded) {
            updateApps(data);
            syncAppsWithDbData();
            sortLPs();
        } else {
            updateApps(data);
        }
    } else if (LPStateInput::LOAD_DB_DATA == input) {
        if (m_isAppsLoaded) {
            syncAppsWithDbData();
            sortLPs();
        }
    }

    if (m_isAppsLoaded && m_isDBLoaded && m_isOrderedListReady) {
        Logger::info(getClassName(), __FUNCTION__, "", "all conditions are ready");
        m_isLPMReady = true;

        replyLpListToSubscribers();
        EventReady();
    } else {
        m_isLPMReady = false;
    }
}

/***********************************************************/
/** handling DB (connect/load/update) **********************/
/***********************************************************/
void LaunchPointManager::onDBConnected(bool connection)
{
    m_isDBConnected = connection;
    if (!m_isDBConnected)
        return;

    m_DBHandler.handleDbState(m_isDBConnected);
    m_orderingHandler.handleDBState(m_isDBConnected);
}

void LaunchPointManager::onLPLoaded(const pbnjson::JValue& loaded_db_result)
{
    m_launchPointsDBData = loaded_db_result.duplicate();
    m_isDBLoaded = true;

    handleLpmState(LPStateInput::LOAD_DB_DATA, loaded_db_result);
}

void LaunchPointManager::reloadDbData()
{
    m_isDBLoaded = false;
    if (!m_isDBConnected)
        return;

    m_DBHandler.reloadDbData(m_isDBConnected);
    m_orderingHandler.reloadDBData(m_isDBConnected);
}

/***********************************************************/
/** handling Ordering (make/update) ************************/
/***********************************************************/
void LaunchPointManager::sortLPs()
{
    m_isOrderedListReady = false;

    std::vector<LaunchPointPtr> visible_list;
    for (auto it : m_launchPointList) {
        if (it->isVisible())
            visible_list.push_back(it);
    }

    m_orderingHandler.makeLaunchPointsInOrder(visible_list, m_listAppsChanges);
}

void LaunchPointManager::onLPOrdered(const OrderChangeState& change_state)
{
    Logger::info(getClassName(), __FUNCTION__, "", Logger::format("change_state(%d)", (int)change_state));
    if (change_state == OrderChangeState::FULL) {
        m_isOrderedListReady = true;
        handleLpmState(LPStateInput::LOAD_ORDERED_LIST, pbnjson::Object());
    }
}

/***********************************************************/
/** handling Applist (listApps/update/syncDB) **************/
/***********************************************************/
void LaunchPointManager::onPackageStatusChanged(const std::string& appId, const PackageStatus& status)
{
    if (PackageStatus::AboutToUninstall == status)
        removeLPsByAppId(appId);
}

void LaunchPointManager::onListAppsChanged(const pbnjson::JValue& apps, const std::vector<std::string>& changes, bool dev)
{
    if (dev)
        return;

    m_listAppsChanges = pbnjson::Array();
    for (const auto& c : changes) {
        m_listAppsChanges.append(c);
    }

    Logger::info(getClassName(), __FUNCTION__, "", m_listAppsChanges.stringify());
    handleLpmState(LPStateInput::LOAD_APPS, apps);
}

void LaunchPointManager::onOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev)
{

    if (dev)
        return;

    std::string appId = app["id"].asString();
    std::string errorText;

    Logger::info(getClassName(), __FUNCTION__, appId, "one_app_changed_on_list_apps", change);

    if (LP_CHANGE_ADDED == change) {
        addLP(LPMAction::APP_INSTALLED, app, errorText);
    } else if (LP_CHANGE_UPDATED == change) {
        updateLP(LPMAction::APP_UPDATED, app, errorText);
    } else if (LP_CHANGE_REMOVED == change) {
        removeLPsByAppId(appId);
    }
}

void LaunchPointManager::updateApps(const pbnjson::JValue& apps)
{
    std::string errorText;
    m_launchPointList.clear();

    for (int i = 0; i < apps.arraySize(); ++i) {
        std::string appId = apps[i]["id"].asString();
        LaunchPointPtr launchPointPtr = addLP(LPMAction::LOAD_APPS, apps[i], errorText);

        if (launchPointPtr == nullptr) {
            Logger::warning(getClassName(), __FUNCTION__, appId, "lp is nullptr", errorText);
        }
    }

    m_isAppsLoaded = true;
}

void LaunchPointManager::syncAppsWithDbData()
{
    std::string appId, launchPointId, errorText;
    LPType lp_type;

    int db_data_idx = m_launchPointsDBData.arraySize();

    Logger::info(getClassName(), __FUNCTION__, appId, "start sync_apps_with_db_data");
    for (int idx = 0; idx < db_data_idx; ++idx) {
        pbnjson::JValue delete_json = pbnjson::Object();

        appId = m_launchPointsDBData[idx]["id"].asString();
        launchPointId = m_launchPointsDBData[idx]["launchPointId"].asString();
        lp_type = static_cast<LPType>(m_launchPointsDBData[idx]["lptype"].asNumber<int>());

        if (appId.empty() || lp_type == LPType::UNKNOWN) {
            delete_json.put("launchPointId", launchPointId);
            m_DBHandler.deleteData(delete_json);
            continue;
        }

        LaunchPointPtr defaultLaunchPoint = getDefaultLpByAppId(appId);
        if (defaultLaunchPoint == nullptr) {
            delete_json.put("launchPointId", launchPointId);
            m_DBHandler.deleteData(delete_json);
            continue;
        }

        if (LPType::DEFAULT == lp_type) {
            updateLP(LPMAction::DB_SYNC, m_launchPointsDBData[idx], errorText);
        } else if (LPType::BOOKMARK == lp_type) {
            addLP(LPMAction::DB_SYNC, m_launchPointsDBData[idx], errorText);
        }
    }
    Logger::info(getClassName(), __FUNCTION__, appId, "end sync_apps_with_db_data");
}

/***********************************************************/
/** add launch point ****************************************/
/** Action Item: load_apps/db_sync/app_installed/api_add ***/
/***********************************************************/
LaunchPointPtr LaunchPointManager::addLP(const LPMAction action, const pbnjson::JValue& data, std::string& errorText)
{
    std::string appId, launchPointId;
    int position = DEFAULT_POSITION_INVALID;
    LPType lpType = LPType::UNKNOWN;
    pbnjson::JValue launchPointAsJson = pbnjson::Object();

    if (!data.hasKey("id") || data["id"].asString(appId) != CONV_OK) {
        errorText = "id is empty";
        return nullptr;
    }

    LaunchPointPtr defaultLaunchPoint = getDefaultLpByAppId(appId);
    if (defaultLaunchPoint == nullptr && (LPMAction::DB_SYNC == action || LPMAction::API_ADD == action)) {
        errorText = "cannot find default launch point";
        return nullptr;
    }

    switch (action) {
    case LPMAction::LOAD_APPS:
    case LPMAction::APP_INSTALLED:
        lpType = LPType::DEFAULT;
        launchPointId = appId + "_default";
        break;

    case LPMAction::DB_SYNC:
        lpType = LPType::BOOKMARK;
        launchPointId = data["launchPointId"].asString();
        break;

    case LPMAction::API_ADD:
        lpType = LPType::BOOKMARK;
        launchPointId = generateLpId();
        break;

    default:
        Logger::warning(getClassName(), __FUNCTION__, "", "unknown_request");
        errorText = "unknown action";
        return nullptr;
    }

    if (launchPointId.empty()) {
        errorText = "fail to generate launch point id";
        return nullptr;
    }

    LaunchPointPtr launchPointPtr = m_launchPointFactory.createLaunchPoint(lpType, launchPointId, data, errorText);
    if (launchPointPtr == nullptr)
        return nullptr;

    switch (action) {
    case LPMAction::LOAD_APPS:
        break;

    case LPMAction::DB_SYNC:
        launchPointPtr->setStoredInDb(true);
        launchPointPtr->updateIfEmptyTitle(defaultLaunchPoint);
        break;

    case LPMAction::APP_INSTALLED:
        position = m_orderingHandler.insertLPInOrder(launchPointId, data);
        if (position == DEFAULT_POSITION_INVALID) {
            errorText = "fail to add in proper position";
            return nullptr;
        }

        replyLpChangeToSubscribers(launchPointPtr, LP_CHANGE_ADDED, position);
        break;
    case LPMAction::API_ADD:
        launchPointPtr->updateIfEmpty(defaultLaunchPoint);
        // If there's no need to support i18n, fill in title and put it into DB.
        if (data.hasKey("supportI18nTitle") && !data["supportI18nTitle"].asBool())
            launchPointPtr->updateIfEmptyTitle(defaultLaunchPoint);

        position = m_orderingHandler.insertLPInOrder(launchPointId, data);
        if (position == DEFAULT_POSITION_INVALID) {
            errorText = "fail to add in proper position";
            return nullptr;
        }

        launchPointAsJson = launchPointPtr->toJValue().duplicate();
        launchPointAsJson.put("lptype", static_cast<int>(launchPointPtr->getLPType()));

        if (m_DBHandler.insertData(launchPointAsJson))
            launchPointPtr->setStoredInDb(true);

        launchPointPtr->updateIfEmptyTitle(defaultLaunchPoint);
        replyLpChangeToSubscribers(launchPointPtr, LP_CHANGE_ADDED, position);
        break;

    default:
        Logger::warning(getClassName(), __FUNCTION__, "", "unknown_request");
        errorText = "unknown action";
        return nullptr;
    }

    // prevent too much log
    if ((LPMAction::LOAD_APPS != action) && (LPMAction::DB_SYNC != action)) {
        Logger::info(getClassName(), __FUNCTION__, appId, Logger::format("lp_type(%d) action(%d) position(%d)", lpType, action, position));
    }
    m_launchPointList.push_back(launchPointPtr);

    return launchPointPtr;
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

    std::string launchPointId, appId;
    LaunchPointPtr launchPointByAppId = nullptr;
    LaunchPointPtr launchPointByLaunchPointId = nullptr;
    LaunchPointPtr updatedLaunchPoint = nullptr;
    pbnjson::JValue launchPointAsJson = pbnjson::Object();
    pbnjson::JValue delete_json = pbnjson::Object();
    pbnjson::JValue original_json = pbnjson::Object();
    int position = DEFAULT_POSITION_INVALID;

    switch (action) {
    case LPMAction::API_UPDATE:
    case LPMAction::DB_SYNC:
        launchPointId = data["launchPointId"].asString();
        break;
    case LPMAction::APP_UPDATED:
        appId = data["id"].asString();
        launchPointByAppId = getDefaultLpByAppId(appId);

        if (launchPointByAppId != nullptr) {
            launchPointId = launchPointByAppId->getLunchPointId();
        }
        break;

    default:
        Logger::info(getClassName(), __FUNCTION__, "", "unknown_request");
        errorText = "unknown action";
        return nullptr;
    }

    if (launchPointId.empty()) {
        errorText = "launch point id is empty";
        return nullptr;
    }

    launchPointByLaunchPointId = getLpByLpId(launchPointId);
    if (launchPointByLaunchPointId == nullptr) {
        errorText = "cannot find launch point";
        return nullptr;
    }

    original_json = launchPointByLaunchPointId->toJValue();
    launchPointByLaunchPointId->update(data);

    switch (action) {
    case LPMAction::DB_SYNC:
        launchPointByLaunchPointId->setStoredInDb(true);
        break;
    case LPMAction::APP_UPDATED:
        if (launchPointByLaunchPointId->isStoredInDb()) {
            delete_json.put("launchPointId", launchPointByLaunchPointId->getLunchPointId());
            m_DBHandler.deleteData(delete_json);
        }

        updatedLaunchPoint = m_launchPointFactory.createLaunchPoint(LPType::DEFAULT, launchPointId, data, errorText);
        if (updatedLaunchPoint == nullptr)
            return nullptr;

        m_launchPointList.remove_if([=](LaunchPointPtr p) {return p->getLunchPointId() == launchPointId;});
        m_launchPointList.push_back(updatedLaunchPoint);
        replyLpChangeToSubscribers(updatedLaunchPoint, LP_CHANGE_UPDATED);

        break;
    case LPMAction::API_UPDATE:
        position = m_orderingHandler.updateLPInOrder(launchPointId, data);

        if ((original_json == launchPointByLaunchPointId->toJValue()) && (position == DEFAULT_POSITION_INVALID)) {
            Logger::warning(getClassName(), __FUNCTION__, "", "no_difference");
            break;
        }

        launchPointAsJson = data.duplicate();
        launchPointAsJson.put("id", launchPointByLaunchPointId->Id());
        launchPointAsJson.put("launchPointId", launchPointByLaunchPointId->getLunchPointId());
        launchPointAsJson.put("lptype", static_cast<int>(launchPointByLaunchPointId->getLPType()));

        if (launchPointByLaunchPointId->isStoredInDb()) {
            m_DBHandler.updateData(launchPointAsJson);
        } else {
            if (m_DBHandler.insertData(launchPointAsJson))
                launchPointByLaunchPointId->setStoredInDb(true);
        }
        replyLpChangeToSubscribers(launchPointByLaunchPointId, LP_CHANGE_UPDATED, position);
        break;

    default:
        errorText = "unknown action";
        Logger::warning(getClassName(), __FUNCTION__, "", errorText);
        return nullptr;
    }

    // prevent too much log
    if (LPMAction::DB_SYNC != action)
        Logger::info(getClassName(), __FUNCTION__, appId, Logger::format("LPType(%d) action(%d) position(%d)", launchPointByLaunchPointId->getLPType(), action, position));
    return launchPointByLaunchPointId;
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

    std::string launchPointId, appId;
    LaunchPointPtr launchPointByLaunchPointId = nullptr;
    int position = DEFAULT_POSITION_INVALID;

    switch (action) {
    case LPMAction::API_MOVE:
        launchPointId = data["launchPointId"].asString();
        break;

    default:
        errorText = "unknown action";
        Logger::warning(getClassName(), __FUNCTION__, "", errorText);
        return nullptr;
    }

    if (launchPointId.empty()) {
        errorText = "launch point id is empty";
        return nullptr;
    }

    launchPointByLaunchPointId = getLpByLpId(launchPointId);
    if (launchPointByLaunchPointId == nullptr) {
        errorText = "cannot find launch point";
        return nullptr;
    }

    if (launchPointByLaunchPointId->isUnmovable() || !(launchPointByLaunchPointId->isVisible())) {
        errorText = "app is unmovable or invisible, cannot change the position of this app";
        return nullptr;
    }

    position = data["position"].asNumber<int>();

    if (position < 0) {
        errorText = "position number should be over 0";
        return nullptr;
    }

    Logger::info(getClassName(), __FUNCTION__, launchPointByLaunchPointId->Id(), Logger::format("LPType(%d) action(%d) position(%d)", launchPointByLaunchPointId->getLPType(), action, position));
    switch (action) {
    case LPMAction::API_MOVE:
        m_orderingHandler.updateLPInOrder(launchPointId, data, position);
        replyLpChangeToSubscribers(launchPointByLaunchPointId, LP_CHANGE_UPDATED, position);
        break;

    default:
        errorText = "unknown action";
        Logger::warning(getClassName(), __FUNCTION__, "", errorText);
        return nullptr;
    }

    return launchPointByLaunchPointId;
}

/***********************************************************/
/** remove launch point *************************************/
/***********************************************************/
void LaunchPointManager::removeLP(const pbnjson::JValue& data, std::string& errorText)
{
    std::string launchPointId = data["launchPointId"].asString();
    if (launchPointId.empty()) {
        errorText = "launch point id is empty";
        return;
    }

    LaunchPointPtr launchPointPtr = getLpByLpId(launchPointId);
    if (launchPointPtr == nullptr) {
        errorText = "cannot find launch point";
        return;
    }

    if (!launchPointPtr->isRemovable()) {
        errorText = "this launch point cannot be removable";
        return;
    }

    Logger::info(getClassName(), __FUNCTION__, launchPointPtr->Id(), Logger::format("LPType(%d)", launchPointPtr->getLPType()));
    if (LPType::DEFAULT == launchPointPtr->getLPType()) {
        AppPackageManager::getInstance().uninstallApp(launchPointPtr->Id(), errorText);
        m_orderingHandler.deleteLPInOrder(launchPointPtr->getLunchPointId());
    } else if (LPType::BOOKMARK == launchPointPtr->getLPType()) {
        pbnjson::JValue delete_json = pbnjson::Object();
        delete_json.put("launchPointId", launchPointPtr->getLunchPointId());

        replyLpChangeToSubscribers(launchPointPtr, LP_CHANGE_REMOVED);
        m_DBHandler.deleteData(delete_json);
        m_orderingHandler.deleteLPInOrder(launchPointPtr->getLunchPointId());

        if (isLastVisibleLp(launchPointPtr)) {
            std::string errorText;
            Logger::info(getClassName(), __FUNCTION__, launchPointPtr->Id(), "last visible app is closed");
            LifecycleManager::getInstance().closeByAppId(launchPointPtr->Id(), "", "", errorText, false, true);
        }

        m_launchPointList.remove_if([=](LaunchPointPtr p) {return p->getLunchPointId() == launchPointPtr->getLunchPointId();});
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

void LaunchPointManager::replyLpChangeToSubscribers(LaunchPointPtr launchPointPtr, const std::string& change, int position)
{
    if ((LPType::DEFAULT == launchPointPtr->getLPType()) && !launchPointPtr->isVisible())
        return;

    // payload
    pbnjson::JValue json = launchPointPtr->toJValue();
    json.put("change", change);
    if (DEFAULT_POSITION_INVALID != position && (LP_CHANGE_ADDED == change || LP_CHANGE_UPDATED == change)) {
        json.put("position", position);
    }

    EventLaunchPointChanged(change, json);
}

/***********************************************************/
/** common util function ***********************************/
/***********************************************************/
bool LaunchPointManager::isLastVisibleLp(LaunchPointPtr launchPointPTr)
{

    for (auto it : m_launchPointList) {
        if ((it->Id() == launchPointPTr->Id()) && (it->getLunchPointId() != launchPointPTr->getLunchPointId()) && (it->isVisible()))
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

        std::string new_launchPointId = boost::lexical_cast<std::string>(verifier);
        if (getLpByLpId(new_launchPointId) == nullptr)
            return new_launchPointId;
    }

    return std::string("");
}

LaunchPointPtr LaunchPointManager::getDefaultLpByAppId(const std::string& appId)
{

    LaunchPointPtr defaultLaunchPointPtr = nullptr;

    for (auto launchPoint : m_launchPointList) {
        if ((appId == launchPoint->Id()) && (LPType::DEFAULT == launchPoint->getLPType())) {
            defaultLaunchPointPtr = launchPoint;
            break;
        }
    }

    return defaultLaunchPointPtr;
}

LaunchPointPtr LaunchPointManager::getLpByLpId(const std::string& launchPointId)
{
    LaunchPointPtr LaunchPointPtrByLaunchPointId = nullptr;

    for (auto lp : m_launchPointList) {
        if (launchPointId == lp->getLunchPointId()) {
            LaunchPointPtrByLaunchPointId = lp;
            break;
        }
    }

    return LaunchPointPtrByLaunchPointId;
}

void LaunchPointManager::convertLPsToJson(pbnjson::JValue& array)
{
    std::vector<std::string> ordered_list = m_orderingHandler.getOrderedList();

    for (auto& launchPointId : ordered_list) {
        auto it = find_if(m_launchPointList.begin(), m_launchPointList.end(), [&](LaunchPointPtr lp) {return lp->getLunchPointId() == launchPointId;});

        if ((it != m_launchPointList.end()) && (*it)->isVisible())
            array.append((*it)->toJValue());
    }
}
