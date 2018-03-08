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

#include "extensions/webos_base/base_settings.h"

#include "core/base/jutil.h"
#include "core/setting/settings.h"
#include "core/setting/settings_conf.h"
#include "extensions/webos_base/base_logs.h"
#include "extensions/webos_base/base_settings_conf.h"

BaseSettings::BaseSettings() {
  launch_point_dbkind_ = pbnjson::Object();
  launch_point_permissions_ = pbnjson::Object();
}

BaseSettings::~BaseSettings() {
}

bool BaseSettings::Load(const char* file_path) {
  return LoadSAMConf(file_path);
}

bool BaseSettings::LoadSAMConf(const char* file_path) {
  std::string conf_path = kBasicSettingsFile;

  LOG_DEBUG("Base LoadConf : %s", conf_path.c_str());

  JUtil::Error error;
  pbnjson::JValue root = JUtil::parseFile(conf_path, "", &error);
  if (root.isNull()) {
    LOG_WARNING(MSGID_SETTINGS_ERR, 1, PMLOGKS("FILE", conf_path.c_str()), "");
    return false;
  }

  if (root.hasKey("BootTimeApps") && root["BootTimeApps"].isArray()) {
    for (auto it : root["BootTimeApps"].items()) {
      if (!it.isString())
        continue;
      SettingsImpl::instance().AddBootTimeApp(it.asString());
    }
  }

  if (root.hasKey("CRIUSupportApps") && root["CRIUSupportApps"].isArray()) {
    for (auto it : root["CRIUSupportApps"].items()) {
      if (!it.isString())
        continue;
      SettingsImpl::instance().AddCRIUSupportApp(it.asString());
    }
  }

  if (root.hasKey("LaunchPointDBKind"))
    launch_point_dbkind_ = root["LaunchPointDBKind"];

  if (root.hasKey("LaunchPointDBPermissions"))
    launch_point_permissions_ = root["LaunchPointDBPermissions"];

  return true;
}

