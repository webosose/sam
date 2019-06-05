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

#include <bus/appmgr_service.h>
#include <lifecycle/app_info_manager.h>
#include <lifecycle/prelauncher_4_base.h>
#include <lifecycle/prelauncher_stages_4_base.h>
#include <luna-service2/lunaservice.h>

#include <package/PackageManager.h>
#include <setting/settings.h>
#include <util/base_logs.h>
#include <util/jutil.h>
#include <util/lsutils.h>

static Prelauncher4Base* g_this = NULL;

Prelauncher4Base::Prelauncher4Base()
{
    g_this = this;
}

Prelauncher4Base::~Prelauncher4Base()
{
    m_itemQueue.clear();
    m_lscallRequestList.clear();
}

void Prelauncher4Base::addItem(AppLaunchingItemPtr item)
{
    AppLaunchingItem4BasePtr newItem = std::static_pointer_cast<AppLaunchingItem4Base>(item);
    auto it = std::find_if(m_itemQueue.begin(), m_itemQueue.end(),
                           [&item](AppLaunchingItemPtr it_item) {return (it_item->uid() == item->uid());});
    if (it != m_itemQueue.end()) {
        LOG_CRITICAL(MSGID_APPLAUNCH_ERR, 3,
                     PMLOGKS("app_id", item->appId().c_str()),
                     PMLOGKS("uid", item->uid().c_str()),
                     PMLOGKS("reason", "already_in_prelaunching_queue"), "");
        return;
    }

    // add item into queue
    m_itemQueue.push_back(newItem);

    if (set_prelaunching_stages_4_base(newItem) == false) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("app_id", item->appId().c_str()),
                  PMLOGKS("reason", "set_stages_fail"),
                  "errText: %s", newItem->errText().c_str());
        finishPrelaunching(newItem);
        return;
    }

    runStages(newItem);
}

void Prelauncher4Base::removeItem(const std::string& app_uid)
{
    auto it = std::find_if(m_itemQueue.begin(), m_itemQueue.end(), [&app_uid](AppLaunchingItemPtr it_item) {return (it_item->uid() == app_uid);});
    if (it != m_itemQueue.end())
        m_itemQueue.erase(it);
}

AppLaunchingItem4BasePtr Prelauncher4Base::getLSCallRequestItemByToken(const LSMessageToken& token)
{
    auto it = m_lscallRequestList.begin();
    auto it_end = m_lscallRequestList.end();
    for (; it != it_end; ++it) {
        if ((*it)->returnToken() == token)
            return *it;
    }
    return NULL;
}

AppLaunchingItem4BasePtr Prelauncher4Base::getItemByUid(const std::string& uid)
{
    auto it = m_itemQueue.begin();
    auto it_end = m_itemQueue.end();
    for (; it != it_end; ++it) {
        if ((*it)->uid() == uid)
            return *it;
    }
    return NULL;
}

void Prelauncher4Base::removeItemFromLSCallRequestList(const std::string& uid)
{
    auto it = std::find_if(m_lscallRequestList.begin(), m_lscallRequestList.end(),
                           [&uid](AppLaunchingItemPtr item) {return (item->uid() == uid);});
    if (it == m_lscallRequestList.end())
        return;
    m_lscallRequestList.erase(it);
}

bool Prelauncher4Base::cbReturnLSCallForBridgedRequest(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "parsing_fail"),
                  PMLOGKS("where", "bridged_prelaunching_cb_return"), "");
        return false;
    }

    LSMessageToken token = LSMessageGetResponseToken(lsmsg);
    AppLaunchingItem4BasePtr prelaunching_item = g_this->getLSCallRequestItemByToken(token);
    if (prelaunching_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKFV("token", "%d", (int) token),
                  PMLOGKS("reason", "null_prelaunching_item"),
                  PMLOGKS("where", "bridged_prelaunching_cb_return"), "");
        return true;
    }

    const std::string& uid = prelaunching_item->uid();
    g_this->removeItemFromLSCallRequestList(uid);

    if (g_this->getItemByUid(uid) == NULL) {
        LOG_INFO(MSGID_APPLAUNCH_ERR, 3,
                 PMLOGKS("uid", uid.c_str()),
                 PMLOGKS("reason", "not_valid_prelaunching_item"),
                 PMLOGKS("where", "bridged_prelaunching_cb_return"),
                "launching item is already removed");
        return true;
    }

    prelaunching_item->resetReturnToken();

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS("app_id", prelaunching_item->appId().c_str()),
             PMLOGJSON("payload", jmsg.stringify().c_str()),
             "received return for just bridge request");

    return true;
}

bool Prelauncher4Base::cbReturnLSCall(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "parsing_fail"),
                  PMLOGKS("where", "prelaunching_cb_return"), "");
        return false;
    }

    LSMessageToken token = LSMessageGetResponseToken(lsmsg);
    AppLaunchingItem4BasePtr prelaunching_item = g_this->getLSCallRequestItemByToken(token);
    if (prelaunching_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKFV("token", "%d", (int) token),
                  PMLOGKS("reason", "null_prelaunching_item"),
                  PMLOGKS("where", "prelaunching_cb_return"), "");
        return true;
    }

    std::string uid = prelaunching_item->uid();
    g_this->removeItemFromLSCallRequestList(uid);

    if (g_this->getItemByUid(uid) == NULL) {
        LOG_INFO(MSGID_APPLAUNCH_ERR, 3,
                 PMLOGKS("uid", uid.c_str()),
                 PMLOGKS("reason", "not_valid_prelaunching_item"),
                 PMLOGKS("where", "prelaunching_cb_return"),
                "launching item is already removed");
        return true;
    }

    prelaunching_item->setCallReturnJmsg(jmsg);
    prelaunching_item->resetReturnToken();

    g_this->handleStages(prelaunching_item);

    return true;
}

void Prelauncher4Base::inputBridgedReturn(AppLaunchingItemPtr item, const pbnjson::JValue& jmsg)
{
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "null_pointer"),
                  PMLOGKS("where", "input_bridged_return"), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", item->appId().c_str()),
             PMLOGJSON("jmsg", jmsg.duplicate().stringify().c_str()),
             PMLOGKS("action", "trigger_bridged_launching"), "");

    AppLaunchingItem4BasePtr prelaunching_item = std::static_pointer_cast<AppLaunchingItem4Base>(item);
    prelaunching_item->setCallReturnJmsg(jmsg);
    prelaunching_item->resetReturnToken();
    handleStages(prelaunching_item);
}

void Prelauncher4Base::runStages(AppLaunchingItem4BasePtr item)
{
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("reason", "null_prelaunching_item"), PMLOGKS("where", "run_prelaunching_stages"), "");
        return;
    }

    StageItem4BaseList& stage_list = item->stage_list();
    if (stage_list.empty()) {
        LOG_INFO(MSGID_APPLAUNCH, 1, PMLOGKS("stage", "prelaunching"), "run_stage: no stage to run, finishing prelaunch stages");
        finishPrelaunching(item);
        return;
    }

    // handle sync check type (DIRECT_CHECK)
    StageItem4Base& stage_item = stage_list.front();
    while (StageHandlerType4Base::DIRECT_CHECK == stage_item.handler_type) {
        item->setSubStage(static_cast<int>(stage_item.launching_stage));

        // call direct checker
        StageHandlerReturn4Base handler_return = stage_item.handler(item);

        // only support GO_NEXT_STAGE, REDIRECTED, ERROR
        // TODO: implement if needed to handle other results
        if (StageHandlerReturn4Base::REDIRECTED == handler_return) {
            redirectToAnother(item);
            return;
        } else if (StageHandlerReturn4Base::ERROR == handler_return) {
            LOG_ERROR(MSGID_APPLAUNCH_ERR, 1, PMLOGKS("stage", "prelaunching"), "handle_stage: handling fail");
            finishPrelaunching(item);
            return;
        }

        // remove completed stage (direct_check)
        stage_list.pop_front();

        // remove sub_call if not GO_DEPENDENT_STAGE
        if (StageHandlerReturn4Base::GO_NEXT_STAGE == handler_return) {
            // remove sub stages if item passed previous stage
            while (!stage_list.empty()) {
                StageItem4Base& next_stage_item = stage_list.front();
                if ((StageHandlerType4Base::SUB_CALL != next_stage_item.handler_type) && (StageHandlerType4Base::SUB_BRIDGE_CALL != next_stage_item.handler_type))
                    break;
                stage_list.pop_front();
            }
        }

        if (stage_list.empty()) {
            LOG_INFO(MSGID_APPLAUNCH, 1, PMLOGKS("app_id", item->appId().c_str()), "handled all stages");
            finishPrelaunching(item);
            return;
        }

        stage_item = stage_list.front();
    }

    item->setSubStage(static_cast<int>(stage_item.launching_stage));

    // handle async call type (MAIN_CALL, SUB_CALL, BRIDGE_CALL)
    pbnjson::JValue payload = pbnjson::Object();
    if (!stage_item.payload_maker(item, payload)) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 1, PMLOGKS("stage", "prelaunching"), "run_stage: failed to make payload");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        finishPrelaunching(item);
        return;
    }

    LSMessageToken token = 0;
    LSErrorSafe lserror;
    if (!LSCallOneReply(AppMgrService::instance().serviceHandle(),
                        stage_item.uri.c_str(),
                        payload.stringify().c_str(),
                        (((stage_item.handler_type == StageHandlerType4Base::BRIDGE_CALL) || (stage_item.handler_type == StageHandlerType4Base::SUB_BRIDGE_CALL)) ? cbReturnLSCallForBridgedRequest : cbReturnLSCall), this, &token, &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "lscallonereply"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", stage_item.uri.c_str()),
                  "err: %s", lserror.message);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        finishPrelaunching(item);
        return;
    }

    item->setReturnToken(token);
    m_lscallRequestList.push_back(item);
}

void Prelauncher4Base::handleStages(AppLaunchingItem4BasePtr prelaunching_item)
{
    if (prelaunching_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "null_prelaunching_item"),
                  PMLOGKS("where", "handle_prelaunching_stages"), "");
        return;
    }

    StageItem4BaseList& stage_list = prelaunching_item->stage_list();
    if (stage_list.empty()) {
        LOG_INFO(MSGID_APPLAUNCH, 1, PMLOGKS("app_id", prelaunching_item->appId().c_str()), "handled all stages");
        finishPrelaunching(prelaunching_item);
        return;
    }

    StageItem4Base& stage_item = stage_list.front();

    // run stage
    StageHandlerReturn4Base handler_return = stage_item.handler(prelaunching_item);

    // handle stage exceptional result (error, redirect)
    if (StageHandlerReturn4Base::ERROR == handler_return) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 1, PMLOGKS("stage", "prelaunching"), "handle_stage: handling fail");
        finishPrelaunching(prelaunching_item);
        return;
    } else if (StageHandlerReturn4Base::REDIRECTED == handler_return) {
        redirectToAnother(prelaunching_item);
        return;
    }

    // remove completed stage
    stage_list.pop_front();

    // handle stage result (error, redirect)
    if (StageHandlerReturn4Base::GO_NEXT_STAGE == handler_return) {
        // remove sub stages if item passed previous stage
        while (!stage_list.empty()) {
            StageItem4Base& next_stage_item = stage_list.front();
            if (StageHandlerType4Base::SUB_CALL != next_stage_item.handler_type &&
                StageHandlerType4Base::SUB_BRIDGE_CALL != next_stage_item.handler_type)
                break;
            stage_list.pop_front();
        }
    }

    if (stage_list.empty()) {
        // finish prelaunching if complete all stages
        finishPrelaunching(prelaunching_item);
    } else {
        // otherwise, run next stage
        runStages(prelaunching_item);
    }
}

void Prelauncher4Base::finishPrelaunching(AppLaunchingItem4BasePtr prelaunching_item)
{
    std::string uid = prelaunching_item->uid();

    prelaunching_item->setSubStage(static_cast<int>(AppLaunchingStage4Base::PRELAUNCH_DONE));
    signal_prelaunching_done(prelaunching_item->uid());

    // clear prelaunching data
    removeItem(uid);
    removeItemFromLSCallRequestList(uid);
}

void Prelauncher4Base::redirectToAnother(AppLaunchingItem4BasePtr prelaunching_item)
{
    LOG_INFO(MSGID_APPLAUNCH, 3, PMLOGKS("action", "redirected"), PMLOGKS("new_app_id", prelaunching_item->appId().c_str()), PMLOGKS("old_app_id", prelaunching_item->requestedAppId().c_str()),
            "%s->%s", prelaunching_item->requestedAppId().c_str(), prelaunching_item->appId().c_str());

    prelaunching_item->clear_all_stages();
    set_prelaunching_stages_4_base(prelaunching_item);
    runStages(prelaunching_item);
}

void Prelauncher4Base::cancelAll()
{
    for (auto& prelaunching_item : m_itemQueue) {
        LOG_INFO(MSGID_APPLAUNCH, 2, PMLOGKS("app_id", prelaunching_item->appId().c_str()), PMLOGKS("status", "cancel_launching"), "");
        prelaunching_item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "cancel all request");
        signal_prelaunching_done(prelaunching_item->uid());
    }

    m_itemQueue.clear();
    m_lscallRequestList.clear();
}
