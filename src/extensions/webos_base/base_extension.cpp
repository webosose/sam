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

#include "extensions/webos_base/base_extension.h"

#include "core/base/jutil.h"
#include "core/base/lsutils.h"
#include "core/base/prerequisite_monitor.h"
#include "core/bus/appmgr_service.h"
#include "core/launch_point/launch_point_manager.h"
#include "core/lifecycle/app_info.h"
#include "core/lifecycle/app_life_manager.h"
#include "core/module/service_observer.h"
#include "core/module/subscriber_of_appinstalld.h"
#include "core/module/subscriber_of_bootd.h"
#include "core/module/subscriber_of_configd.h"
#include "core/module/subscriber_of_lsm.h"
#include "core/package/application_manager.h"
#include "extensions/webos_base/base_logs.h"
#include "extensions/webos_base/base_settings.h"

BaseExtension::BaseExtension() {

}

BaseExtension::~BaseExtension() {
}

void BaseExtension::init(PrerequisiteMonitor& prerequisite_monitor) {
  BaseSettingsImpl::instance().Load(NULL);

  AppinstalldSubscriber::instance().SubscribeUpdateInfo(boost::bind(&BaseExtension::OnUpdateInfoChanged, this, _1));

  // to check first app launch finished
  AppLifeManager::instance().signal_launching_finished.connect(boost::bind(&BaseExtension::OnLaunchingFinished, this, _1));

  // set extension handlers for lifecycle interface
  AppLifeManager::instance().set_applifeitem_factory(app_launching_item_factory_);
  AppLifeManager::instance().set_prelauncher_handler(prelauncher_);
  AppLifeManager::instance().set_memory_checker_handler(memory_checker_);
  AppLifeManager::instance().set_lastapp_handler(lastapp_handler_);

  // set extension handlers for package interface
  ApplicationManager::instance().SetAppScanFilter(app_scan_filter_);
  ApplicationManager::instance().SetAppDescriptionFactory(app_description_factory_);

  // set extension handlers for launch point interface
  LaunchPointManager::instance().SetDbHandler(db_handler_);
  LaunchPointManager::instance().SetOrderingHandler(ordering_handler_);
  LaunchPointManager::instance().SetLaunchPointFactory(launch_point_factory_);
}

void BaseExtension::OnReady() {
}

AppDesc4BasicPtr BaseExtension::GetAppDesc(const std::string& app_id) {
  AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
  if (!app_desc)
    return nullptr;
  return std::static_pointer_cast<ApplicationDescription4Base>(app_desc);
}

void BaseExtension::OnUpdateInfoChanged(const pbnjson::JValue& jmsg) {
  LOG_DEBUG("update status: %s\n", JUtil::jsonToString(jmsg).c_str());

  if (!jmsg.hasKey("returnValue") || !jmsg["returnValue"].asBool() ||
      !jmsg["status"].isObject() || !jmsg["status"]["apps"].isArray()) {
    LOG_WARNING(MSGID_INVALID_UPDATE_STATUS_MSG, 1,
                PMLOGJSON("payload", JUtil::jsonToString(jmsg).c_str()), "");
    return;
  }

  const pbnjson::JValue& update_info = jmsg["status"]["apps"];
  int update_info_num = (int) update_info.arraySize();

  LOG_INFO(MSGID_RECEIVED_UPDATE_INFO, 2,
           PMLOGKFV("update_info_num", "%d", update_info_num),
           PMLOGJSON("payload", JUtil::jsonToString(update_info).c_str()), "");

  // clear all previous information
  AppInfoManager::instance().reset_all_update_info();
  AppInfoManager::instance().reset_all_out_of_service_info();

  // set new update information
  for (int i = 0; i < update_info_num; ++i) {
    std::string app_id, category, update_type, update_ver;

    if (!update_info[i].hasKey("id") || !update_info[i]["id"].isString()
        || update_info[i]["id"].asString(app_id) != CONV_OK)
      continue;

    update_type = update_info[i]["status"].asString();
    category = CATEGORY_ON_STORE_APPS;
    update_ver = "";

    if (update_type == OUT_OF_SERVICE)
      AppInfoManager::instance().add_out_of_service_info(app_id);
    else
      AppInfoManager::instance().add_update_info(app_id, update_type, category, update_ver);
  }
}

void BaseExtension::OnLaunchingFinished(AppLaunchingItemPtr item) {
  AppLaunchingItem4BasePtr basic_item = std::static_pointer_cast<AppLaunchingItem4Base>(item);

  if (basic_item->err_text().empty())
    AppLifeManager::instance().set_last_loading_app(basic_item->app_id());
}

