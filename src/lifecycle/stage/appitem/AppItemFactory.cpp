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

#include <lifecycle/stage/appitem/AppItemFactory.h>
#include <lifecycle/stage/appitem/AppItemFactory.h>
#include <lifecycle/stage/appitem/LaunchAppItem.h>
#include <package/AppPackage.h>
#include <package/AppPackageManager.h>
#include <setting/Settings.h>
#include <util/LSUtils.h>

LaunchAppItemPtr AppItemFactory::createLaunchItem(LifecycleTaskPtr task)
{
    if (task->getAppId().empty()) {
        Logger::error("AppItemFactory", __FUNCTION__, "empty_appId");
        return nullptr;
    }

    AppPackagePtr appDesc = AppPackageManager::getInstance().getAppById(task->getAppId());
    if (appDesc == nullptr) {
        Logger::error("AppItemFactory", __FUNCTION__, "not exist");
        return nullptr;
    }

    // parse caller info
    std::string caller = task->getLunaTask()->caller();
    std::string callerId = getCallerID(caller);
    std::string callerPid = getCallerPID(caller);

    const pbnjson::JValue& params = task->getLunaTask()->getRequestPayload();
    pbnjson::JValue params4app = (params.hasKey("params") && params["params"].isObject()) ? params["params"].duplicate() : pbnjson::Object();

    // this is for WAM, SAM will bypass to WAM. WAM doens't unload the app even if user clicks "X" button to close it.
    bool keepAlive = params.hasKey("keepAlive") && params["keepAlive"].asBool();
    if (SettingsImpl::getInstance().isKeepAliveApp(task->getAppId())) {
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

    LaunchAppItemPtr item = std::make_shared<LaunchAppItem>(task->getAppId(), task->getDisplay(), params4app, task->getLunaTask()->getRequest());
    if (item == NULL) {
        Logger::error("LaunchAppItemPtr", __FUNCTION__, "make_shared_error");
        return NULL;
    }

    item->setCallerId(callerId);
    item->setCallerPid(callerPid);
    item->setKeepAlive(keepAlive);
    item->setShowSplash(noSplash);
    item->setShowSpinner(spinner);
    item->setSubStage(static_cast<int>(AppLaunchingStage::PREPARE_PRELAUNCH));

    Logger::info("LaunchAppItemPtr", __FUNCTION__, task->getAppId(), Logger::format("keepAlive(%B) noSplash(%B) spinner(%B)", keepAlive, noSplash, spinner), params4app.stringify());
    return item;
}

CloseAppItemPtr AppItemFactory::createCloseItem(LifecycleTaskPtr task)
{
    // return std::make_shared<CloseAppItem>(appId, pid, callerId, closeReason);
    return nullptr;
}
