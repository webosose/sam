// Copyright (c) 2017-2018 LG Electronics, Inc.
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

#include "core/bus/launchpoint_luna_adapter.h"

#include <boost/bind.hpp>

#include "core/base/logging.h"
#include "core/bus/appmgr_service.h"
#include "core/bus/lunaservice_api.h"
#include "core/launch_point/launch_point_manager.h"
#include "core/module/locale_preferences.h"

#define LP_SUBSCRIPTION_KEY "listLaunchPoints"

LaunchPointLunaAdapter::LaunchPointLunaAdapter()
{
}

LaunchPointLunaAdapter::~LaunchPointLunaAdapter()
{
}

void LaunchPointLunaAdapter::LaunchPointLunaAdapter::init()
{
    initLunaApiHandler();

    LaunchPointManager::instance().signal_on_launch_point_ready_.connect(boost::bind(&LaunchPointLunaAdapter::onReady, this));

    LaunchPointManager::instance().SubscribeListChange(boost::bind(&LaunchPointLunaAdapter::onLaunchPointsListChanged, this, _1));

    LaunchPointManager::instance().SubscribeLaunchPointChange(boost::bind(&LaunchPointLunaAdapter::onLaunchPointChanged, this, _1, _2));
}

void LaunchPointLunaAdapter::LaunchPointLunaAdapter::initLunaApiHandler()
{
    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_ADD_LAUNCHPOINT, "applicationManager.addLaunchPoint", boost::bind(&LaunchPointLunaAdapter::requestController, this, _1));

    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_UPDATE_LAUNCHPOINT, "applicationManager.updateLaunchPoint",
            boost::bind(&LaunchPointLunaAdapter::requestController, this, _1));

    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_REMOVE_LAUNCHPOINT, "applicationManager.removeLaunchPoint",
            boost::bind(&LaunchPointLunaAdapter::requestController, this, _1));

    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_MOVE_LAUNCHPOINT, "applicationManager.moveLaunchPoint", boost::bind(&LaunchPointLunaAdapter::requestController, this, _1));

    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_LIST_LAUNCHPOINTS, "applicationManager.listLaunchPoints", boost::bind(&LaunchPointLunaAdapter::requestController, this, _1));

    AppMgrService::instance().registerApiHandler(API_CATEGORY_GENERAL, API_SEARCH_APPS, "applicationManager.searchApps", boost::bind(&LaunchPointLunaAdapter::requestController, this, _1));
}

void LaunchPointLunaAdapter::requestController(LunaTaskPtr task)
{
    if (!LaunchPointManager::instance().Ready()) {
        m_pendingTasks.push_back(task);
        LOG_INFO(MSGID_LAUNCH_POINT_REQUEST, 4, PMLOGKS("category", task->category().c_str()), PMLOGKS("method", task->method().c_str()), PMLOGKS("status", "pending"),
                PMLOGKS("caller", task->caller().c_str()), "received message, but will handle later");
        return;
    }

    handleRequest(task);
}

void LaunchPointLunaAdapter::onReady()
{
    auto it = m_pendingTasks.begin();
    while (it != m_pendingTasks.end()) {
        handleRequest(*it);
        it = m_pendingTasks.erase(it);
    }
}

void LaunchPointLunaAdapter::handleRequest(LunaTaskPtr task)
{
    if (API_CATEGORY_GENERAL == task->category()) {
        if (API_ADD_LAUNCHPOINT == task->method())
            addLaunchPoint(task);
        else if (API_UPDATE_LAUNCHPOINT == task->method())
            updateLaunchPoint(task);
        else if (API_REMOVE_LAUNCHPOINT == task->method())
            removeLaunchPoint(task);
        else if (API_MOVE_LAUNCHPOINT == task->method())
            moveLaunchPoint(task);
        else if (API_LIST_LAUNCHPOINTS == task->method())
            listLaunchPoints(task);
        else if (API_SEARCH_APPS == task->method())
            searchApps(task);
    }
}

void LaunchPointLunaAdapter::addLaunchPoint(LunaTaskPtr task)
{

    bool result = false;
    std::string app_id;
    std::string lpid;
    std::string err_text;
    LaunchPointPtr LP = NULL;

    LP = LaunchPointManager::instance().AddLaunchPoint(LPMAction::API_ADD, task->jmsg(), err_text);

    if (LP != NULL) {
        result = true;
        app_id = LP->Id();
        lpid = LP->LaunchPointId();
    }

    LOG_NORMAL(NLID_LAUNCH_POINT_ADDED, 4, PMLOGKS("caller", task->caller().c_str()), PMLOGKS("status", result?"done":"fail"), PMLOGKS("app_id", app_id.c_str()), PMLOGKS("lp_id", lpid.c_str()), "");

    if (!result) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, err_text);
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("launchPointId", lpid);

    task->ReplyResult(payload);
}

void LaunchPointLunaAdapter::updateLaunchPoint(LunaTaskPtr task)
{

    bool result = false;
    std::string app_id;
    std::string lpid;
    std::string err_text;
    LaunchPointPtr LP = NULL;

    LP = LaunchPointManager::instance().UpdateLaunchPoint(LPMAction::API_UPDATE, task->jmsg(), err_text);

    if (LP != NULL) {
        result = true;
        app_id = LP->Id();
        lpid = LP->LaunchPointId();
    }

    LOG_INFO(MSGID_LAUNCH_POINT_UPDATED, 4, PMLOGKS("caller", task->caller().c_str()), PMLOGKS("status", (result?"done":"failed")), PMLOGKS("app_id", app_id.c_str()), PMLOGKS("lp_id", lpid.c_str()),
            "");

    if (!result)
        task->setError(API_ERR_CODE_GENERAL, err_text);

    task->replyResult();
}

void LaunchPointLunaAdapter::removeLaunchPoint(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string lpid = jmsg["launchPointId"].asString();
    std::string err_text;

    LaunchPointManager::instance().RemoveLaunchPoint(task->jmsg(), err_text);

    LOG_INFO(MSGID_LAUNCH_POINT_REMOVED, 3, PMLOGKS("caller", task->caller().c_str()), PMLOGKS("status", (err_text.empty()?"done":"failed")), PMLOGKS("lp_id", lpid.c_str()), "");

    if (!err_text.empty())
        task->setError(API_ERR_CODE_GENERAL, err_text);

    task->replyResult();
}

void LaunchPointLunaAdapter::moveLaunchPoint(LunaTaskPtr task)
{

    bool result = false;
    std::string app_id;
    std::string lpid;
    std::string err_text;
    LaunchPointPtr LP = NULL;

    LP = LaunchPointManager::instance().MoveLaunchPoint(LPMAction::API_MOVE, task->jmsg(), err_text);

    if (LP != NULL) {
        result = true;
        app_id = LP->Id();
        lpid = LP->LaunchPointId();
    }

    LOG_INFO(MSGID_LAUNCH_POINT_MOVED, 4, PMLOGKS("caller", task->caller().c_str()), PMLOGKS("status", (result?"done":"failed")), PMLOGKS("app_id", app_id.c_str()), PMLOGKS("lp_id", lpid.c_str()),
            "");

    if (!err_text.empty())
        task->setError(API_ERR_CODE_GENERAL, err_text);

    task->replyResult();
}

void LaunchPointLunaAdapter::listLaunchPoints(LunaTaskPtr task)
{

    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue launch_points = pbnjson::Array();
    bool subscribed = false;

    LaunchPointManager::instance().LaunchPointsAsJson(launch_points);

    if (LSMessageIsSubscription(task->lsmsg()))
        subscribed = LSSubscriptionAdd(task->lshandle(), LP_SUBSCRIPTION_KEY, task->lsmsg(), NULL);

    payload.put("subscribed", subscribed);
    payload.put("launchPoints", launch_points);

    LOG_INFO(MSGID_LAUNCH_POINT_REQUEST, 2, PMLOGKS("STATUS", "done"), PMLOGKS("CALLER", task->caller().c_str()), "reply listLaunchPoint");

    task->ReplyResult(payload);
}

void LaunchPointLunaAdapter::searchApps(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string keyword = jmsg["keyword"].asString();
    if (keyword.length() < 2) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "keyword should be longer than 1");
        return;
    }

    pbnjson::JValue results = pbnjson::Array();
    LaunchPointManager::instance().SearchLaunchPoints(results, keyword);

    if (0 == results.arraySize()) {
        task->replyResultWithError(API_ERR_CODE_GENERAL, "There is no match for the keyword");
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("results", results);

    task->ReplyResult(payload);
}

///////////////////////////////////////////////////////////////////
// Publisher for subscribers
///////////////////////////////////////////////////////////////////

void LaunchPointLunaAdapter::onLaunchPointsListChanged(const pbnjson::JValue& launch_points)
{

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("subscribed", true);
    payload.put("returnValue", true);
    payload.put("launchPoints", launch_points);

    LOG_INFO(MSGID_LAUNCH_POINT_REPLY_SUBSCRIBER, 1, PMLOGKS("status", "reply_lp_list_to_subscribers"), "");

    LSErrorSafe lserror;
    if (!LSSubscriptionReply(AppMgrService::instance().serviceHandle(),
                             LP_SUBSCRIPTION_KEY,
                             payload.stringify().c_str(),
                             &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 2, PMLOGKS("type", "ls_subscription_reply"), PMLOGKS("where", __FUNCTION__), "err: %s", lserror.message);
    }
}

void LaunchPointLunaAdapter::onLaunchPointChanged(const std::string& change, const pbnjson::JValue& launch_point)
{

    pbnjson::JValue payload = launch_point.duplicate();
    payload.put("returnValue", true);
    payload.put("subscribed", true);

    LOG_INFO(MSGID_LAUNCH_POINT_REPLY_SUBSCRIBER, 3, PMLOGKS("status", "reply_lp_change_to_subscribers"), PMLOGKS("reason", change.c_str()),
            PMLOGKFV("position", "%d", launch_point.hasKey("position") ? launch_point["position"].asNumber<int>():-1), "");
    LSErrorSafe lserror;
    if (!LSSubscriptionReply(AppMgrService::instance().serviceHandle(),
                             LP_SUBSCRIPTION_KEY,
                             payload.stringify().c_str(),
                             &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 2, PMLOGKS("type", "ls_subscription_reply"), PMLOGKS("where", __FUNCTION__), "err: %s", lserror.message);
    }
}
