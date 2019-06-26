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

#ifndef BUS_SERVICEOBSERVER_H_
#define BUS_SERVICEOBSERVER_H_

#include <luna-service2/lunaservice.h>
#include <boost/signals2.hpp>
#include <bus/RelatedLunaServiceRoster.h>
#include <bus/RelatedLunaServiceRoster.h>
#include <util/Singleton.h>

class ServiceObserver: public Singleton<ServiceObserver> {
    struct ObserverItem {
        std::string m_name;
        bool m_connection;
        LSMessageToken m_token;
        boost::signals2::signal<void(bool)> signalStatusChanged;

        ObserverItem(const std::string& service_name, boost::function<void(bool)> func)
            : m_name(service_name),
              m_connection(false),
              m_token(0)
        {
            signalStatusChanged.connect(func);
        }
    };
    typedef std::shared_ptr<ObserverItem> ObserverItemPtr;

public:
    void run();
    void stop();
    bool isConnected(const std::string& service_name) const;
    void add(const std::string& service_name, boost::function<void(bool)> func);

private:
    friend class Singleton<ServiceObserver> ;

    ServiceObserver();
    ~ServiceObserver();

    static bool onServerStatus(LSHandle *lshandle, LSMessage *message, void *user_data);

    std::vector<ObserverItemPtr> m_watchingServices;
    bool m_isStarted;
};

#endif // BUS_SERVICEOBSERVER_H_

