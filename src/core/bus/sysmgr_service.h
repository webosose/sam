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

#ifndef SYSMGRSERVICE_H
#define SYSMGRSERVICE_H

#include <glib.h>
#include <luna-service2/lunaservice.h>
#include <pbnjson.hpp>
#include <stdbool.h>

#include <memory>
#include <set>

#include "core/bus/service_base.h"

class SysMgrService: public ServiceBase {
public:
    static SysMgrService* instance();

    SysMgrService();
    ~SysMgrService();

    virtual bool Attach(GMainLoop* gml);
    virtual void Detach();

    bool getBootStatus()
    {
        return m_bootStatus;
    }
    void setBootStatus(bool status)
    {
        m_bootStatus = status;
    }

    void postBootStatus(const pbnjson::JValue& jmsg);

protected:
    LSMethod* get_methods(std::string category) const;
    void get_categories(std::vector<std::string> &categories) const;

private:
    static bool cb_no_op(LSHandle* lshandle, LSMessage *massage, void *user_data);
    static bool cb_getBootStatus(LSHandle* lshandle, LSMessage *message, void *user_data);

    bool m_bootStatus;
};
#endif /* SYSMGRSERVICE_H */
