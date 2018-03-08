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

#ifndef DB_HANDLER_INTERFACE_H
#define DB_HANDLER_INTERFACE_H

#include <boost/signals2.hpp>
#include <pbnjson.hpp>

class DbHandlerInterface
{
 public:
  DbHandlerInterface() {}
  virtual ~DbHandlerInterface() {}

  virtual void Init() = 0;
  virtual void HandleDbState(bool connection) = 0;
  virtual void ReloadDbData(bool connection) = 0;

  virtual bool InsertData(const pbnjson::JValue& json) = 0;
  virtual bool UpdateData(const pbnjson::JValue& json) = 0;
  virtual bool DeleteData(const pbnjson::JValue& json) = 0;

  boost::signals2::signal<void (const pbnjson::JValue&)> signal_db_loaded_;
};

#endif
