// Copyright (c) 2017-2019 LG Electronics, Inc.
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

#ifndef PREREQUISITE_BOOTDPREREQUISITEITEM_H_
#define PREREQUISITE_BOOTDPREREQUISITEITEM_H_

#include <iostream>
#include <pbnjson.hpp>
#include <boost/bind.hpp>
#include <boost/signals2.hpp>
#include <bus/client/Bootd.h>
#include <prerequisite/PrerequisiteItem.h>
#include "util/JValueUtil.h"

#include "util/Logging.h"

class BootdPrerequisiteItem: public PrerequisiteItem {
public:
    BootdPrerequisiteItem()
    {
    }

    virtual ~BootdPrerequisiteItem()
    {
        m_eventConnection.disconnect();
    }

    virtual void start()
    {
        m_eventConnection = Bootd::getInstance().EventBootStatusChanged.connect(boost::bind(&BootdPrerequisiteItem::onGetBootStatus, this, _1));
        setStatus(PrerequisiteItemStatus::DOING);
    }

    void onGetBootStatus(const pbnjson::JValue& responsePayload)
    {
        LOG_INFO(MSGID_BOOTSTATUS_RECEIVED, 1,
                 PMLOGJSON(LOG_KEY_PAYLOAD, responsePayload.duplicate().stringify().c_str()), "");

        bool coreBootDone;
        if (!JValueUtil::getValue(responsePayload, "signals", "core-boot-done", coreBootDone))
            return;

        m_eventConnection.disconnect();
        setStatus(PrerequisiteItemStatus::PASSED);
    }
private:
    boost::signals2::connection m_eventConnection;
};

#endif /* PREREQUISITE_BOOTDPREREQUISITEITEM_H_ */
