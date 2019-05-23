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

#ifndef LAUNCH_POINT_FACTORY_4_BASE_H
#define LAUNCH_POINT_FACTORY_4_BASE_H

#include "extensions/webos_base/launch_point/launch_point/launch_point_4_base.h"
#include "interface/launch_point/launch_point_factory_interface.h"

#include <map>

class LaunchPointFactory4Basic: public LaunchPointFactoryInterface {
public:
    LaunchPointFactory4Basic();
    virtual ~LaunchPointFactory4Basic();

    virtual LaunchPointPtr CreateLaunchPoint(const LPType type, const std::string& lp_id, const pbnjson::JValue& data, std::string& errText);

private:
    LaunchPointFactory4Basic(const LaunchPointFactory4Basic&);
    LaunchPointFactory4Basic& operator=(const LaunchPointFactory4Basic&);

};

#endif
