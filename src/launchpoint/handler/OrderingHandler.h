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

#ifndef ORDERING_HANDLER_4_BASE_H
#define ORDERING_HANDLER_4_BASE_H

#include <functional>
#include <pbnjson.hpp>
#include <boost/signals2.hpp>
#include <launchpoint/handler/OrderingHandler.h>
#include <launchpoint/launch_point/LaunchPoint.h>

const int INVALID_POSITION = -1;
const int DEFAULT_POSITION_INVALID = -1;

enum class OrderChangeState {
    FULL = 0,
    PARTIAL
};

class OrderingHandler {
public:
    OrderingHandler();
    virtual ~OrderingHandler();

    virtual void init();
    virtual void handleDBState(bool connection);
    virtual void reloadDBData(bool connection);
    virtual void makeLaunchPointsInOrder(const std::vector<LaunchPointPtr>& visible_lps, const pbnjson::JValue& changed_reason);

    virtual bool SetOrder(const pbnjson::JValue& data, const std::vector<LaunchPointPtr>& visible_lps, std::string& err_text);

    virtual int insertLPInOrder(const std::string& lp_id, const pbnjson::JValue& data, int position = INVALID_POSITION);
    virtual int updateLPInOrder(const std::string& lp_id, const pbnjson::JValue& data, int position = INVALID_POSITION);
    virtual void deleteLPInOrder(const std::string& lp_id);

    virtual std::vector<std::string> getOrderedList()
    {
        return m_orderedList;
    }

    boost::signals2::signal<void (const OrderChangeState&)> signal_launch_points_ordered_;

private:
    void reorder();

    std::vector<std::string> m_orderedList;
    std::vector<LaunchPointPtr> m_visibleLPs;

};

#endif
