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

#ifndef APP_LAUNCHING_ITEM_4_BASE_H
#define APP_LAUNCHING_ITEM_4_BASE_H

#include <boost/function.hpp>

#include <core/lifecycle/app_launching_item.h>

enum class StageHandlerReturn4Base
    : int {
        GO_NEXT_STAGE, GO_DEPENDENT_STAGE, REDIRECTED, ERROR,
};

enum class StageHandlerType4Base
    : int {
        MAIN_CALL, SUB_CALL, SUB_BRIDGE_CALL, BRIDGE_CALL, DIRECT_CHECK,
};

enum class AppLaunchingStage4Base {
    INVALID = -1, PREPARE_PRELAUNCH = 0, CHECK_EXECUTE, PRELAUNCH_DONE, MEMORY_CHECK_DONE,
};

class AppLaunchingItem4Base;

typedef std::shared_ptr<AppLaunchingItem4Base> AppLaunchingItem4BasePtr;
typedef std::list<AppLaunchingItem4BasePtr> AppLaunchingItem4BaseList;
typedef boost::function<bool(AppLaunchingItem4BasePtr, pbnjson::JValue&)> PayloadMaker4Base;
typedef boost::function<StageHandlerReturn4Base(AppLaunchingItem4BasePtr)> StageHandler4Base;

struct StageItem4Base {
    StageHandlerType4Base handler_type;
    std::string uri;
    PayloadMaker4Base payload_maker;
    StageHandler4Base handler;
    AppLaunchingStage4Base launching_stage;

    StageItem4Base(StageHandlerType4Base _type, const std::string& _uri, PayloadMaker4Base _maker, StageHandler4Base _handler, AppLaunchingStage4Base _stage) :
            handler_type(_type), uri(_uri), payload_maker(_maker), handler(_handler), launching_stage(_stage)
    {
    }
};

typedef std::deque<StageItem4Base> StageItem4BaseList;

class AppLaunchingItem4Base: public AppLaunchingItem {
public:
    AppLaunchingItem4Base(const std::string& appid, AppLaunchRequestType rtype, const pbnjson::JValue& params, LSMessage* lsmsg);
    virtual ~AppLaunchingItem4Base();

    StageItem4BaseList& stage_list()
    {
        return stage_list_;
    }

    void add_stage(StageItem4Base item)
    {
        stage_list_.push_back(item);
    }

    void pop_front_stage_list()
    {
        stage_list_.pop_front();
    }

    void clear_all_stages()
    {
        stage_list_.clear();
    }

private:
    StageItem4BaseList stage_list_;
};

#endif
