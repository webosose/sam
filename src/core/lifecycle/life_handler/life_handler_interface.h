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

#ifndef APP_LIFE_HANDLER_INTERFACE_H_
#define APP_LIFE_HANDLER_INTERFACE_H_

#include <boost/signals2.hpp>

#include "core/lifecycle/application_errors.h"
#include "core/lifecycle/app_info.h"
#include "core/lifecycle/launching_item.h"
#include "core/lifecycle/app_close_item.h"

class AppLifeHandlerInterface {
public:
    AppLifeHandlerInterface()
    {
    }
    virtual ~AppLifeHandlerInterface()
    {
    }

    virtual void launch(AppLaunchingItemPtr item) = 0;
    virtual void close(AppCloseItemPtr item, std::string& err_text) = 0;
    virtual void pause(const std::string& app_id, const pbnjson::JValue& params, std::string& err_text, bool send_life_event = true) = 0;

    virtual void clear_handling_item(const std::string& app_id) = 0;
};

#endif
