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

#include <bus/AppMgrService.h>
#include <luna-service2/lunaservice.h>
#include <module/ServiceObserver.h>

#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/LSUtils.h>

ServiceObserver::ServiceObserver() :
        started_(false)
{
}

ServiceObserver::~ServiceObserver()
{
}

void ServiceObserver::Add(const std::string& service_name, boost::function<void(bool)> func)
{

    auto it = std::find_if(watching_services_.begin(), watching_services_.end(), [&service_name](ObserverItemPtr item) {return service_name == item->name_;});

    if (it == watching_services_.end()) {
        watching_services_.push_back(std::make_shared<ObserverItem>(service_name, func));
    } else {
        (*it)->signalStatusChanged.connect(func);
    }

    LOG_INFO(MSGID_SERVICE_OBSERVER_INFORM, 1, PMLOGKS("event", "item_added"), "service: %s", service_name.c_str());

    if (started_)
        Run();
}

bool ServiceObserver::IsConnected(const std::string& service_name) const
{

    auto it = std::find_if(watching_services_.begin(), watching_services_.end(), [&service_name](ObserverItemPtr item) {return service_name == item->name_;});
    if (it == watching_services_.end())
        return false;
    return (*it)->connection_;
}

void ServiceObserver::Run()
{

    LSHandle* lsHandle = AppMgrService::instance().serviceHandle();
    for (auto it : watching_services_) {

        if (it->token_ != 0)
            continue;

        LOG_INFO(MSGID_SERVICE_OBSERVER_INFORM, 1, PMLOGKS("event", "enquiring"), "service: %s", it->name_.c_str());
        std::string payload = "{\"serviceName\":\"" + it->name_ + "\"}";

        LSErrorSafe lserror;
        if (!LSCall(lsHandle, "luna://com.webos.service.bus/signal/registerServerStatus", payload.c_str(), cbServerStatus, this, &(it->token_), &lserror)) {
            LOG_WARNING(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "lscall"), PMLOGKS("where", "register_serv_status"), PMLOGKS("service_name", it->name_.c_str()), "%s", lserror.message);
            return;
        }
    }

    started_ = true;
}

void ServiceObserver::Stop()
{

    LSHandle* lsHandle = AppMgrService::instance().serviceHandle();
    for (auto it : watching_services_) {
        it->connection_ = false;
        it->signalStatusChanged.disconnect_all_slots();
        (void) LSCallCancel(lsHandle, it->token_, NULL);
    }
}

bool ServiceObserver::cbServerStatus(LSHandle* lshandle, LSMessage* message, void* user_data)
{

    ServiceObserver* service_observer = static_cast<ServiceObserver*>(user_data);
    LSMessageToken token = LSMessageGetResponseToken(message);

    auto it = std::find_if(service_observer->watching_services_.begin(), service_observer->watching_services_.end(), [&token](ObserverItemPtr item) {return token == item->token_;});

    if (it == service_observer->watching_services_.end())
        return true;

    ObserverItemPtr watching_item = (*it);

    pbnjson::JValue json;
    bool connection = false;

    json = JUtil::parse(LSMessageGetPayload(message), std::string(""));
    if (json.isNull()) {
        return true;
    }

    connection = json["connected"].asBool();

    if (watching_item->connection_ != connection) {
        LOG_INFO(MSGID_SERVICE_OBSERVER_INFORM, 2, PMLOGKS("service_name", watching_item->name_.c_str()), PMLOGKS("connection", connection?"on":"off"), "");
        watching_item->connection_ = connection;
        watching_item->signalStatusChanged(connection);
    }

    return true;
}
