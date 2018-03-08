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

#ifndef LAUNCH_POINT_BOOKMARK_4_BASE_H
#define LAUNCH_POINT_BOOKMARK_4_BASE_H

#include "extensions/webos_base/launch_point/launch_point/launch_point_4_base.h"

class LaunchPointBookmark4Base: public LaunchPoint4Base {
public:
  LaunchPointBookmark4Base(const std::string& id, const std::string& lp_id)
      : LaunchPoint4Base(id, lp_id) {
  }

  static LaunchPointPtr Create(const std::string& lp_id, const pbnjson::JValue& data, std::string& errText);
  virtual std::string Update(const pbnjson::JValue& data);
};

#endif /* LAUNCH_POINT_BOOKMARK_4_BASE_H */
