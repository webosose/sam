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

#ifndef ORDERING_HANDLER_INTERFACE_H
#define ORDERING_HANDLER_INTERFACE_H

#include <boost/signals2.hpp>
#include <launchpoint/launch_point/LaunchPoint.h>
#include <pbnjson.hpp>

const int INVALID_POSITION = -1;

enum class OrderChangeState {
  FULL = 0,
  PARTIAL
};

class OrderingHandlerInterface {
 public:
  OrderingHandlerInterface() {}
  virtual ~OrderingHandlerInterface() {}

  virtual void Init() = 0;
  virtual void HandleDbState(bool connection) = 0;
  virtual void ReloadDbData(bool connection) = 0;
  virtual void MakeLaunchPointsInOrder(const std::vector<LaunchPointPtr>& visible_lps, const pbnjson::JValue& changed_reason) = 0;

  virtual int  InsertLpInOrder(const std::string& lp_id, const pbnjson::JValue& data, int position = INVALID_POSITION) = 0;
  virtual int  UpdateLpInOrder(const std::string& lp_id, const pbnjson::JValue& data, int position = INVALID_POSITION) = 0;
  virtual void DeleteLpInOrder(const std::string& lp_id) = 0;

  virtual std::vector<std::string> GetOrderedList() = 0;

  boost::signals2::signal<void (const OrderChangeState&)> signal_launch_points_ordered_;
};

#endif
