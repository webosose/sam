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

#include "extensions/webos_base/package/app_description_factory_4_base.h"
#include "extensions/webos_base/package/application_description_4_base.h"

AppDescriptionFactory4Base::AppDescriptionFactory4Base() {
}

AppDescriptionFactory4Base::~AppDescriptionFactory4Base() {
}

AppDescPtr AppDescriptionFactory4Base::Create(pbnjson::JValue& jdesc, const AppTypeByDir& type_by_dir) {

  AppDesc4BasicPtr app_desc = std::make_shared<ApplicationDescription4Base>();
  if (!app_desc)
    return nullptr;

  if (app_desc->LoadJson(jdesc, type_by_dir) == false) {
    return nullptr;
  }

  return app_desc;
}
