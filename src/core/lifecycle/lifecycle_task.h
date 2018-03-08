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

#ifndef CORE_LIFECYCLE_LIFECYCLE_TASK_H_
#define CORE_LIFECYCLE_LIFECYCLE_TASK_H_

#include "core/bus/appmgr_service.h"
#include "core/package/application_description.h"

enum class LifeCycleTaskType: int8_t {
  Launch = 1,
  Pause,
  Close,
  CloseAll,
};

enum class LifeCycleTaskStatus: int8_t {
  Ready = 1,
  Pending,
  Waiting,
  Done,
};

class LifeCycleTask {
 public:
  LifeCycleTask(LifeCycleTaskType type, LunaTaskPtr luna_task, AppDescPtr app_desc = nullptr)
      : life_cycle_task_type_(type),
        status_(LifeCycleTaskStatus::Ready),
        app_desc_(app_desc),
        luna_task_(luna_task) {}
  virtual ~LifeCycleTask() {}

  LifeCycleTaskType type() const { return life_cycle_task_type_; }
  LifeCycleTaskStatus status() const { return status_; }
  void set_status(LifeCycleTaskStatus status) { status_ = status; }
  void SetError(int32_t code, const std::string& text) { luna_task_->SetError(code, text); }
  const std::string& app_id() const { return app_id_; }
  void SetAppId(const std::string& app_id) { app_id_ = app_id; }
  LunaTaskPtr LunaTask() const { return luna_task_; }
  void Finalize() {
    luna_task_->ReplyResult();
  }
  void Finalize(const pbnjson::JValue& payload) {
    luna_task_->ReplyResult(payload);
  }
  void FinalizeWithError(int32_t code, const std::string& text) {
    SetError(code, text);
    Finalize();
  }

 private:
  LifeCycleTaskType   life_cycle_task_type_;
  LifeCycleTaskStatus status_;
  AppDescPtr          app_desc_;
  std::string         app_id_;
  LunaTaskPtr         luna_task_;
};

typedef std::shared_ptr<LifeCycleTask> LifeCycleTaskPtr;

#endif  // CORE_LIFECYCLE_LIFECYCLE_TASK_CONTROLLER_H_

