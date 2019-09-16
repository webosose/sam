// Copyright (c) 2017-2019 LG Electronics, Inc.
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

#include <boost/bind.hpp>
#include <launchpoint/handler/OrderingHandler.h>
#include <setting/Settings.h>

OrderingHandler::OrderingHandler()
{
}

OrderingHandler::~OrderingHandler()
{
}

void OrderingHandler::init()
{
}

void OrderingHandler::handleDBState(bool connection)
{
}

void OrderingHandler::reloadDBData(bool connection)
{
}

void OrderingHandler::makeLaunchPointsInOrder(const std::vector<LaunchPointPtr>& visibleLaunchPoints, const pbnjson::JValue& changed_reason)
{
    m_visibleLPs = visibleLaunchPoints;
    reorder();
}

bool OrderingHandler::SetOrder(const pbnjson::JValue& data, const std::vector<LaunchPointPtr>& visibleLaunchPoints, std::string& errorText)
{
    return false;
}

int OrderingHandler::insertLPInOrder(const std::string& launchPointId, const pbnjson::JValue& data, int position)
{
    int i = 0;
    for (auto it = m_orderedList.begin(); it != m_orderedList.end(); ++it, ++i) {
        if (*it > launchPointId) {
            m_orderedList.insert(it, launchPointId);
            return i;
        }
    }
    m_orderedList.push_back(launchPointId);
    return m_orderedList.size() - 1;
}

int OrderingHandler::updateLPInOrder(const std::string& launchPointId, const pbnjson::JValue& data, int position)
{
    return 0;
}

void OrderingHandler::deleteLPInOrder(const std::string& launchPointId)
{
    auto it = std::find(m_orderedList.begin(), m_orderedList.end(), launchPointId);

    if (it != m_orderedList.end())
        m_orderedList.erase(it);
}

void OrderingHandler::reorder()
{
    m_orderedList.clear();
    for (auto it = m_visibleLPs.begin(); it != m_visibleLPs.end(); ++it) {
        m_orderedList.push_back(it->get()->getLunchPointId());
    }
    std::sort(m_orderedList.begin(), m_orderedList.end(), [](const std::string& a, const std::string& b) -> bool {return (a < b);});
    EventLaunchPointsOrdered(OrderChangeState::FULL);
}
