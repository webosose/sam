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

#include <core/util/jutil.h>
#include "extensions/webos_base/package/application_description_4_base.h"

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
    m_appinfoJson.remove("lockable");
    m_appinfoJson.remove("bootLaunchParams");
    m_appinfoJson.remove("checkUpdateOnLaunch");
    m_appinfoJson.remove("hasPromotion");
    m_appinfoJson.remove("inAppSetting");
    m_appinfoJson.remove("installTime");
    m_appinfoJson.remove("disableBackHistoryAPI");
    m_appinfoJson.remove("accessibility");
    m_appinfoJson.remove("tileSize");

    return true;
}
