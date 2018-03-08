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

#include "extensions/webos_base/package/appscan_filter_4_base.h"

#include "core/setting/settings.h"

#include "extensions/webos_base/base_logs.h"
#include "extensions/webos_base/base_settings.h"

AppScanFilter4Base::AppScanFilter4Base() {
}

AppScanFilter4Base::~AppScanFilter4Base() {
}

bool AppScanFilter4Base::ValidatePreCondition() {
  return true;
}

bool AppScanFilter4Base::ValidatePostCondition(AppTypeByDir type_by_dir, AppDescPtr app_desc) {
  if (app_desc == NULL)
    return false;
  return true;
}
