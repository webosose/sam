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

#ifndef CORE_MODULE_SERVICE_OBSERVER_H_
#define CORE_MODULE_SERVICE_OBSERVER_H_

#include <luna-service2/lunaservice.h>
#include <boost/signals2.hpp>
#include <core/util/singleton.h>

#include "core/base/related_luna_service_roster.h"

class ServiceObserver: public Singleton<ServiceObserver> {
    struct ObserverItem {
        std::string name_;
        bool connection_;
        LSMessageToken token_;
        boost::signals2::signal<void(bool)> signalStatusChanged;

        ObserverItem(const std::string& service_name, boost::function<void(bool)> func) :
                name_(service_name), connection_(false), token_(0)
        {
            signalStatusChanged.connect(func);
        }
    };
    typedef std::shared_ptr<ObserverItem> ObserverItemPtr;

public:
    void Run();
    void Stop();
    bool IsConnected(const std::string& service_name) const;
    void Add(const std::string& service_name, boost::function<void(bool)> func);

private:
    friend class Singleton<ServiceObserver> ;

    ServiceObserver();
    ~ServiceObserver();

    static bool cbServerStatus(LSHandle *lshandle, LSMessage *message, void *user_data);

    std::vector<ObserverItemPtr> watching_services_;
    bool started_;
};

#endif // CORE_MODULE_SERVICE_OBSERVER_H_

