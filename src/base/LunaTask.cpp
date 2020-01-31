// Copyright (c) 2019 LG Electronics, Inc.
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

#include "LunaTask.h"

#include "AppDescriptionList.h"
#include "RunningAppList.h"
#include "util/JValueUtil.h"

const int LunaTask::getDisplayId() const
{
    // TODO This is temp solution about displayId
    // When home app support peropery. Please detete following code block
    if (getAppId().find("home1") != string::npos) {
       Logger::info("LunaTask", __FUNCTION__, "Reserved AppId. DispalyId is 1");
       return 1;
   } else if (getAppId().find("statusbar1") != string::npos) {
       Logger::info("LunaTask", __FUNCTION__, "Reserved AppId. DispalyId is 1");
       return 1;
   } else if (getCaller() == "com.webos.app.home") {
        RunningAppPtr home = RunningAppList::getInstance().getByWebprocessid(getWebprocessId());
        if (home != nullptr) {
            return home->getDisplayId();
        }
    }

    // TODO currently, only webapp supports multiple display
    AppDescriptionPtr appDesc = AppDescriptionList::getInstance().getByAppId(getAppId());
    if (appDesc != nullptr && appDesc->getLifeHandlerType() != LifeHandlerType::LifeHandlerType_Web)
        return 0;

    int displayId = 0;
    if (!JValueUtil::getValue(m_requestPayload, "displayId", displayId))
        JValueUtil::getValue(m_requestPayload, "params", "displayAffinity", displayId);
    return displayId;
}
