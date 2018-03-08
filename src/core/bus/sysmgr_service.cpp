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

//->Start of API documentation comment block
/**
@page com_webos_systemmanager com.webos.systemmanager
@brief This is a service to provide system functionality
@{
@}
*/
//->End of API documentation comment block

#include "core/bus/sysmgr_service.h"

#include <string>

#include <boost/filesystem.hpp>
#include <pbnjson.hpp>

#include "core/package/application_manager.h"
#include "core/base/jutil.h"
#include "core/base/logging.h"
#include "core/base/lsutils.h"
#include "core/base/utils.h"
#include "core/module/subscriber_of_bootd.h"
#include "core/setting/settings.h"

static SysMgrService* s_instance = 0;
static const char* g_firt_use_flag = "/var/luna/preferences/ran-firstuse";


SysMgrService* SysMgrService::instance()
{
    if(s_instance)
        return s_instance;

    s_instance = new SysMgrService();
    return s_instance;
}

SysMgrService::SysMgrService()
    : ServiceBase("com.webos.service.systemmanager"),
      m_bootStatus(false)
{
}

SysMgrService::~SysMgrService()
{
}

bool SysMgrService::Attach(GMainLoop* gml)
{
    if(!ServiceBase::Attach(gml))
        return false;

    (void) BootdSubscriber::instance().SubscribeBootStatus(boost::bind(&SysMgrService::postBootStatus, this, _1));

    return true;
}

void SysMgrService::Detach()
{
    ServiceBase::Detach();
}

//->Start of API documentation comment block
/**
@page com_webos_systemmanager com.webos.systemmanager
@{
@section com_webos_systemmanager_getBootStatus getBootStatus

gets the boot status

@par Parameters
None

@par Returns(Call)
Name | Required | Type | Description
-----|--------|------|----------
returnValue  | yes | Boolean | true if booted
subscribed  | yes | Boolean | true if subscribed

@par Returns(Subscription)
Name | Required | Type | Description
-----|--------|------|----------
finished  | yes | Boolean | true if finished
firstUse  | yes | Boolean | false
returnValue  | yes | Boolean | true if returned correctly
subscribed  | yes | Boolean | true if subscribed
@}
*/
//->End of API documentation comment block
bool SysMgrService::cb_getBootStatus(LSHandle* lshandle, LSMessage* message, void* user_data)
{
    LSErrorSafe lserror;
    pbnjson::JValue reply = pbnjson::Object();
    bool success = false;
    bool subscribed = false;

    LOG_WARNING(MSGID_DEPRECATED_API, 4, PMLOGKS("SENDER", LSMessageGetSender(message)),
                                         PMLOGKS("SENDER_SERVICE", LSMessageGetSenderServiceName(message)),
                                         PMLOGKS("SERVICE_NAME", SysMgrService::instance()->name_.c_str()),
                                         PMLOGKS("METHOD", LSMessageGetMethod(message)), "");

    if(LSMessageIsSubscription(message))
    {
        if( !LSSubscriptionProcess(lshandle, message, &subscribed, &lserror) )
        {
            goto Done;
        }
    }

    reply.put("finished", SysMgrService::instance()->getBootStatus());
    reply.put("firstUse", ! (boost::filesystem::exists(g_firt_use_flag)) );

    success = true;
Done:
    reply.put("returnValue", success);
    reply.put("subscribed", subscribed);

    if ( !LSMessageReply(lshandle, message, JUtil::jsonToString(reply).c_str(), &lserror) )
    {
        return false;
    }
    return true;
}

void SysMgrService::postBootStatus(const pbnjson::JValue& jmsg)
{
    bool status = false;
    if (jmsg.hasKey("signals") && jmsg["signals"].isObject() &&
        jmsg["signals"].hasKey("boot-done") && jmsg["signals"]["boot-done"].isBoolean()) {
        status = jmsg["signals"]["boot-done"].asBool();
    }

    if(m_bootStatus == status) return;

    LSErrorSafe lserror;
    pbnjson::JValue request = pbnjson::Object();

    m_bootStatus = status;

    request.put("finished", status);

    std::vector<std::string> categories;
    get_categories(categories);

    std::string category;
    for(auto it = categories.begin(); it != categories.end(); ++it)
    {
        category = *it;

        if(!LSSubscriptionReply(SysMgrService::instance()->ServiceHandle(),
                               (category + "getBootStatus").c_str(),
                               JUtil::jsonToString(request).c_str(), &lserror))
            return;
    }
}


bool SysMgrService::cb_no_op(LSHandle* lshandle, LSMessage *msg,void *user_data)
{
    return false;
}

LSMethod * SysMgrService::get_methods(std::string category) const {
    static LSMethod s_methods[] = {
        { "getBootStatus", SysMgrService::cb_getBootStatus },
        { 0, 0 }
    };
    return s_methods;
}

void SysMgrService::get_categories(std::vector<std::string> &categories) const
{
    categories.push_back("/");
}

