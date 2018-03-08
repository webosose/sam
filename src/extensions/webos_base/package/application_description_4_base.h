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

#ifndef APPLICATION_DESCRIPTION_4_BASE_H_
#define APPLICATION_DESCRIPTION_4_BASE_H_

#include "core/package/application_description.h"

class ApplicationDescription4Base: public ApplicationDescription {
public:
  ApplicationDescription4Base();
  virtual ~ApplicationDescription4Base();

  virtual bool LoadJson(pbnjson::JValue& jdesc, const AppTypeByDir& type_by_dir);

private:
  // make this class on-copyable
  ApplicationDescription4Base& operator=(const ApplicationDescription4Base&) = delete;
  ApplicationDescription4Base(const ApplicationDescription4Base&) = delete;

};
typedef std::shared_ptr<ApplicationDescription4Base> AppDesc4BasicPtr;

#endif // APPLICATION_DESCRIPTION_4_BASE_H_

