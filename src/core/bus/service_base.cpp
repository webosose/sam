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

#include "core/bus/service_base.h"

#include <glib.h>
#include <luna-service2/lunaservice.h>

#include "core/base/logging.h"
#include "core/base/lsutils.h"

ServiceBase::ServiceBase(const std::string& name) :
        name_(name), handle_(NULL)
{
    services_.push_back( { name, NULL });
}

ServiceBase::~ServiceBase()
{
}

void ServiceBase::AddCompatibleNames(const std::vector<std::string>& names)
{
    for (const auto& n : names) {
        services_.push_back( { n, NULL });
    }
}

bool ServiceBase::Attach(GMainLoop * gml)
{

    bool first_handle = true;
    for (auto& it : services_) {
        LSErrorSafe lse;
        const std::string& name = it.first;

        if (!LSRegister(name.c_str(), &(it.second), &lse)) {
            LOG_ERROR(MSGID_SRVC_REGISTER_FAIL, 1, PMLOGKS("service", name.c_str()), "err: %s", lse.message);
            return false;
        }

        std::vector<std::string> categories;
        get_categories(categories);

        for (const auto& category : categories) {
            if (!LSRegisterCategory(it.second, category.c_str(), get_methods(category), NULL, NULL, &lse)) {
                LOG_ERROR(MSGID_SRVC_CATEGORY_FAIL, 1, PMLOGKS("service", name.c_str()), "err: %s", lse.message);
                return false;
            }

            if (!LSCategorySetData(it.second, category.c_str(), this, &lse)) {
                LOG_ERROR(MSGID_SRVC_CATEGORY_FAIL, 1, PMLOGKS("service", name.c_str()), "err: %s", lse.message);
                return false;
            }
        }

        if (!LSGmainAttach(it.second, gml, &lse)) {
            LOG_ERROR(MSGID_SRVC_ATTACH_FAIL, 1, PMLOGKS("service", name.c_str()), "err: %s", lse.message);
            return false;
        }

        if (first_handle) {
            handle_ = it.second;
            first_handle = false;
        }
    }

    return true;
}

void ServiceBase::Detach()
{

    for (auto& it : services_) {
        LSErrorSafe lse;
        if (!LSUnregister(it.second, &lse)) {
            LOG_WARNING(MSGID_SRVC_DETACH_FAIL, 1, PMLOGKS("service", it.first.c_str()), "err: %s", lse.message);
            return;
        }

        it.second = NULL;
    }

    handle_ = NULL;
}

bool ServiceBase::IsAttached()
{
    return (NULL != handle_);
}
