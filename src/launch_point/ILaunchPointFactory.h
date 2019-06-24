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

#ifndef LAUNCH_POINT_INTERFACE_H
#define LAUNCH_POINT_INTERFACE_H

#include <launch_point/launch_point/LaunchPoint.h>
#include <string>
#include <stdio.h>

class ILaunchPointFactory {
 public:
  ILaunchPointFactory() {}
  virtual ~ILaunchPointFactory() {}

  virtual LaunchPointPtr CreateLaunchPoint(const LPType type, const std::string& lp_id, const pbnjson::JValue& data, std::string& errText) = 0;
};

#endif /* LAUNCH_POINT_INTERFACE_H */
