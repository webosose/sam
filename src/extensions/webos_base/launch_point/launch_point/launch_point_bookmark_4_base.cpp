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

#include "extensions/webos_base/launch_point/launch_point/launch_point_bookmark_4_base.h"

#include "core/base/jutil.h"
#include "core/base/logging.h"
#include "core/base/utils.h"

LaunchPointPtr LaunchPointBookmark4Base::Create(const std::string& lp_id,
                                                const pbnjson::JValue& data,
                                                std::string& err_text) {
  std::string app_id = data["id"].asString();
  if (app_id.empty())
    return nullptr;

  LaunchPoint4BasePtr new_lp = std::make_shared<LaunchPointBookmark4Base>(app_id, lp_id);
  if (new_lp == nullptr)
    return nullptr;

  new_lp->SetAttrWithJson(data);

  new_lp->SetLpType(LPType::BOOKMARK);
  new_lp->SetDefaultLp(false);
  new_lp->SetRemovable(true);
  new_lp->SetSystemApp(false);
  new_lp->SetVisible(true);

  return new_lp;
}

std::string LaunchPointBookmark4Base::Update(const pbnjson::JValue& data) {
  SetAttrWithJson(data);
  return "";
}
