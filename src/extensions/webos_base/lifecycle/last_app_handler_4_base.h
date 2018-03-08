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

#ifndef LAST_APP_HANDLER_4_BASE_H
#define LAST_APP_HANDLER_4_BASE_H

#include <luna-service2/lunaservice.h>
#include <pbnjson.hpp>

#include "core/lifecycle/app_life_status.h"
#include "interface/lifecycle/lastapp_handler_interface.h"

class LastAppHandler4Base: public LastAppHandlerInterface {
public:
  LastAppHandler4Base();
  virtual ~LastAppHandler4Base();

  virtual void launch();
  virtual void cancel();

};

#endif
