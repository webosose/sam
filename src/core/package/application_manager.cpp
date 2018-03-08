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

#include "core/package/application_manager.h"

#include <algorithm>
#include <sys/types.h>
#include <utime.h>

#include "core/base/call_chain.h"
#include "core/base/jutil.h"
#include "core/base/logging.h"
#include "core/base/lsutils.h"
#include "core/base/utils.h"
#include "core/bus/appmgr_service.h"
#include "core/lifecycle/app_info_manager.h"
#include "core/lifecycle/app_life_manager.h"
#include "core/module/locale_preferences.h"
#include "core/package/mime_system.h"
#include "core/package/virtual_app_manager.h"
#include "core/setting/settings.h"

const std::string APP_CHANGE_ADDED = "added";
const std::string APP_CHANGE_REMOVED = "removed";
const std::string APP_CHANGE_UPDATED = "updated";

namespace CallChainEventHandler {
  class CheckAppLockStatus : public LSCallItem {
   public:
    CheckAppLockStatus(const std::string& appId)
        : LSCallItem(AppMgrService::instance().ServiceHandle(),
                     "luna://com.webos.settingsservice/batch", "{}") {
      std::string query = std::string(R"(
        {"operations":[
          {"method":"getSystemSettings","params":{"category":"lock","key":"parentalControl"}},
          {"method":"getSystemSettings","params":{
            "category":"lock","key":"applockPerApp","app_id":")")+appId+std::string(R"("}}
        ]})");

      setPayload(query.c_str());
    }

   protected:
    virtual bool onReceiveCall(pbnjson::JValue jmsg) {
      bool received_valid_parental_mode_info = false;
      bool received_valid_lock_info = false;
      bool parental_control_mode_on = true;
      bool app_locked = true;

      if (!jmsg.hasKey("results") || !jmsg["results"].isArray()) {
        LOG_DEBUG("result fail: %s", JUtil::jsonToString(jmsg).c_str());
        setError(PARENTAL_ERR_GET_SETTINGVAL, "invalid return message");
        return false;
      }

      for (int i = 0 ; i < jmsg["results"].arraySize() ; ++i) {
        if (!jmsg["results"][i].hasKey("settings") || !jmsg["results"][i]["settings"].isObject())
          continue;

        if (jmsg["results"][i]["settings"].hasKey("parentalControl") &&
            jmsg["results"][i]["settings"]["parentalControl"].isBoolean()) {
          parental_control_mode_on = jmsg["results"][i]["settings"]["parentalControl"].asBool();
          received_valid_parental_mode_info = true;
        }

        if (jmsg["results"][i]["settings"].hasKey("applockPerApp") &&
            jmsg["results"][i]["settings"]["applockPerApp"].isBoolean()) {
          app_locked = jmsg["results"][i]["settings"]["applockPerApp"].asBool();
          received_valid_lock_info = true;
        }
      }

      if (!received_valid_parental_mode_info || !received_valid_lock_info) {
        LOG_DEBUG("receiving valid result fail: %s", JUtil::jsonToString(jmsg).c_str());
        setError(PARENTAL_ERR_GET_SETTINGVAL, "not found valid lock info");
        return false;
      }

      if (parental_control_mode_on && app_locked) return false;

      return true;
    }
  };

  class CheckPin : public LSCallItem {
   public:
    CheckPin() : LSCallItem(AppMgrService::instance().ServiceHandle(),
                         "luna://com.webos.notification/createPincodePrompt",
                         R"({"promptType":"parental"})") {}
   protected:
    virtual bool onReceiveCall(pbnjson::JValue jmsg) {
      //{"returnValue": true, "matched" : true/false }
      bool matched = false;
      if (!jmsg.hasKey("matched") || !jmsg["matched"].isBoolean() ||
          jmsg["matched"].asBool(matched) != CONV_OK || matched == false) {
        LOG_DEBUG("Pincode is not matched: %s", JUtil::jsonToString(jmsg).c_str());
        setError(PARENTAL_ERR_UNMATCH_PINCODE, "pincode is not matched");
        return false;
      }

      return true;
    }
  };
}

ApplicationManager::ApplicationManager() : first_full_scan_started_(false) {
  app_scanner_.signalAppScanFinished.connect(boost::bind(&ApplicationManager::OnAppScanFinished, this, _1, _2));
}

ApplicationManager::~ApplicationManager() {
  Clear();
}

void ApplicationManager::SetAppScanFilter(AppScanFilterInterface& scan_filter) {
  app_scanner_.SetScanFilter(scan_filter);
}

void ApplicationManager::SetAppDescriptionFactory(AppDescriptionFactoryInterface& factory) {
  app_scanner_.SetAppDescFactory(factory);
}

void ApplicationManager::Clear() {
  app_roster_.clear();
  MimeSystemImpl::instance().clearMimeTable();
}

void ApplicationManager::Init() {
  LocalePreferences::instance().signalLocaleChanged.connect(
      boost::bind(&ApplicationManager::OnLocaleChanged, this, _1, _2, _3));

  AppinstalldSubscriber::instance().SubscribeInstallStatus(
      boost::bind(&ApplicationManager::OnPackageStatusChanged, this, _1, _2));

  // required apps scanning : before rw filesystem ready
  const BaseScanPaths& base_dirs = SettingsImpl::instance().GetBaseAppDirs();
  for (auto& it : base_dirs) {
    app_scanner_.AddDirectory(it.first, it.second);
  }
}

void ApplicationManager::StartPostInit()
{
  // remaining apps scanning : after rw filesystem ready
  const BaseScanPaths& base_dirs = SettingsImpl::instance().GetBaseAppDirs();
  for (auto& it : base_dirs) {
    app_scanner_.AddDirectory(it.first, it.second);
  }

  Scan();
}

void ApplicationManager::ScanInitialApps() {
  auto boot_time_apps = SettingsImpl::instance().GetBootTimeApps();
  for (auto& app_id: boot_time_apps) {
    AppDescPtr app_desc = app_scanner_.ScanForOneApp(app_id);
    if (app_desc) {
      app_roster_[app_id] = app_desc;
    }
  }
}

void ApplicationManager::Scan() {

  if (!first_full_scan_started_) {
    LOG_INFO(MSGID_START_SCAN, 1, PMLOGKS("STATUS", "FIRST_FULL_SCAN"), "");
    first_full_scan_started_ = true;
  }
  app_scanner_.Run(ScanMode::FULL_SCAN);
}

void ApplicationManager::Rescan(const std::vector<std::string>& reason) {

  for (const auto& r : reason) {
    auto it = std::find(scan_reason_.begin(), scan_reason_.end(), r);
    if (it != scan_reason_.end()) continue;
    scan_reason_.push_back(r);
  }

  if (!first_full_scan_started_) return;

  LOG_INFO(MSGID_START_SCAN, 1, PMLOGKS("STATUS", "RESCAN"), "");
  Scan();
}

void ApplicationManager::OnAppInstalled(const std::string& app_id) {

  AppInfoManager::instance().set_execution_lock(app_id, false);

  AppDescPtr new_desc = app_scanner_.ScanForOneApp(app_id);
  if (!new_desc) {
    return;
  }

  // skip if stub type, (stub apps will be loaded on next full scan)
  if (AppType::Stub == new_desc->type()) {
    return;
  }

  // disallow dev apps if current not in dev mode
  if (AppTypeByDir::Dev == new_desc->getTypeByDir() && SettingsImpl::instance().isDevMode == false) {
    return;
  }

  AddApp(app_id, new_desc, AppStatusChangeEvent::APP_INSTALLED);

  // clear web app cache
  if (AppTypeByDir::Dev == new_desc->getTypeByDir() && AppType::Web == new_desc->type()) {
    (void) LSCallOneReply(AppMgrService::instance().ServiceHandle(),
                          "luna://com.palm.webappmanager/discardCodeCache",
                          "{\"force\":true}", NULL, NULL, NULL, NULL);
  }
}

void ApplicationManager::OnAppUninstalled(const std::string& app_id) {

  RemoveApp(app_id, true, AppStatusChangeEvent::APP_UNINSTALLED);
}

const AppDescMaps& ApplicationManager::allApps() {
  return app_roster_;
}

AppDescPtr ApplicationManager::getAppById(const std::string& app_id) {

  if (app_roster_.count(app_id) == 0) return NULL;
  return app_roster_[app_id];
}

void ApplicationManager::ReplaceAppDesc(const std::string& app_id, AppDescPtr new_desc) {

  if (app_roster_.count(app_id) == 0) {
    app_roster_[app_id] = new_desc;
    new_desc->setMimeData();
    return;
  }

  app_roster_[app_id]->clearMimeData();
  app_roster_[app_id] = new_desc;
  new_desc->setMimeData();
}

void ApplicationManager::RemoveAppsOnUSB(const std::string& app_dir, const AppTypeByDir& app_type) {

  std::vector<std::string> candidates_for_removal;

  for (auto& app_it : app_roster_) {
    if (0 == app_it.second->folderPath().find(app_dir) && app_type == app_it.second->getTypeByDir() ) {
      candidates_for_removal.push_back(app_it.second->id());
    }
  }

  // change app_status to close->stop
  AppLifeManager::instance().close_apps(candidates_for_removal, true);

  for (const auto& app_id :  candidates_for_removal) {
    RemoveApp(app_id, true, AppStatusChangeEvent::STORAGE_DETACHED);
  }
}

void ApplicationManager::AddApp(const std::string& app_id, AppDescPtr new_desc, AppStatusChangeEvent event) {

  AppDescPtr current_desc = getAppById(app_id);

  // new app
  if (!current_desc) {
    app_roster_[app_id] = new_desc;
    new_desc->setMimeData();
    PublishOneAppChange(new_desc, APP_CHANGE_ADDED, event);
    return;
  }

  if (AppStatusChangeEvent::STORAGE_ATTACHED == event &&
      current_desc->IsHigherVersionThanMe(current_desc, new_desc) == false) {
    return;
  }

  (void) ReplaceAppDesc(app_id, new_desc);

  if (AppStatusChangeEvent::APP_INSTALLED == event)
    event = AppStatusChangeEvent::UPDATE_COMPLETED;

  PublishOneAppChange(new_desc, APP_CHANGE_UPDATED, event);
}

void ApplicationManager::RemoveApp(const std::string& app_id, bool rescan, AppStatusChangeEvent event) {

  AppDescPtr current_desc = getAppById(app_id);
  if (!current_desc) return;

  current_desc->clearMimeData();
  AppInfoManager::instance().set_execution_lock(app_id, false);

  if (current_desc->IsBuiltinBasedApp() && AppStatusChangeEvent::APP_UNINSTALLED == event) {
    LOG_INFO(MSGID_UNINSTALL_APP, 3, PMLOGKS("app_id", app_id.c_str()),
                                     PMLOGKS("app_type", "system"),
                                     PMLOGKS("location", "rw"),
                                     "remove system-app in read-write area");
    SettingsImpl::instance().setSystemAppAsRemoved(app_id);
  }

  // just remove current app without rescan
  if (!rescan) {
    PublishOneAppChange(current_desc, APP_CHANGE_REMOVED, event);
    app_roster_.erase(app_id);
    AppInfoManager::instance().remove_app_info(app_id);
    return;
  }

  // rescan file system
  AppDescPtr rescanned_desc = app_scanner_.ScanForOneApp(app_id);
  if (!rescanned_desc) {
    PublishOneAppChange(current_desc, APP_CHANGE_REMOVED, event);
    app_roster_.erase(app_id);
    AppInfoManager::instance().remove_app_info(app_id);
    return;
  }

  // compare current app and rescanned app
  // if same
  if (current_desc->IsSame(current_desc, rescanned_desc)) {
    // (still remains in file system)
    return;
  // if not same
  } else {
    ReplaceAppDesc(app_id, rescanned_desc);
    PublishOneAppChange(rescanned_desc, APP_CHANGE_UPDATED, event);
  }

  LOG_INFO(MSGID_UNINSTALL_APP, 2, PMLOGKS("app_id", app_id.c_str()),
                                   PMLOGKS("status", "clear_memory"), "event: %d", (int) event);
}

void ApplicationManager::ReloadApp(const std::string& app_id) {

  AppDescPtr current_desc = getAppById(app_id);
  if (!current_desc) {
    LOG_INFO(MSGID_PACKAGE_STATUS, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("action", "reload"),
                                      "no current description, just skip");
    return;
  }

  AppInfoManager::instance().set_execution_lock(app_id, false);

  AppDescPtr rescanned_desc = app_scanner_.ScanForOneApp(app_id);
  if (!rescanned_desc) {
    PublishOneAppChange(current_desc, APP_CHANGE_REMOVED, AppStatusChangeEvent::APP_UNINSTALLED);
    app_roster_.erase(app_id);
    AppInfoManager::instance().remove_app_info(app_id);
    LOG_INFO(MSGID_PACKAGE_STATUS, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("action", "reload"),
                                      "no app package left, just remove");
    return;
  }

  if (current_desc->IsSame(current_desc, rescanned_desc)) {
    LOG_INFO(MSGID_PACKAGE_STATUS, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("action", "reload"),
                                      "no change, just skip");
    return;
  } else {
    ReplaceAppDesc(app_id, rescanned_desc);
    PublishOneAppChange(rescanned_desc, APP_CHANGE_UPDATED, AppStatusChangeEvent::UPDATE_COMPLETED);
    LOG_INFO(MSGID_PACKAGE_STATUS, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("action", "reload"),
                                      "different package detected, update info now");
  }
}

bool ApplicationManager::LockAppForUpdate(const std::string& appId, bool lock, std::string& err_text) {

  AppDescPtr desc = getAppById(appId);
  if (!desc) {
    err_text = appId + " was not found OR Unsupported Application Type";
    return false;
  }

  LOG_INFO(MSGID_APPLAUNCH_LOCK, 2, PMLOGKS("app_id", appId.c_str()),
                                    PMLOGKS("lock_status", (lock?"locked":"unlocked")), "");
  AppInfoManager::instance().set_execution_lock(appId, lock);
  return true;
}


void ApplicationManager::OnLocaleChanged(
    const std::string& lang, const std::string& region, const std::string& script) {

  app_scanner_.SetBCP47Info(lang, region, script);
  Rescan({"LANG"});
}

void ApplicationManager::OnAppScanFinished(ScanMode mode, const AppDescMaps& scanned_apps) {

  if (ScanMode::FULL_SCAN == mode) {

    // check changes after rescan
    for (const auto& app: scanned_apps) {
      if (app_roster_.count(app.first) == 0) {
        LOG_INFO(MSGID_PACKAGE_STATUS, 1, PMLOGKS("added_after_full_scan", app.first.c_str()), "");
        signal_app_status_changed(AppStatusChangeEvent::APP_INSTALLED, app.second);
      }
    }
    std::vector<std::string> removed_apps;
    for (const auto& app: app_roster_) {
      if (scanned_apps.count(app.first) == 0) {
        LOG_INFO(MSGID_PACKAGE_STATUS, 1, PMLOGKS("removed_after_full_scan", app.first.c_str()), "");
        removed_apps.push_back(app.first);
        signal_app_status_changed(AppStatusChangeEvent::APP_UNINSTALLED, app.second);
      }
    }

    // close removed apps if it's running
    if (!removed_apps.empty())
      AppLifeManager::instance().close_apps(removed_apps, true);

    Clear();
    app_roster_ = scanned_apps;

    for (const auto& it: app_roster_) {
      (it.second)->setMimeData();
    }

    signalAllAppRosterChanged(app_roster_);
    PublishListApps();
    scan_reason_.clear();
  } else if (ScanMode::PARTIAL_SCAN == mode) {

    for (auto& it : scanned_apps) {
      const std::string app_id = it.first;
      AppDescPtr new_desc = it.second;
      AddApp(app_id, new_desc, AppStatusChangeEvent::STORAGE_ATTACHED);
    }
  }
}

void ApplicationManager::OnReadyToUninstallSystemApp(
    pbnjson::JValue result, ErrorInfo err_info, void* user_data) {

  std::string *ptrAppId = static_cast<std::string*>(user_data);
  std::string app_id = *ptrAppId;
  delete ptrAppId;

  if (app_id.empty()) return;

  if (!result.hasKey("returnValue")) {
    LOG_ERROR(MSGID_CALLCHAIN_ERR, 1, PMLOGKS("reason", "key_does_not_exist"), "");
    return;
  }

  if (!result["returnValue"].asBool()) {
    LOG_INFO(MSGID_UNINSTALL_APP, 3, PMLOGKS("app_id", app_id.c_str()),
                                     PMLOGKS("app_type", "system"),
                                     PMLOGKS("status", "invalid_pincode"),
                                     "uninstallation is canceled");
    return;
  }

  RemoveApp(app_id, false, AppStatusChangeEvent::APP_UNINSTALLED);
}

void ApplicationManager::OnReadyToUninstallVirtualApp(
    pbnjson::JValue result, ErrorInfo err_info, void* user_data) {

  std::string *ptrAppId = static_cast<std::string*>(user_data);
  std::string app_id = *ptrAppId;
  delete ptrAppId;

  LOG_INFO(MSGID_REMOVE_VIRTUAL_APP, 2, PMLOGKS("app_id", app_id.c_str()),
                                        PMLOGKS("status", "ready_to_uninstall"), "");

  if (app_id.empty()) return;

  if (!result.hasKey("returnValue")) {
    LOG_ERROR(MSGID_CALLCHAIN_ERR, 1, PMLOGKS("reason", "key_does_not_exist"), "");
    return;
  }

  if (!result["returnValue"].asBool()) {
    LOG_INFO(MSGID_UNINSTALL_APP, 3, PMLOGKS("app_id", app_id.c_str()),
                                     PMLOGKS("app_type", "virtual_app"),
                                     PMLOGKS("status", "invalid_pincode"), "uninstallation is canceled");
  }

  std::string error_text = "";
  AppLifeManager::instance().close_by_app_id(app_id, "", "", error_text, false);
  VirtualAppManager::instance().RemoveVirtualAppPackage(app_id, VirtualAppType::REGULAR);
}


void ApplicationManager::UninstallApp(const std::string& id, std::string& errorReason) {

  AppDescPtr appDesc = getAppById(id);
  if (!appDesc) return;

  if (!appDesc->isRemovable()) {
    errorReason = id + " is not removable";
    return;
  }

  LOG_INFO(MSGID_UNINSTALL_APP, 1, PMLOGKS("app_id", id.c_str()), "start_uninstall_app");

  if (AppTypeByDir::System_BuiltIn == appDesc->getTypeByDir()) {
    CallChain& callchain = CallChain::acquire(
            boost::bind(&ApplicationManager::OnReadyToUninstallSystemApp, this, _1, _2, _3),
            NULL, new std::string(appDesc->id()));

    if (appDesc->isLockable() && appDesc->isVisible()) {
      LOG_DEBUG("%s : check parental lock to remove %s app", __FUNCTION__, id.c_str());
      auto settingValPtr = std::make_shared<CallChainEventHandler::CheckAppLockStatus>(appDesc->id());
      auto pinPtr = std::make_shared<CallChainEventHandler::CheckPin>();
      callchain.add(settingValPtr).add_if(settingValPtr, false, pinPtr);
    }
    callchain.run();
  }
  else if(AppTypeByDir::Alias == appDesc->getTypeByDir()) {
    CallChain& callchain = CallChain::acquire(
            boost::bind(&ApplicationManager::OnReadyToUninstallVirtualApp, this, _1, _2, _3),
            NULL, new std::string(appDesc->id()));

    if (appDesc->isLockable() && appDesc->isVisible()) {
      LOG_DEBUG("%s : check parental lock to remove %s app", __FUNCTION__, id.c_str());
      auto settingValPtr = std::make_shared<CallChainEventHandler::CheckAppLockStatus>(appDesc->id());
      auto pinPtr = std::make_shared<CallChainEventHandler::CheckPin>();
      callchain.add(settingValPtr).add_if(settingValPtr, false, pinPtr);
    }
    callchain.run();
  }
  else {
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("id", id);
    payload.put("subscribe", false);

    LSErrorSafe lserror;

    if(!LSCallOneReply(AppMgrService::instance().ServiceHandle(),
                        "luna://com.webos.appInstallService/remove",
                        JUtil::jsonToString(payload).c_str(),
                        cbAppUninstalled,
                        NULL, NULL, &lserror)) {
        errorReason = lserror.message;
        return;
    }

    LOG_INFO(MSGID_UNINSTALL_APP, 1, PMLOGKS("app_id", id.c_str()), "requested_to_appinstalld");
  }
}

bool ApplicationManager::cbAppUninstalled(LSHandle* lshandle, LSMessage *message, void* data) {

  pbnjson::JValue jmsg = JUtil::parse( LSMessageGetPayload(message), std::string(""));

  if (jmsg.isNull()) {
    LOG_ERROR(MSGID_UNINSTALL_APP_ERR, 1, PMLOGKS("status", "received_return_false"), "null jmsg");
  }

  LOG_INFO(MSGID_UNINSTALL_APP, 1, PMLOGKS("status", "recieved_result"),
                                   "%s", JUtil::jsonToString(jmsg).c_str());

  return true;
}

void ApplicationManager::OnPackageStatusChanged(const std::string& app_id, const PackageStatus& status) {

  if (PackageStatus::Installed == status) OnAppInstalled(app_id);
  else if (PackageStatus::InstallFailed == status) ReloadApp(app_id);
  else if (PackageStatus::Uninstalled == status) OnAppUninstalled(app_id);
  else if (PackageStatus::UninstallFailed == status) ReloadApp(app_id);
  else if (PackageStatus::ResetToDefault == status) ReloadApp(app_id);
}

std::string ApplicationManager::AppStatusChangeEventToString(const AppStatusChangeEvent& event) {
  switch (event) {
    case AppStatusChangeEvent::NOTHING:
      return "nothing";
      break;
    case AppStatusChangeEvent::APP_INSTALLED:
      return "appInstalled";
      break;
    case AppStatusChangeEvent::APP_UNINSTALLED:
      return "appUninstalled";
      break;
    case AppStatusChangeEvent::STORAGE_ATTACHED:
      return "storageAttached";
      break;
    case AppStatusChangeEvent::STORAGE_DETACHED:
      return "storageDetached";
      break;
    case AppStatusChangeEvent::UPDATE_COMPLETED:
      return "updateCompleted";
      break;
    default:
      return "unknown";
      break;
  }
}

void ApplicationManager::PublishListApps() {

  pbnjson::JValue apps = pbnjson::Array();
  pbnjson::JValue dev_apps = pbnjson::Array();

  for (auto it : app_roster_) {
    apps.append(it.second->toJValue());

    if (SettingsImpl::instance().isDevMode && AppTypeByDir::Dev == it.second->getTypeByDir())
      dev_apps.append(it.second->toJValue());
  }

  signalListAppsChanged(apps, scan_reason_, false);

  if (SettingsImpl::instance().isDevMode)
    signalListAppsChanged(dev_apps, scan_reason_, true);
}

void ApplicationManager::PublishOneAppChange(AppDescPtr app_desc, const std::string& change, AppStatusChangeEvent event) {

  pbnjson::JValue app = app_desc->toJValue();
  std::string reason = (event != AppStatusChangeEvent::NOTHING) ? AppStatusChangeEventToString(event) : "";

  signalOneAppChanged(app, change, reason, false);
  if (SettingsImpl::instance().isDevMode && AppTypeByDir::Dev == app_desc->getTypeByDir())
    signalOneAppChanged(app, change, reason, true);

  signal_app_status_changed(event, app_desc);
}
