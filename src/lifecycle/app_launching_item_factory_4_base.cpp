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

#include <base_extension.h>
#include <lifecycle/app_launching_item_factory_4_base.h>
#include <lifecycle/app_launching_item_4_base.h>
#include <lifecycle/app_launching_item_factory_4_base.h>
#include <package/AppDescription.h>
#include <setting/settings.h>
#include <util/base_logs.h>
#include <util/jutil.h>

AppLaunchingItemFactory4Base::AppLaunchingItemFactory4Base()
{
}

AppLaunchingItemFactory4Base::~AppLaunchingItemFactory4Base()
{
}

AppLaunchingItemPtr AppLaunchingItemFactory4Base::Create(const std::string& app_id, AppLaunchRequestType rtype, const pbnjson::JValue& params, LSMessage* lsmsg, int& err_code, std::string& err_text)
{
    if (app_id.empty()) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "empty_app_id"),
                  PMLOGKS("where", "tv_launching_item_factory"), "");
        return NULL;
    }

    AppDescPtr app_desc = BaseExtension::instance().GetAppDesc(app_id);
    if (app_desc == nullptr) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "not_exist"),
                  PMLOGKS("where", "AppLaunchingItemFactory4Basic"), "");
        err_code = -101;
        err_text = "not exist";
        return NULL;
    }

    // parse caller info
    std::string caller = getCallerFromMessage(lsmsg);
    std::string caller_id = getCallerID(caller);
    std::string caller_pid = getCallerPID(caller);

    pbnjson::JValue params4app = (params.hasKey("params") && params["params"].isObject()) ? params["params"].duplicate() : pbnjson::Object();

    // this is for WAM, SAM will bypass to WAM. WAM doens't unload the app even if user clicks "X" button to close it.
    bool keep_alive = params.hasKey("keepAlive") && params["keepAlive"].asBool();
    if (SettingsImpl::instance().IsKeepAliveApp(app_id)) {
        keep_alive = true;
    }

    // caller can request not show splash image on launching app (default action is showing splash image)
    bool show_splash = !(params.hasKey("noSplash") && params["noSplash"].asBool());
    // need to check if it's running. set to false if so.
    if (!app_desc->splashOnLaunch()) {
        show_splash = false;
    }

    // caller can request not show spinner on launching app (default action is showing spinner)
    bool show_spinner = true;
    if (params.hasKey("spinner")) {
        show_spinner = params["spinner"].asBool();
    }
    // need to check if it's running. set to false if so.
    if (!app_desc->spinnerOnLaunch()) {
        show_spinner = false;
    }

    AppLaunchingItem4BasePtr new_item = std::make_shared<AppLaunchingItem4Base>(app_id, rtype, params4app, lsmsg);
    if (new_item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "make_shared_error"),
                  PMLOGKS("where", "tv_launching_item_factory"), "");
        return NULL;
    }

    new_item->setCallerId(caller_id);
    new_item->setCallerPid(caller_pid);
    new_item->setKeepAlive(keep_alive);
    new_item->setShowSplash(show_splash);
    new_item->setShowSpinner(show_spinner);
    new_item->setSubStage(static_cast<int>(AppLaunchingStage4Base::PREPARE_PRELAUNCH));

    LOG_INFO(MSGID_APPLAUNCH, 6,
             PMLOGKS("app_id", app_id.c_str()),
             PMLOGKS("caller_id", new_item->callerId().c_str()),
             PMLOGKS("keep_alive", (keep_alive?"true":"false")),
             PMLOGKS("show_splash", (show_splash?"true":"false")),
             PMLOGKS("show_spinner", (show_spinner?"true":"false")),
             PMLOGJSON("params", params4app.stringify().c_str()), "");

    return new_item;
}
