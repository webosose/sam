// Copyright (c) 2017-2019 LG Electronics, Inc.
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

#include <bus/service/ApplicationManager.h>
#include <lifecycle/RunningInfoManager.h>
#include <lifecycle/stage/Prelauncher.h>
#include <lifecycle/stage/PrelauncherStages.h>
#include <luna-service2/lunaservice.h>
#include <package/AppPackageManager.h>

#include <setting/Settings.h>
#include <util/LSUtils.h>

Prelauncher::Prelauncher()
{
}

Prelauncher::~Prelauncher()
{
    m_itemQueue.clear();
    m_lscallRequestList.clear();
}

void Prelauncher::addItem(LaunchAppItemPtr item)
{
    LaunchAppItemPtr newItem = std::static_pointer_cast<LaunchAppItem>(item);
    auto it = std::find_if(m_itemQueue.begin(), m_itemQueue.end(),
                           [&item](LaunchAppItemPtr it_item) {return (it_item->getUid() == item->getUid());});
    if (it != m_itemQueue.end()) {
        Logger::error(getClassName(), __FUNCTION__, item->getAppId(), "already_in_prelaunching_queue");
        return;
    }

    // add item into queue
    m_itemQueue.push_back(newItem);

    if (PrelauncherStage::setPrelaunchingStages(newItem) == false) {
        Logger::error(getClassName(), __FUNCTION__, item->getAppId(), "set_stages_fail", newItem->getErrorText());
        finishPrelaunching(newItem);
        return;
    }

    runStages(newItem);
}

void Prelauncher::removeItem(const std::string& app_uid)
{
    auto it = std::find_if(m_itemQueue.begin(), m_itemQueue.end(), [&app_uid](LaunchAppItemPtr it_item) {return (it_item->getUid() == app_uid);});
    if (it != m_itemQueue.end())
        m_itemQueue.erase(it);
}

LaunchAppItemPtr Prelauncher::getLSCallRequestItemByToken(const LSMessageToken& token)
{
    auto it = m_lscallRequestList.begin();
    auto it_end = m_lscallRequestList.end();
    for (; it != it_end; ++it) {
        if ((*it)->returnToken() == token)
            return *it;
    }
    return NULL;
}

LaunchAppItemPtr Prelauncher::getItemByUid(const std::string& uid)
{
    auto it = m_itemQueue.begin();
    auto it_end = m_itemQueue.end();
    for (; it != it_end; ++it) {
        if ((*it)->getUid() == uid)
            return *it;
    }
    return NULL;
}

void Prelauncher::removeItemFromLSCallRequestList(const std::string& uid)
{
    auto it = std::find_if(m_lscallRequestList.begin(), m_lscallRequestList.end(),
                           [&uid](LaunchAppItemPtr item) {return (item->getUid() == uid);});
    if (it == m_lscallRequestList.end())
        return;
    m_lscallRequestList.erase(it);
}

bool Prelauncher::onReturnLSCallForBridgedRequest(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull()) {
        Logger::error("Prelauncher", __FUNCTION__, "parsing_fail");
        return false;
    }

    LSMessageToken token = LSMessageGetResponseToken(message);
    LaunchAppItemPtr prelaunching_item = Prelauncher::getInstance().getLSCallRequestItemByToken(token);
    if (prelaunching_item == NULL) {
        Logger::error("Prelauncher", __FUNCTION__, "null_prelaunching_item");
        return true;
    }

    const std::string& uid = prelaunching_item->getUid();
    Prelauncher::getInstance().removeItemFromLSCallRequestList(uid);

    if (Prelauncher::getInstance().getItemByUid(uid) == NULL) {
        Logger::info("Prelauncher", __FUNCTION__, uid, "launching item is already removed");
        return true;
    }

    prelaunching_item->resetReturnToken();

    Logger::info("Prelauncher", __FUNCTION__, prelaunching_item->getAppId(), "received return for just bridge request");
    return true;
}

bool Prelauncher::onReturnLSCall(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));
    if (responsePayload.isNull()) {
        Logger::error("Prelauncher", __FUNCTION__, "parsing_fail");
        return false;
    }

    LSMessageToken token = LSMessageGetResponseToken(message);
    LaunchAppItemPtr prelaunching_item = Prelauncher::getInstance().getLSCallRequestItemByToken(token);
    if (prelaunching_item == NULL) {
        Logger::error("Prelauncher", __FUNCTION__, "null_prelaunching_item");
        return true;
    }

    std::string uid = prelaunching_item->getUid();
    Prelauncher::getInstance().removeItemFromLSCallRequestList(uid);

    if (Prelauncher::getInstance().getItemByUid(uid) == NULL) {
        Logger::info("Prelauncher", __FUNCTION__, uid, "launching item is already removed");
        return true;
    }

    prelaunching_item->resetReturnToken();
    Prelauncher::getInstance().handleStages(prelaunching_item);

    return true;
}

void Prelauncher::inputBridgedReturn(LaunchAppItemPtr item, const pbnjson::JValue& responsePayload)
{
    if (item == NULL) {
        Logger::error(getClassName(), __FUNCTION__, "null_pointer");
        return;
    }

    Logger::info(getClassName(), __FUNCTION__, item->getAppId(), "trigger_bridged_launching");
    LaunchAppItemPtr prelaunching_item = std::static_pointer_cast<LaunchAppItem>(item);

    prelaunching_item->resetReturnToken();
    handleStages(prelaunching_item);
}

void Prelauncher::runStages(LaunchAppItemPtr item)
{
    if (item == NULL) {
        Logger::error(getClassName(), __FUNCTION__, "null_prelaunching_item");
        return;
    }

    StageItemList& stage_list = item->stageList();
    if (stage_list.empty()) {
        Logger::error(getClassName(), __FUNCTION__, "run_stage: no stage to run, finishing prelaunch stages");
        finishPrelaunching(item);
        return;
    }

    // handle sync check type (DIRECT_CHECK)
    StageItem& stage_item = stage_list.front();
    while (StageHandlerType::DIRECT_CHECK == stage_item.m_handlerType) {
        item->setSubStage(static_cast<int>(stage_item.m_launchingStage));

        // call direct checker
        StageHandlerReturn handler_return = stage_item.m_handler(item);

        // only support GO_NEXT_STAGE, REDIRECTED, ERROR
        // TODO: implement if needed to handle other results
        if (StageHandlerReturn::REDIRECTED == handler_return) {
            redirectToAnother(item);
            return;
        } else if (StageHandlerReturn::ERROR == handler_return) {
            Logger::error(getClassName(), __FUNCTION__, "prelaunching", "handle_stage: handling fail");
            finishPrelaunching(item);
            return;
        }

        // remove completed stage (direct_check)
        stage_list.pop_front();

        // remove sub_call if not GO_DEPENDENT_STAGE
        if (StageHandlerReturn::GO_NEXT_STAGE == handler_return) {
            // remove sub stages if item passed previous stage
            while (!stage_list.empty()) {
                StageItem& next_stage_item = stage_list.front();
                if ((StageHandlerType::SUB_CALL != next_stage_item.m_handlerType) && (StageHandlerType::SUB_BRIDGE_CALL != next_stage_item.m_handlerType))
                    break;
                stage_list.pop_front();
            }
        }

        if (stage_list.empty()) {
            Logger::info(getClassName(), __FUNCTION__, item->getAppId(), "handled all stages");
            finishPrelaunching(item);
            return;
        }

        stage_item = stage_list.front();
    }

    item->setSubStage(static_cast<int>(stage_item.m_launchingStage));

    // handle async call type (MAIN_CALL, SUB_CALL, BRIDGE_CALL)
    pbnjson::JValue requestPayload = pbnjson::Object();
    if (!stage_item.m_payloadMaker(item, requestPayload)) {
        Logger::error(getClassName(), __FUNCTION__, "run_stage: failed to make payload");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        finishPrelaunching(item);
        return;
    }

    LSMessageToken token = 0;
    LSErrorSafe lserror;
    if (!LSCallOneReply(ApplicationManager::getInstance().get(),
                        stage_item.m_uri.c_str(),
                        requestPayload.stringify().c_str(),
                        (((stage_item.m_handlerType == StageHandlerType::BRIDGE_CALL) || (stage_item.m_handlerType == StageHandlerType::SUB_BRIDGE_CALL)) ? onReturnLSCallForBridgedRequest : onReturnLSCall), this, &token, &lserror)) {
        Logger::error(getClassName(), __FUNCTION__, lserror.message);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        finishPrelaunching(item);
        return;
    }

    item->setReturnToken(token);
    m_lscallRequestList.push_back(item);
}

void Prelauncher::handleStages(LaunchAppItemPtr prelaunching_item)
{
    if (prelaunching_item == NULL) {
        Logger::error(getClassName(), __FUNCTION__, "null_prelaunching_item");
        return;
    }

    StageItemList& stage_list = prelaunching_item->stageList();
    if (stage_list.empty()) {
        Logger::info(getClassName(), __FUNCTION__, prelaunching_item->getAppId(), "handled all stages");
        finishPrelaunching(prelaunching_item);
        return;
    }

    StageItem& stage_item = stage_list.front();

    // run stage
    StageHandlerReturn handler_return = stage_item.m_handler(prelaunching_item);

    // handle stage exceptional result (error, redirect)
    if (StageHandlerReturn::ERROR == handler_return) {
        Logger::info(getClassName(), __FUNCTION__, prelaunching_item->getAppId(), "handle_stage: handling fail");
        finishPrelaunching(prelaunching_item);
        return;
    } else if (StageHandlerReturn::REDIRECTED == handler_return) {
        redirectToAnother(prelaunching_item);
        return;
    }

    // remove completed stage
    stage_list.pop_front();

    // handle stage result (error, redirect)
    if (StageHandlerReturn::GO_NEXT_STAGE == handler_return) {
        // remove sub stages if item passed previous stage
        while (!stage_list.empty()) {
            StageItem& next_stage_item = stage_list.front();
            if (StageHandlerType::SUB_CALL != next_stage_item.m_handlerType &&
                StageHandlerType::SUB_BRIDGE_CALL != next_stage_item.m_handlerType)
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

void Prelauncher::finishPrelaunching(LaunchAppItemPtr prelaunching_item)
{
    std::string uid = prelaunching_item->getUid();

    prelaunching_item->setSubStage(static_cast<int>(AppLaunchingStage::PRELAUNCH_DONE));
    EventPrelaunchingDone(prelaunching_item->getUid());

    // clear prelaunching data
    removeItem(uid);
    removeItemFromLSCallRequestList(uid);
}

void Prelauncher::redirectToAnother(LaunchAppItemPtr item)
{
    Logger::info(getClassName(), __FUNCTION__, Logger::format("redirected: old(%s) new(%s)", item->requestedAppId().c_str(), item->getAppId().c_str()));
    item->clearAllStages();
    PrelauncherStage::setPrelaunchingStages(item);
    runStages(item);
}

void Prelauncher::cancelAll()
{
    for (auto& prelaunching_item : m_itemQueue) {
        Logger::info(getClassName(), __FUNCTION__, prelaunching_item->getAppId(), "cancel_launching");
        prelaunching_item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "cancel all request");
        EventPrelaunchingDone(prelaunching_item->getUid());
    }

    m_itemQueue.clear();
    m_lscallRequestList.clear();
}
