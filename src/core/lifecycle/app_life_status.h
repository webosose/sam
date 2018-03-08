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

#ifndef APP_LIFE_STATUS_H_
#define APP_LIFE_STATUS_H_

#include <map>
#include <string>
#include <tuple>
#include <vector>

enum class LifeStatus: int8_t {
  INVALID = -1,
  STOP = 0,
  PRELOADING,
  LAUNCHING,
  RELAUNCHING,
  FOREGROUND,
  BACKGROUND,
  CLOSING,
  PAUSING,
  RUNNING, // internal event
};

enum class LifeEvent: int8_t {
  INVALID = -1,
  SPLASH = 0,
  PRELOAD,
  LAUNCH,
  FOREGROUND,
  BACKGROUND,
  PAUSE,
  CLOSE,
  STOP,
};

enum class RuntimeStatus: int8_t {
  STOP = 0,
  LAUNCHING,
  PRELOADING,
  RUNNING,
  REGISTERED,
  CLOSING,
  PAUSING, // internal status
};

enum class RouteAction: int8_t {
  SET = 0,
  IGNORE,
  CONVERT,
};

enum class RouteLog: int8_t {
  NONE = 0,   // normal flow as expected
  CHECK,      // for checking improvement point later
  WARN,       // can happen
  ERROR,      // should not happen
};

typedef std::tuple<LifeStatus, RouteAction, RouteLog> LifeCycleRoutePolicy;
typedef std::map<LifeStatus, LifeCycleRoutePolicy> LifeStatusConvertPolicy;
typedef std::pair<RuntimeStatus, RouteAction> RuntimeRoutePolicy;

class LifeCycleRouter {
 public:
  LifeCycleRouter();
  ~LifeCycleRouter();

  void Init();
  const LifeCycleRoutePolicy& GetLifeCycleRoutePolicy(LifeStatus current, LifeStatus next);
  void SetRuntimeStatus(const std::string& app_id, RuntimeStatus next);
  LifeStatus GetLifeStatusFromRuntimeStatus(RuntimeStatus runtime_status);
  LifeEvent GetLifeEventFromLifeStatus(LifeStatus status);

 private:
  const LifeCycleRoutePolicy& GetConvRoutePolilcy(LifeStatus current, LifeStatus next);

  std::map<LifeStatus, std::vector<LifeCycleRoutePolicy>> lifecycle_route_map_;
  std::map<LifeStatus, LifeStatusConvertPolicy> lifestatus_conv_map_;
  std::map<RuntimeStatus, std::vector<RuntimeRoutePolicy>> runtime_route_map_;
  std::map<RuntimeStatus, LifeStatus> runtime_status_conv_map_;
  std::map<LifeStatus, LifeEvent> lifeevent_conv_map_;
  static const LifeCycleRoutePolicy invalid_route_policy_;
};

#endif // APP_LIFE_STATUS_H_

