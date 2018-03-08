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

#ifndef CORE_BUS_PACKAGE_LUNA_ADATER_H_
#define CORE_BUS_PACKAGE_LUNA_ADATER_H_

#include "core/bus/luna_task.h"
#include "core/package/application_description.h"
#include "core/package/application_manager.h"
#include "core/package/app_scanner.h"

class PackageLunaAdapter {
 public:
  PackageLunaAdapter();
  ~PackageLunaAdapter();

  void Init();

 private:
  void InitLunaApiHandler();
  void RequestController(LunaTaskPtr task);
  void HandleRequest(LunaTaskPtr task);
  void OnReady();
  void OnScanFinished(ScanMode mode, const AppDescMaps& scannced_apps);
  void OnListAppsChanged(const pbnjson::JValue& apps, const std::vector<std::string>& changes, bool dev);
  void OnOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev);
  void OnAppStatusChanged(AppStatusChangeEvent event, AppDescPtr app_desc);

  void ListApps(LunaTaskPtr task);
  void ListAppsForDev(LunaTaskPtr task);
  void GetAppStatus(LunaTaskPtr task);
  void GetAppInfo(LunaTaskPtr task);
  void GetAppBasePath(LunaTaskPtr task);
  void LaunchVirtualApp(LunaTaskPtr task);
  void AddVirtualApp(LunaTaskPtr task);
  void RemoveVirtualApp(LunaTaskPtr task);
  void RegisterVerbsForRedirect(LunaTaskPtr task);
  void RegisterVerbsForResource(LunaTaskPtr task);
  void GetHandlerForExtension(LunaTaskPtr task);
  void ListExtensionMap(LunaTaskPtr task);
  void MimeTypeForExtension(LunaTaskPtr task);
  void GetHandlerForMimeType(LunaTaskPtr task);
  void GetHandlerForMimeTypeByVerb(LunaTaskPtr task);
  void GetHandlerForUrl(LunaTaskPtr task);
  void GetHandlerForUrlByVerb(LunaTaskPtr task);
  void ListAllHandlersForMime(LunaTaskPtr task);
  void ListAllHandlersForMimeByVerb(LunaTaskPtr task);
  void ListAllHandlersForUrl(LunaTaskPtr task);
  void ListAllHandlersForUrlByVerb(LunaTaskPtr task);
  void ListAllHandlersForUrlPattern(LunaTaskPtr task);
  void ListAllHandlersForMultipleMime(LunaTaskPtr task);
  void ListAllHandlersForMultipleUrlPattern(LunaTaskPtr task);

  std::vector<LunaTaskPtr> pending_tasks_on_ready_;
  std::vector<LunaTaskPtr> pending_tasks_on_scanner_;
};

#endif  // CORE_BUS_PACKGE_LUNA_ADAPTER_H_

