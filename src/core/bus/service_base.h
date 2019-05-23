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

#ifndef SERVICE_BASE_H
#define SERVICE_BASE_H

#include <glib.h>
#include <luna-service2/lunaservice.h>
#include <stdbool.h>

#include <vector>
#include <string>

class ServiceBase {
public:
    ServiceBase(const std::string& name);
    virtual ~ServiceBase();

    virtual bool Attach(GMainLoop* gml);
    virtual void Detach();

    const std::string& ServiceName() const
    {
        return name_;
    }
    LSHandle* ServiceHandle() const
    {
        return handle_;
    }
    bool IsAttached();

protected:
    void AddCompatibleNames(const std::vector<std::string>& names);
    virtual LSMethod* get_methods(std::string category) const = 0;

    virtual void get_categories(std::vector<std::string> &categories) const = 0;

    std::string name_;
    LSHandle* handle_;
    std::vector<std::pair<std::string, LSHandle*>> services_;
};
#endif
