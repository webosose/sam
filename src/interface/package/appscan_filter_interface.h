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

#ifndef INTERFACE_APPSCAN_FILTER_INTERFACE_H_
#define INTERFACE_APPSCAN_FILTER_INTERFACE_H_

#include "core/package/application_description.h"

class AppScanFilterInterface
{
public:
    virtual bool ValidatePreCondition() = 0;
    virtual bool ValidatePostCondition(AppTypeByDir type_by_dir, AppDescPtr app_desc) = 0;

    AppScanFilterInterface() {}
    virtual ~AppScanFilterInterface() {}
};

#endif
