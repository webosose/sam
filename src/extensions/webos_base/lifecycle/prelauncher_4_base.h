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

#ifndef PRELAUNCHER_4_BASE_H
#define PRELAUNCHER_4_BASE_H

#include "extensions/webos_base/lifecycle/app_launching_item_4_base.h"
#include "interface/lifecycle/prelauncher_interface.h"

class Prelauncher4Base: public PrelauncherInterface {
public:
  Prelauncher4Base();
  virtual ~Prelauncher4Base();

  virtual void add_item(AppLaunchingItemPtr item);
  virtual void remove_item(const std::string& app_uid);
  virtual void input_bridged_return(AppLaunchingItemPtr item, const pbnjson::JValue& jmsg);
  virtual void cancel_all();

private:
  static bool cb_return_lscall(LSHandle* handle, LSMessage* lsmsg, void* user_data);
  static bool cb_return_lscall_for_bridged_request(LSHandle* handle, LSMessage* lsmsg, void* user_data);

  void run_stages(AppLaunchingItem4BasePtr prelaunching_item);
  void handle_stages(AppLaunchingItem4BasePtr prelaunching_item);

  void redirect_to_another(AppLaunchingItem4BasePtr prelaunching_item);
  void finish_prelaunching(AppLaunchingItem4BasePtr prelaunching_item);

  AppLaunchingItem4BasePtr get_lscall_request_item_by_token(const LSMessageToken& token);
  AppLaunchingItem4BasePtr get_item_by_uid(const std::string& uid);
  void remove_item_from_lscall_request_list(const std::string& uid);

private:
  AppLaunchingItem4BaseList item_queue_;
  AppLaunchingItem4BaseList lscall_request_list_;

};

#endif
