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

#include "core/lifecycle/app_life_status.h"

#include "core/base/logging.h"
#include "core/lifecycle/app_info_manager.h"

const LifeCycleRoutePolicy LifeCycleRouter::invalid_route_policy_ = {
  LifeStatus::INVALID, RouteAction::IGNORE, RouteLog::ERROR};

LifeCycleRouter::LifeCycleRouter() {
}

LifeCycleRouter::~LifeCycleRouter() {
}

void LifeCycleRouter::Init() {
  /////////////////////////////////////////////////////////////
  // general app life status (current -> possible next status)
  lifecycle_route_map_[LifeStatus::STOP] = {
    {LifeStatus::STOP,        RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::PRELOADING,  RouteAction::SET,     RouteLog::NONE},  // fresh launch for preload
    {LifeStatus::LAUNCHING,   RouteAction::SET,     RouteLog::NONE},  // fresh launch
    {LifeStatus::RELAUNCHING, RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::FOREGROUND,  RouteAction::IGNORE,  RouteLog::WARN},  // foreground on SAM respawned
    {LifeStatus::BACKGROUND,  RouteAction::IGNORE,  RouteLog::WARN},  // background on SAM respawned
    {LifeStatus::CLOSING,     RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::PAUSING,     RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::RUNNING,     RouteAction::CONVERT, RouteLog::WARN}
  };
  lifestatus_conv_map_[LifeStatus::STOP] = {
    {LifeStatus::RUNNING, {LifeStatus::BACKGROUND, RouteAction::SET, RouteLog::NONE}}
  };

  lifecycle_route_map_[LifeStatus::PRELOADING] = {
    {LifeStatus::STOP,        RouteAction::SET,     RouteLog::WARN},  // app crash
    {LifeStatus::PRELOADING,  RouteAction::IGNORE,  RouteLog::ERROR}, // consecutive preload launch
    {LifeStatus::LAUNCHING,   RouteAction::SET,     RouteLog::CHECK}, // launch while preloading
    {LifeStatus::RELAUNCHING, RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::FOREGROUND,  RouteAction::SET,     RouteLog::WARN},  // foreground on SAM respawned
    {LifeStatus::BACKGROUND,  RouteAction::IGNORE,  RouteLog::NONE},  // background on SAM respawned
    {LifeStatus::CLOSING,     RouteAction::SET,     RouteLog::CHECK}, // close while preloading
    {LifeStatus::PAUSING,     RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::RUNNING,     RouteAction::CONVERT, RouteLog::WARN}
  };
  lifestatus_conv_map_[LifeStatus::PRELOADING] = {
    {LifeStatus::RUNNING, {LifeStatus::BACKGROUND, RouteAction::SET, RouteLog::NONE}}
  };

  lifecycle_route_map_[LifeStatus::LAUNCHING] = {
    {LifeStatus::STOP,        RouteAction::SET,     RouteLog::WARN},  // app crash
    {LifeStatus::PRELOADING,  RouteAction::IGNORE,  RouteLog::ERROR}, // [should return false]
    {LifeStatus::LAUNCHING,   RouteAction::IGNORE,  RouteLog::CHECK}, // consecutive launch
    {LifeStatus::RELAUNCHING, RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::FOREGROUND,  RouteAction::SET,     RouteLog::NONE},  // normal flow
    {LifeStatus::BACKGROUND,  RouteAction::IGNORE,  RouteLog::WARN},  // keep current status
    {LifeStatus::CLOSING,     RouteAction::SET,     RouteLog::CHECK}, // close while preloading
    {LifeStatus::PAUSING,     RouteAction::IGNORE,  RouteLog::CHECK},
    {LifeStatus::RUNNING,     RouteAction::IGNORE,  RouteLog::NONE}
  };

  lifecycle_route_map_[LifeStatus::RELAUNCHING] = {
    {LifeStatus::STOP,        RouteAction::SET,     RouteLog::WARN},  // app crash
    {LifeStatus::PRELOADING,  RouteAction::IGNORE,  RouteLog::ERROR}, // [should return false]
    {LifeStatus::LAUNCHING,   RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::RELAUNCHING, RouteAction::IGNORE,  RouteLog::CHECK}, // consecutive launch
    {LifeStatus::FOREGROUND,  RouteAction::SET,     RouteLog::NONE},  // normal flow
    {LifeStatus::BACKGROUND,  RouteAction::IGNORE,  RouteLog::WARN},  // keep current status
    {LifeStatus::CLOSING,     RouteAction::SET,     RouteLog::CHECK}, // close while preloading
    {LifeStatus::PAUSING,     RouteAction::IGNORE,  RouteLog::CHECK},
    {LifeStatus::RUNNING,     RouteAction::IGNORE,  RouteLog::NONE}
  };

  lifecycle_route_map_[LifeStatus::FOREGROUND]  = {
    {LifeStatus::STOP,        RouteAction::SET,     RouteLog::WARN},  // app crash
    {LifeStatus::PRELOADING,  RouteAction::IGNORE,  RouteLog::ERROR}, // [should return false]
    {LifeStatus::LAUNCHING,   RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::RELAUNCHING, RouteAction::IGNORE,  RouteLog::WARN},  // keep current status
    {LifeStatus::FOREGROUND,  RouteAction::IGNORE,  RouteLog::WARN},  // keep current status
    {LifeStatus::BACKGROUND,  RouteAction::SET,     RouteLog::NONE},  // app switched or paused
    {LifeStatus::CLOSING,     RouteAction::SET,     RouteLog::NONE},  // close request
    {LifeStatus::PAUSING,     RouteAction::SET,     RouteLog::NONE},
    {LifeStatus::RUNNING,     RouteAction::IGNORE,  RouteLog::NONE}
  };

  lifecycle_route_map_[LifeStatus::BACKGROUND] = {
    {LifeStatus::STOP,        RouteAction::SET,     RouteLog::WARN},  // app crash
    {LifeStatus::PRELOADING,  RouteAction::IGNORE,  RouteLog::ERROR}, // [should return false]
    {LifeStatus::LAUNCHING,   RouteAction::CONVERT, RouteLog::ERROR},
    {LifeStatus::RELAUNCHING, RouteAction::SET,     RouteLog::NONE},
    {LifeStatus::FOREGROUND,  RouteAction::SET,     RouteLog::WARN},  // unexpected
    {LifeStatus::BACKGROUND,  RouteAction::IGNORE,  RouteLog::WARN},  // unexpected
    {LifeStatus::CLOSING,     RouteAction::SET,     RouteLog::NONE},  // close request
    {LifeStatus::PAUSING,     RouteAction::IGNORE,  RouteLog::NONE},  // keep current status
    {LifeStatus::RUNNING,     RouteAction::IGNORE,  RouteLog::NONE}
  };
  lifestatus_conv_map_[LifeStatus::BACKGROUND] = {
    {LifeStatus::LAUNCHING, {LifeStatus::RELAUNCHING, RouteAction::SET, RouteLog::NONE}}
  };

  lifecycle_route_map_[LifeStatus::CLOSING] = {
    {LifeStatus::STOP,        RouteAction::SET,     RouteLog::NONE},  // normal flow
    {LifeStatus::PRELOADING,  RouteAction::IGNORE,  RouteLog::ERROR}, // [should return false]
    {LifeStatus::LAUNCHING,   RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::RELAUNCHING, RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::FOREGROUND,  RouteAction::IGNORE,  RouteLog::CHECK}, // unexpected
    {LifeStatus::BACKGROUND,  RouteAction::IGNORE,  RouteLog::WARN},  // unexpected
    {LifeStatus::CLOSING,     RouteAction::IGNORE,  RouteLog::NONE},  // keep current status
    {LifeStatus::PAUSING,     RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::RUNNING,     RouteAction::IGNORE,  RouteLog::ERROR}
  };

  lifecycle_route_map_[LifeStatus::PAUSING] = {
    {LifeStatus::STOP,        RouteAction::SET,     RouteLog::WARN},  // normal flow
    {LifeStatus::PRELOADING,  RouteAction::IGNORE,  RouteLog::ERROR}, // [should return false]
    {LifeStatus::LAUNCHING,   RouteAction::IGNORE,  RouteLog::ERROR},
    {LifeStatus::RELAUNCHING, RouteAction::SET,     RouteLog::CHECK},
    {LifeStatus::FOREGROUND,  RouteAction::IGNORE,  RouteLog::WARN},  // unexpected
    {LifeStatus::BACKGROUND,  RouteAction::SET,     RouteLog::NONE},  // unexpected
    {LifeStatus::CLOSING,     RouteAction::SET,     RouteLog::CHECK}, // keep current status
    {LifeStatus::PAUSING,     RouteAction::IGNORE,  RouteLog::WARN},
    {LifeStatus::RUNNING,     RouteAction::IGNORE,  RouteLog::ERROR}
  };

  lifeevent_conv_map_[LifeStatus::INVALID]      = LifeEvent::INVALID;
  lifeevent_conv_map_[LifeStatus::STOP]         = LifeEvent::STOP;
  lifeevent_conv_map_[LifeStatus::PRELOADING]   = LifeEvent::PRELOAD;
  lifeevent_conv_map_[LifeStatus::LAUNCHING]    = LifeEvent::LAUNCH;
  lifeevent_conv_map_[LifeStatus::RELAUNCHING]  = LifeEvent::LAUNCH;
  lifeevent_conv_map_[LifeStatus::FOREGROUND]   = LifeEvent::FOREGROUND;
  lifeevent_conv_map_[LifeStatus::BACKGROUND]   = LifeEvent::BACKGROUND;
  lifeevent_conv_map_[LifeStatus::CLOSING]      = LifeEvent::CLOSE;
  lifeevent_conv_map_[LifeStatus::PAUSING]      = LifeEvent::PAUSE;
  lifeevent_conv_map_[LifeStatus::RUNNING]      = LifeEvent::INVALID;

  runtime_status_conv_map_[RuntimeStatus::STOP]        = LifeStatus::STOP;
  runtime_status_conv_map_[RuntimeStatus::LAUNCHING]   = LifeStatus::LAUNCHING;
  runtime_status_conv_map_[RuntimeStatus::PRELOADING]  = LifeStatus::PRELOADING;
  runtime_status_conv_map_[RuntimeStatus::RUNNING]     = LifeStatus::RUNNING;
  runtime_status_conv_map_[RuntimeStatus::REGISTERED]  = LifeStatus::RUNNING;
  runtime_status_conv_map_[RuntimeStatus::PAUSING]     = LifeStatus::PAUSING;
  runtime_status_conv_map_[RuntimeStatus::CLOSING]     = LifeStatus::CLOSING;

  // NOTE: define seperate route policy based on app type if required
  // currently this route map focuses on native apps
  // it doesn't matter for web app and qml apps
  runtime_route_map_[RuntimeStatus::STOP] = {
    {RuntimeStatus::STOP,         RouteAction::IGNORE},
    {RuntimeStatus::LAUNCHING,    RouteAction::SET},
    {RuntimeStatus::PRELOADING,   RouteAction::SET},
    {RuntimeStatus::RUNNING,      RouteAction::SET},
    {RuntimeStatus::REGISTERED,   RouteAction::IGNORE},
    {RuntimeStatus::CLOSING,      RouteAction::IGNORE},
    {RuntimeStatus::PAUSING,      RouteAction::IGNORE}
  };

  runtime_route_map_[RuntimeStatus::LAUNCHING] = {
    {RuntimeStatus::STOP,         RouteAction::SET},
    {RuntimeStatus::LAUNCHING,    RouteAction::IGNORE},
    {RuntimeStatus::PRELOADING,   RouteAction::IGNORE},
    {RuntimeStatus::RUNNING,      RouteAction::SET},
    {RuntimeStatus::REGISTERED,   RouteAction::IGNORE},
    {RuntimeStatus::CLOSING,      RouteAction::IGNORE},
    {RuntimeStatus::PAUSING,      RouteAction::IGNORE}
  };

  runtime_route_map_[RuntimeStatus::PRELOADING] = {
    {RuntimeStatus::STOP,         RouteAction::SET},
    {RuntimeStatus::LAUNCHING,    RouteAction::IGNORE},
    {RuntimeStatus::PRELOADING,   RouteAction::IGNORE},
    {RuntimeStatus::RUNNING,      RouteAction::SET},
    {RuntimeStatus::REGISTERED,   RouteAction::IGNORE},
    {RuntimeStatus::CLOSING,      RouteAction::IGNORE},
    {RuntimeStatus::PAUSING,      RouteAction::IGNORE}
  };

  runtime_route_map_[RuntimeStatus::RUNNING] = {
    {RuntimeStatus::STOP,         RouteAction::SET},
    {RuntimeStatus::LAUNCHING,    RouteAction::IGNORE},
    {RuntimeStatus::PRELOADING,   RouteAction::IGNORE},
    {RuntimeStatus::RUNNING,      RouteAction::IGNORE},
    {RuntimeStatus::REGISTERED,   RouteAction::SET},
    {RuntimeStatus::CLOSING,      RouteAction::SET},
    {RuntimeStatus::PAUSING,      RouteAction::IGNORE}
  };

  runtime_route_map_[RuntimeStatus::REGISTERED] = {
    {RuntimeStatus::STOP,         RouteAction::SET},
    {RuntimeStatus::LAUNCHING,    RouteAction::IGNORE},
    {RuntimeStatus::PRELOADING,   RouteAction::IGNORE},
    {RuntimeStatus::RUNNING,      RouteAction::IGNORE},
    {RuntimeStatus::REGISTERED,   RouteAction::IGNORE},
    {RuntimeStatus::CLOSING,      RouteAction::SET},
    {RuntimeStatus::PAUSING,      RouteAction::IGNORE}
  };

  runtime_route_map_[RuntimeStatus::CLOSING] = {
    {RuntimeStatus::STOP,         RouteAction::SET},
    {RuntimeStatus::LAUNCHING,    RouteAction::IGNORE},
    {RuntimeStatus::PRELOADING,   RouteAction::IGNORE},
    {RuntimeStatus::RUNNING,      RouteAction::IGNORE},
    {RuntimeStatus::REGISTERED,   RouteAction::IGNORE},
    {RuntimeStatus::CLOSING,      RouteAction::IGNORE},
    {RuntimeStatus::PAUSING,      RouteAction::IGNORE}
  };
}

const LifeCycleRoutePolicy& LifeCycleRouter::GetConvRoutePolilcy(
    LifeStatus current, LifeStatus next) {

  if (lifestatus_conv_map_.count(current) == 0) {
    return invalid_route_policy_;
  }

  if (lifestatus_conv_map_[current].count(next) == 0) {
    return invalid_route_policy_;
  }

  return lifestatus_conv_map_[current][next];
}

const LifeCycleRoutePolicy& LifeCycleRouter::GetLifeCycleRoutePolicy(
    LifeStatus current, LifeStatus next) {

  if (lifecycle_route_map_.count(current) == 0) {
    return invalid_route_policy_;
  }
  auto it = std::find_if(lifecycle_route_map_[current].begin(), lifecycle_route_map_[current].end(),
                         [&next](const LifeCycleRoutePolicy policy) {
                            return next == std::get<0>(policy);
                         });
  if (it == lifecycle_route_map_[current].end()) return invalid_route_policy_;

  if (RouteAction::CONVERT == std::get<1>(*it)) {
    return GetConvRoutePolilcy(current, next);
  }

  return (*it);
}

void LifeCycleRouter::SetRuntimeStatus(const std::string& app_id, RuntimeStatus next) {

  RuntimeStatus current = AppInfoManager::instance().runtime_status(app_id);

  auto it = std::find_if(runtime_route_map_[current].begin(), runtime_route_map_[current].end(),
                      [&next](const RuntimeRoutePolicy policy) {
                        return next == policy.first;
                      });
  if (it == runtime_route_map_[current].end() ||
      (*it).second != RouteAction::SET) {
    LOG_INFO(MSGID_RUNTIME_STATUS, 2, PMLOGKS("app_id", app_id.c_str()),
                                      PMLOGKFV("current", "%d", (int)current),
                                      "skip set runtime status (%d)", (int)next);
    return;
  }

  LOG_INFO(MSGID_RUNTIME_STATUS, 3, PMLOGKS("app_id", app_id.c_str()),
                                    PMLOGKFV("current", "%d", (int)current),
                                    PMLOGKFV("new", "%d", (int)next),
                                    "runtime_status_changed");
  AppInfoManager::instance().set_runtime_status(app_id, next);
}

LifeStatus LifeCycleRouter::GetLifeStatusFromRuntimeStatus(RuntimeStatus runtime_status) {
  LifeStatus next_status = LifeStatus::INVALID;
  if (runtime_status_conv_map_.count(runtime_status) != 0) {
    next_status = runtime_status_conv_map_[runtime_status];
  }
  return next_status;
}

LifeEvent LifeCycleRouter::GetLifeEventFromLifeStatus(LifeStatus status) {
  if (lifeevent_conv_map_.count(status) == 0)
    return LifeEvent::INVALID;
  return lifeevent_conv_map_[status];
}
