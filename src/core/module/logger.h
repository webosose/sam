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

#ifndef CORE_MODULE_LOGGER_H_
#define CORE_MODULE_LOGGER_H_

#include <fstream>
#include <pbnjson.hpp>
#include <string>

#include "core/base/singleton.h"
#include "core/lifecycle/app_life_status.h"

class Logger {
 public:
  Logger();
  ~Logger();

  void Init(const std::string& log_path, uint32_t max_log_file = 0, uint32_t max_log_per_file = 0);
  void WriteLog(const std::string& data);

 private:
  void CompressFile();
  void OpenFile();
  bool Exist(const std::string& f);
  std::string GetTime();

  std::ofstream fs_;
  std::string log_path_;
  uint8_t file_no_;
  uint32_t log_no_;
  uint32_t max_log_file_;
  uint32_t max_log_per_file_;
};

class LifeCycleLogger: public Singleton<LifeCycleLogger> {
 public:
  LifeCycleLogger() {}
  ~LifeCycleLogger() {}

  void Init();

 private:
  friend class Singleton<LifeCycleLogger>;

  void LifeEventWatcher(const pbnjson::JValue& payload);
  void LifeStatusWatcher(const std::string& app_id, const LifeStatus& status);

  Logger life_cycle_logger_;
};

#endif //  CORE_MODULE_LOGGER_H_

