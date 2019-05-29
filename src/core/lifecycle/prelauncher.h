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

#ifndef PRELAUNCHER_H_
#define PRELAUNCHER_H_

#include "core/lifecycle/launching_item.h"
#include "interface/lifecycle/prelauncher_interface.h"

class Prelauncher: public PrelauncherInterface {
public:
    Prelauncher();
    ~Prelauncher();

    virtual void add_item(AppLaunchingItemPtr item);
    virtual void remove_item(const std::string& item_uid);
    virtual void input_bridged_return(AppLaunchingItemPtr item, const pbnjson::JValue& jmsg);
    virtual void cancel_all();
};

#endif
