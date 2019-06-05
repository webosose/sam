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

#ifndef ORDERING_HANDLER_H
#define ORDERING_HANDLER_H

#include <launch_point/launch_point/launch_point.h>
#include <launch_point/ordering_handler_interface.h>
#include <pbnjson.hpp>

#include <util/jutil.h>
#include <util/lsutils.h>

const int DEFAULT_POSITION_INVALID = -1;

class OrderingHandler: public OrderingHandlerInterface {
public:
    OrderingHandler()
    {
    }
    virtual ~OrderingHandler()
    {
    }

    virtual void Init();
    virtual void HandleDbState(bool connection);
    virtual void ReloadDbData(bool connection);
    virtual void MakeLaunchPointsInOrder(const std::vector<LaunchPointPtr>& visible_lps, const pbnjson::JValue& changed_reason);

    virtual int InsertLpInOrder(const std::string& lp_id, const pbnjson::JValue& data, int position = INVALID_POSITION);
    virtual int UpdateLpInOrder(const std::string& lp_id, const pbnjson::JValue& data, int position = INVALID_POSITION);
    virtual void DeleteLpInOrder(const std::string& lp_id);

    virtual std::vector<std::string> GetOrderedList()
    {
        return ordered_list_;
    }

private:
    std::vector<std::string> ordered_list_;
};

#endif
