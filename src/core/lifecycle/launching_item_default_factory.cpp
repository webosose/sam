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

#include "core/lifecycle/launching_item_default_factory.h"

AppLaunchingItemDefaultFactory::AppLaunchingItemDefaultFactory()
{

}

AppLaunchingItemDefaultFactory::~AppLaunchingItemDefaultFactory()
{

}

AppLaunchingItemPtr AppLaunchingItemDefaultFactory::Create(const std::string& app_id, AppLaunchRequestType rtype, const pbnjson::JValue& params, LSMessage* lsmsg, int& err_code, std::string& err_text)
{
    if (app_id.empty()) {
        // leave error log
        return NULL;
    }

    AppLaunchingItemPtr new_item = std::make_shared<AppLaunchingItem>(app_id, rtype, params, lsmsg);
    if (new_item == NULL) {
        // leave error log
        return NULL;
    }

    return new_item;
}
