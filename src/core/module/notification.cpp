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

#include <core/util/jutil.h>
#include <core/util/logging.h>
#include <core/util/lsutils.h>
#include <core/util/utils.h>
#include "core/module/notification.h"


ResBundleAdaptor::ResBundleAdaptor()
{
}

ResBundleAdaptor::~ResBundleAdaptor()
{
}

std::string ResBundleAdaptor::getLocString(const std::string& key)
{
    return m_resBundle->getLocString(key);
}

void ResBundleAdaptor::setLocale(const std::string& locale)
{
    if (locale.empty()) {
        LOG_DEBUG("locale is empty");
        return;
    }

    m_resFile = "cppstrings.json";
    m_resPath = "/usr/share/localization/sam";

    m_resBundle = std::make_shared<ResBundle>(locale, m_resFile, m_resPath);
    LOG_DEBUG("Locale is set as %s", locale.c_str());
}

ToastHelper::ToastHelper()
{
}

ToastHelper::~ToastHelper()
{
}

bool ToastHelper::createToast(pbnjson::JValue msg)
{
    LSErrorSafe lserror;

    if (msg.isNull()) {
        LOG_WARNING(MSGID_NOTIFICATION, 0, "msg is empty");
        return false;
    }

    /*
     if(!LSCallOneReply(MemMgrService::instance().getPrivateHandle(),
     "palm://com.webos.notification/createToast",
     JUtil::jsonToString(msg).c_str(),
     toastLsCallBack, NULL, NULL, &lserror))
     {
     LOG_WARNING(MSGID_NOTIFICATION_FAIL, 1, PMLOGKS("NOTI_MSG", JUtil::jsonToString(msg).c_str()), "NOTI send failed");
     return false;
     }
     */
    return true;
}

bool ToastHelper::toastLsCallBack(LSHandle *sh, LSMessage *message, void *user_data)
{
    pbnjson::JValue json = JUtil::parse(LSMessageGetPayload(message), std::string(""));

    if (json.isNull())
        return true;

    bool returnValue = json["returnValue"].asBool();
    if (!returnValue) {
        LOG_WARNING(MSGID_NOTIFICATION_FAIL, 0, "Notification return failed from Noti-Service");
    }

    return true;
}

