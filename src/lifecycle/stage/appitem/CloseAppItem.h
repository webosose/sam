// Copyright (c) 2017-2018 LG Electronics, Inc.
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

#ifndef APP_CLOSE_ITEM_H_
#define APP_CLOSE_ITEM_H_

#include <lifecycle/ApplicationErrors.h>
#include <package/AppPackage.h>
#include <list>
#include <pbnjson.hpp>
#include "AppItem.h"

class CloseAppItem : public AppItem {
public:
    CloseAppItem(const std::string& app_id, const std::string& pid, const std::string& caller, const std::string& reason);
    virtual ~CloseAppItem();

    bool isMemoryReclaim() const
    {
        if (getReason() == "memoryReclaim") {
            return true;
        }
        return false;
    }
};
typedef std::shared_ptr<CloseAppItem> CloseAppItemPtr;
typedef std::list<CloseAppItemPtr> CloseAppItemList;

#endif
