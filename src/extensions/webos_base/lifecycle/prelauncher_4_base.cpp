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

#include "extensions/webos_base/lifecycle/prelauncher_4_base.h"

#include <luna-service2/lunaservice.h>

#include "core/lifecycle/app_info_manager.h"
#include "core/package/application_manager.h"
#include "core/base/jutil.h"
#include "core/base/lsutils.h"
#include "core/bus/appmgr_service.h"
#include "core/setting/settings.h"
#include "extensions/webos_base/base_logs.h"
#include "extensions/webos_base/lifecycle/prelauncher_stages_4_base.h"

static Prelauncher4Base* g_this = NULL;

Prelauncher4Base::Prelauncher4Base()
{
    g_this = this;
}

Prelauncher4Base::~Prelauncher4Base()
{
    item_queue_.clear();
    lscall_request_list_.clear();
}

void Prelauncher4Base::add_item(AppLaunchingItemPtr item)
{
    AppLaunchingItem4BasePtr new_item = std::static_pointer_cast<AppLaunchingItem4Base>(item);
    auto it = std::find_if(item_queue_.begin(), item_queue_.end(), [&item](AppLaunchingItemPtr it_item) {return (it_item->uid() == item->uid());});
    if (it != item_queue_.end()) {
        LOG_CRITICAL(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("app_id", item->app_id().c_str()), PMLOGKS("uid", item->uid().c_str()), PMLOGKS("reason", "already_in_prelaunching_queue"), "");
        return;
    }

    // add item into queue
    item_queue_.push_back(new_item);

    if (set_prelaunching_stages_4_base(new_item) == false) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("app_id", item->app_id().c_str()), PMLOGKS("reason", "set_stages_fail"), "err: %s", new_item->err_text().c_str());
        finish_prelaunching(new_item);
        return;
    }

    run_stages(new_item);
}

void Prelauncher4Base::remove_item(const std::string& app_uid)
{
    auto it = std::find_if(item_queue_.begin(), item_queue_.end(), [&app_uid](AppLaunchingItemPtr it_item) {return (it_item->uid() == app_uid);});
    if (it != item_queue_.end())
        item_queue_.erase(it);
}

AppLaunchingItem4BasePtr Prelauncher4Base::get_lscall_request_item_by_token(const LSMessageToken& token)
{
    auto it = lscall_request_list_.begin();
    auto it_end = lscall_request_list_.end();
    for (; it != it_end; ++it) {
        if ((*it)->return_token() == token)
            return *it;
    }
    return NULL;
}

AppLaunchingItem4BasePtr Prelauncher4Base::get_item_by_uid(const std::string& uid)
{
    auto it = item_queue_.begin();
    auto it_end = item_queue_.end();
    for (; it != it_end; ++it) {
        if ((*it)->uid() == uid)
            return *it;
    }
    return NULL;
}

void Prelauncher4Base::remove_item_from_lscall_request_list(const std::string& uid)
{
    auto it = std::find_if(lscall_request_list_.begin(), lscall_request_list_.end(), [&uid](AppLaunchingItemPtr item) {return (item->uid() == uid);});
    if (it == lscall_request_list_.end())
        return;
    lscall_request_list_.erase(it);
}

bool Prelauncher4Base::cb_return_lscall_for_bridged_request(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("reason", "parsing_fail"), PMLOGKS("where", "bridged_prelaunching_cb_return"), "");
        return false;
    }

    LSMessageToken token = LSMessageGetResponseToken(lsmsg);
    AppLaunchingItem4BasePtr prelaunching_item = g_this->get_lscall_request_item_by_token(token);
    if (prelaunching_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKFV("token", "%d", (int) token), PMLOGKS("reason", "null_prelaunching_item"), PMLOGKS("where", "bridged_prelaunching_cb_return"), "");
        return true;
    }

    const std::string& uid = prelaunching_item->uid();
    g_this->remove_item_from_lscall_request_list(uid);

    if (g_this->get_item_by_uid(uid) == NULL) {
        LOG_INFO(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("uid", uid.c_str()), PMLOGKS("reason", "not_valid_prelaunching_item"), PMLOGKS("where", "bridged_prelaunching_cb_return"),
                "launching item is already removed");
        return true;
    }

    prelaunching_item->reset_return_token();

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS("app_id", prelaunching_item->app_id().c_str()),
             PMLOGJSON("payload", jmsg.stringify().c_str()),
             "received return for just bridge request");

    return true;
}

bool Prelauncher4Base::cb_return_lscall(LSHandle* handle, LSMessage* lsmsg, void* user_data)
{
    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("reason", "parsing_fail"), PMLOGKS("where", "prelaunching_cb_return"), "");
        return false;
    }

    LSMessageToken token = LSMessageGetResponseToken(lsmsg);
    AppLaunchingItem4BasePtr prelaunching_item = g_this->get_lscall_request_item_by_token(token);
    if (prelaunching_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3, PMLOGKFV("token", "%d", (int) token), PMLOGKS("reason", "null_prelaunching_item"), PMLOGKS("where", "prelaunching_cb_return"), "");
        return true;
    }

    std::string uid = prelaunching_item->uid();
    g_this->remove_item_from_lscall_request_list(uid);

    if (g_this->get_item_by_uid(uid) == NULL) {
        LOG_INFO(MSGID_APPLAUNCH_ERR, 3, PMLOGKS("uid", uid.c_str()), PMLOGKS("reason", "not_valid_prelaunching_item"), PMLOGKS("where", "prelaunching_cb_return"),
                "launching item is already removed");
        return true;
    }

    prelaunching_item->set_call_return_jmsg(jmsg);
    prelaunching_item->reset_return_token();

    g_this->handle_stages(prelaunching_item);

    return true;
}

void Prelauncher4Base::input_bridged_return(AppLaunchingItemPtr item, const pbnjson::JValue& jmsg)
{
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("reason", "null_pointer"), PMLOGKS("where", "input_bridged_return"), "");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", item->app_id().c_str()),
             PMLOGJSON("jmsg", jmsg.duplicate().stringify().c_str()),
             PMLOGKS("action", "trigger_bridged_launching"), "");

    AppLaunchingItem4BasePtr prelaunching_item = std::static_pointer_cast<AppLaunchingItem4Base>(item);
    prelaunching_item->set_call_return_jmsg(jmsg);
    prelaunching_item->reset_return_token();
    handle_stages(prelaunching_item);
}

void Prelauncher4Base::run_stages(AppLaunchingItem4BasePtr prelaunching_item)
{
    if (prelaunching_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("reason", "null_prelaunching_item"), PMLOGKS("where", "run_prelaunching_stages"), "");
        return;
    }

    StageItem4BaseList& stage_list = prelaunching_item->stage_list();
    if (stage_list.empty()) {
        LOG_INFO(MSGID_APPLAUNCH, 1, PMLOGKS("stage", "prelaunching"), "run_stage: no stage to run, finishing prelaunch stages");
        finish_prelaunching(prelaunching_item);
        return;
    }

    // handle sync check type (DIRECT_CHECK)
    StageItem4Base& stage_item = stage_list.front();
    while (StageHandlerType4Base::DIRECT_CHECK == stage_item.handler_type) {
        prelaunching_item->set_sub_stage(static_cast<int>(stage_item.launching_stage));

        // call direct checker
        StageHandlerReturn4Base handler_return = stage_item.handler(prelaunching_item);

        // only support GO_NEXT_STAGE, REDIRECTED, ERROR
        // TODO: implement if needed to handle other results
        if (StageHandlerReturn4Base::REDIRECTED == handler_return) {
            redirect_to_another(prelaunching_item);
            return;
        } else if (StageHandlerReturn4Base::ERROR == handler_return) {
            LOG_ERROR(MSGID_APPLAUNCH_ERR, 1, PMLOGKS("stage", "prelaunching"), "handle_stage: handling fail");
            finish_prelaunching(prelaunching_item);
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
            LOG_INFO(MSGID_APPLAUNCH, 1, PMLOGKS("app_id", prelaunching_item->app_id().c_str()), "handled all stages");
            finish_prelaunching(prelaunching_item);
            return;
        }

        stage_item = stage_list.front();
    }

    prelaunching_item->set_sub_stage(static_cast<int>(stage_item.launching_stage));

    // handle async call type (MAIN_CALL, SUB_CALL, BRIDGE_CALL)
    pbnjson::JValue payload = pbnjson::Object();
    if (!stage_item.payload_maker(prelaunching_item, payload)) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 1, PMLOGKS("stage", "prelaunching"), "run_stage: failed to make payload");
        prelaunching_item->set_err_code_text(APP_LAUNCH_ERR_GENERAL, "internal error");
        finish_prelaunching(prelaunching_item);
        return;
    }

    LSMessageToken token = 0;
    LSErrorSafe lserror;
    if (!LSCallOneReply(AppMgrService::instance().ServiceHandle(),
                        stage_item.uri.c_str(),
                        payload.stringify().c_str(),
                        (((stage_item.handler_type == StageHandlerType4Base::BRIDGE_CALL) || (stage_item.handler_type == StageHandlerType4Base::SUB_BRIDGE_CALL)) ? cb_return_lscall_for_bridged_request : cb_return_lscall), this, &token, &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "lscallonereply"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", stage_item.uri.c_str()),
                  "err: %s", lserror.message);
        prelaunching_item->set_err_code_text(APP_LAUNCH_ERR_GENERAL, "internal error");
        finish_prelaunching(prelaunching_item);
        return;
    }

    prelaunching_item->set_return_token(token);
    lscall_request_list_.push_back(prelaunching_item);
}

void Prelauncher4Base::handle_stages(AppLaunchingItem4BasePtr prelaunching_item)
{
    if (prelaunching_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("reason", "null_prelaunching_item"), PMLOGKS("where", "handle_prelaunching_stages"), "");
        return;
    }

    StageItem4BaseList& stage_list = prelaunching_item->stage_list();
    if (stage_list.empty()) {
        LOG_INFO(MSGID_APPLAUNCH, 1, PMLOGKS("app_id", prelaunching_item->app_id().c_str()), "handled all stages");
        finish_prelaunching(prelaunching_item);
        return;
    }

    StageItem4Base& stage_item = stage_list.front();

    // run stage
    StageHandlerReturn4Base handler_return = stage_item.handler(prelaunching_item);

    // handle stage exceptional result (error, redirect)
    if (StageHandlerReturn4Base::ERROR == handler_return) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 1, PMLOGKS("stage", "prelaunching"), "handle_stage: handling fail");
        finish_prelaunching(prelaunching_item);
        return;
    } else if (StageHandlerReturn4Base::REDIRECTED == handler_return) {
        redirect_to_another(prelaunching_item);
        return;
    }

    // remove completed stage
    stage_list.pop_front();

    // handle stage result (error, redirect)
    if (StageHandlerReturn4Base::GO_NEXT_STAGE == handler_return) {
        // remove sub stages if item passed previous stage
        while (!stage_list.empty()) {
            StageItem4Base& next_stage_item = stage_list.front();
            if ((StageHandlerType4Base::SUB_CALL != next_stage_item.handler_type) && (StageHandlerType4Base::SUB_BRIDGE_CALL != next_stage_item.handler_type))
                break;
            stage_list.pop_front();
        }
    }

    // finish prelaunching if complete all stages
    if (stage_list.empty()) {
        finish_prelaunching(prelaunching_item);
    }
    // otherwise, run next stage
    else {
        run_stages(prelaunching_item);
    }
}

void Prelauncher4Base::finish_prelaunching(AppLaunchingItem4BasePtr prelaunching_item)
{
    std::string uid = prelaunching_item->uid();

    prelaunching_item->set_sub_stage(static_cast<int>(AppLaunchingStage4Base::PRELAUNCH_DONE));
    signal_prelaunching_done(prelaunching_item->uid());

    // clear prelaunching data
    remove_item(uid);
    remove_item_from_lscall_request_list(uid);
}

void Prelauncher4Base::redirect_to_another(AppLaunchingItem4BasePtr prelaunching_item)
{
    LOG_INFO(MSGID_APPLAUNCH, 3, PMLOGKS("action", "redirected"), PMLOGKS("new_app_id", prelaunching_item->app_id().c_str()), PMLOGKS("old_app_id", prelaunching_item->requested_app_id().c_str()),
            "%s->%s", prelaunching_item->requested_app_id().c_str(), prelaunching_item->app_id().c_str());

    prelaunching_item->clear_all_stages();
    set_prelaunching_stages_4_base(prelaunching_item);
    run_stages(prelaunching_item);
}

void Prelauncher4Base::cancel_all()
{
    for (auto& prelaunching_item : item_queue_) {
        LOG_INFO(MSGID_APPLAUNCH, 2, PMLOGKS("app_id", prelaunching_item->app_id().c_str()), PMLOGKS("status", "cancel_launching"), "");
        prelaunching_item->set_err_code_text(APP_LAUNCH_ERR_GENERAL, "cancel all request");
        signal_prelaunching_done(prelaunching_item->uid());
    }

    item_queue_.clear();
    lscall_request_list_.clear();
}
