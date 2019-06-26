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

#include <lifecycle/stage/appitem/AppItemFactory.h>
#include <lifecycle/stage/appitem/AppItemFactory.h>
#include <lifecycle/stage/appitem/LaunchAppItem.h>
#include <package/PackageManager.h>
#include <package/AppDescription.h>
#include <setting/Settings.h>
#include <util/BaseLogs.h>
#include <util/JUtil.h>
#include <util/LSUtils.h>

LaunchAppItemPtr AppItemFactory::createLaunchItem(LifeCycleTaskPtr task)
{
    if (task->getAppId().empty()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "empty_app_id"),
                  PMLOGKS("where", "tv_launching_item_factory"), "");
        return nullptr;
    }

    AppDescPtr appDesc = PackageManager::instance().getAppById(task->getAppId());
    if (appDesc == nullptr) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "not_exist"),
                  PMLOGKS("where", "AppLaunchingItemFactory4Basic"), "");
        return nullptr;
    }

    // parse caller info
    std::string caller = getCallerFromMessage(task->getLunaTask()->lsmsg());
    std::string callerId = getCallerID(caller);
    std::string callerPid = getCallerPID(caller);

    const pbnjson::JValue& params = task->getLunaTask()->jmsg();
    pbnjson::JValue params4app = (params.hasKey("params") && params["params"].isObject()) ? params["params"].duplicate() : pbnjson::Object();

    // this is for WAM, SAM will bypass to WAM. WAM doens't unload the app even if user clicks "X" button to close it.
    bool keepAlive = params.hasKey("keepAlive") && params["keepAlive"].asBool();
    if (SettingsImpl::instance().isKeepAliveApp(task->getAppId())) {
        keepAlive = true;
    }

    // caller can request not show splash image on launching app (default action is showing splash image)
    bool noSplash = !(params.hasKey("noSplash") && params["noSplash"].asBool());
    // need to check if it's running. set to false if so.
    if (!appDesc->getSplashOnLaunch()) {
        noSplash = false;
    }

    // caller can request not show spinner on launching app (default action is showing spinner)
    bool spinner = true;
    if (params.hasKey("spinner")) {
        spinner = params["spinner"].asBool();
    }

    // need to check if it's running. set to false if so.
    if (!appDesc->getSpinnerOnLaunch()) {
        spinner = false;
    }

    LaunchAppItemPtr item = std::make_shared<LaunchAppItem>(task->getAppId(), task->getDisplay(), params4app, task->getLunaTask()->lsmsg());
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "make_shared_error"),
                  PMLOGKS("where", "tv_launching_item_factory"), "");
        return NULL;
    }

    item->setCallerId(callerId);
    item->setCallerPid(callerPid);
    item->setKeepAlive(keepAlive);
    item->setShowSplash(noSplash);
    item->setShowSpinner(spinner);
    item->setSubStage(static_cast<int>(AppLaunchingStage::PREPARE_PRELAUNCH));

    LOG_INFO(MSGID_APPLAUNCH, 6,
             PMLOGKS("app_id", task->getAppId().c_str()),
             PMLOGKS("caller_id", item->getCallerId().c_str()),
             PMLOGKS("keep_alive", (keepAlive?"true":"false")),
             PMLOGKS("show_splash", (noSplash?"true":"false")),
             PMLOGKS("show_spinner", (spinner?"true":"false")),
             PMLOGJSON("params", params4app.stringify().c_str()), "");

    return item;
}

CloseAppItemPtr AppItemFactory::createCloseItem(LifeCycleTaskPtr task)
{
    // return std::make_shared<CloseAppItem>(appId, pid, callerId, closeReason);
    return nullptr;
}
