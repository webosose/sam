// Copyright (c) 2019-2020 LG Electronics, Inc.
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

#define LOG_NAME "LunaTask"

int LunaTask::getDisplayId()
{
    int displayId = -1;
    if (!JValueUtil::getValue(m_requestPayload, "displayId", displayId))
        JValueUtil::getValue(m_requestPayload, "params", "displayAffinity", displayId);
    return displayId;
}

void LunaTask::setDisplayId(const int displayId)
{
    int oldDisplayId = getDisplayId();
    if (oldDisplayId != -1 && oldDisplayId != displayId) {
        Logger::warning(LOG_NAME, __FUNCTION__, Logger::format("DisplayId is not empty: Old(%d) New(%d)", oldDisplayId, displayId));
    }
    m_requestPayload.put("displayId", displayId);
    if (!m_requestPayload.hasKey("params")) {
        m_requestPayload.put("params", pbnjson::Object());
    }
    m_requestPayload["params"].put("displayAffinity", displayId);
}
