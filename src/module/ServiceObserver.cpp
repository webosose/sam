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
        m_isStarted(false)
{
}

ServiceObserver::~ServiceObserver()
{
}

void ServiceObserver::add(const std::string& service_name, boost::function<void(bool)> func)
{

    auto it = std::find_if(m_watchingServices.begin(), m_watchingServices.end(), [&service_name](ObserverItemPtr item) {return service_name == item->m_name;});

    if (it == m_watchingServices.end()) {
        m_watchingServices.push_back(std::make_shared<ObserverItem>(service_name, func));
    } else {
        (*it)->signalStatusChanged.connect(func);
    }

    LOG_INFO(MSGID_SERVICE_OBSERVER_INFORM, 1, PMLOGKS("event", "item_added"), "service: %s", service_name.c_str());

    if (m_isStarted)
        run();
}

bool ServiceObserver::isConnected(const std::string& service_name) const
{

    auto it = std::find_if(m_watchingServices.begin(), m_watchingServices.end(), [&service_name](ObserverItemPtr item) {return service_name == item->m_name;});
    if (it == m_watchingServices.end())
        return false;
    return (*it)->m_connection;
}

void ServiceObserver::run()
{

    LSHandle* lsHandle = AppMgrService::instance().serviceHandle();
    for (auto it : m_watchingServices) {

        if (it->m_token != 0)
            continue;

        LOG_INFO(MSGID_SERVICE_OBSERVER_INFORM, 1, PMLOGKS("event", "enquiring"), "service: %s", it->m_name.c_str());
        std::string payload = "{\"serviceName\":\"" + it->m_name + "\"}";

        LSErrorSafe lserror;
        if (!LSCall(lsHandle, "luna://com.webos.service.bus/signal/registerServerStatus", payload.c_str(), onServerStatus, this, &(it->m_token), &lserror)) {
            LOG_WARNING(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "lscall"), PMLOGKS("where", "register_serv_status"), PMLOGKS("service_name", it->m_name.c_str()), "%s", lserror.message);
            return;
        }
    }

    m_isStarted = true;
}

void ServiceObserver::stop()
{

    LSHandle* lsHandle = AppMgrService::instance().serviceHandle();
    for (auto it : m_watchingServices) {
        it->m_connection = false;
        it->signalStatusChanged.disconnect_all_slots();
        (void) LSCallCancel(lsHandle, it->m_token, NULL);
    }
}

bool ServiceObserver::onServerStatus(LSHandle* lshandle, LSMessage* message, void* user_data)
{

    ServiceObserver* service_observer = static_cast<ServiceObserver*>(user_data);
    LSMessageToken token = LSMessageGetResponseToken(message);

    auto it = std::find_if(service_observer->m_watchingServices.begin(), service_observer->m_watchingServices.end(), [&token](ObserverItemPtr item) {return token == item->m_token;});

    if (it == service_observer->m_watchingServices.end())
        return true;

    ObserverItemPtr watching_item = (*it);

    pbnjson::JValue json;
    bool connection = false;

    json = JUtil::parse(LSMessageGetPayload(message), std::string(""));
    if (json.isNull()) {
        return true;
    }

    connection = json["connected"].asBool();

    if (watching_item->m_connection != connection) {
        LOG_INFO(MSGID_SERVICE_OBSERVER_INFORM, 2, PMLOGKS("service_name", watching_item->m_name.c_str()), PMLOGKS("connection", connection?"on":"off"), "");
        watching_item->m_connection = connection;
        watching_item->signalStatusChanged(connection);
    }

    return true;
}
