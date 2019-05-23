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

#include "extensions/webos_base/package/application_description_4_base.h"

#include "core/base/jutil.h"
#include "extensions/webos_base/base_logs.h"
#include "extensions/webos_base/base_settings.h"

ApplicationDescription4Base::ApplicationDescription4Base() :
        ApplicationDescription()
{
}

ApplicationDescription4Base::~ApplicationDescription4Base()
{
}

bool ApplicationDescription4Base::LoadJson(pbnjson::JValue& jdesc, const AppTypeByDir& type_by_dir)
{
    if (ApplicationDescription::LoadJson(jdesc, type_by_dir) == false) {
        return false;
    }

    // TODO: Below keys are product specific values.
    // They should be moved into 'extension'
    appinfo_json_.remove("lockable");
    appinfo_json_.remove("bootLaunchParams");
    appinfo_json_.remove("checkUpdateOnLaunch");
    appinfo_json_.remove("hasPromotion");
    appinfo_json_.remove("inAppSetting");
    appinfo_json_.remove("installTime");
    appinfo_json_.remove("disableBackHistoryAPI");
    appinfo_json_.remove("accessibility");
    appinfo_json_.remove("tileSize");

    return true;
}
