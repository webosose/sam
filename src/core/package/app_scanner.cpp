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

#include "core/package/app_scanner.h"

#include <dirent.h>
#include <sys/stat.h>

#include "core/base/jutil.h"
#include "core/base/logging.h"
#include "core/setting/settings.h"

static void TrimPath(std::string &path) {
  if (path.back() == '/')
    path.erase(std::prev(path.end()));
}

AppScanner::AppScanner()
    : app_desc_factory_(NULL),
      app_scan_filter_(NULL),
      scan_mode_(ScanMode::NOT_RUNNING) {
}

AppScanner::~AppScanner() {
}

void AppScanner::SetBCP47Info(const std::string& lang, const std::string& script, const std::string& region) {
  language_ = lang;
  script_   = script;
  region_   = region;
}

void AppScanner::AddDirectory(std::string path, AppTypeByDir type) {
  TrimPath(path);
  auto it = std::find_if(target_dirs_.begin(), target_dirs_.end(),
              [&path,&type] (const AppDir& dir){ return (dir.path_ == path && dir.type_ == type); });
  if (it != target_dirs_.end()) return;
  target_dirs_.push_back(AppDir(path, type));
}

void AppScanner::RemoveDirectory(std::string path, AppTypeByDir type) {
  TrimPath(path);
  auto it = std::find_if(target_dirs_.begin(), target_dirs_.end(),
              [&path,&type] (const AppDir& dir){ return (dir.path_ == path && dir.type_ == type); });
  if (it == target_dirs_.end()) return;
  target_dirs_.erase(it);
}

bool AppScanner::isRunning() const {
  return (scan_mode_ == ScanMode::FULL_SCAN || scan_mode_ == ScanMode::PARTIAL_SCAN);
}

void AppScanner::Run(ScanMode mode) {
  if (app_desc_factory_ == NULL) {
    LOG_ERROR(MSGID_APP_SCANNER, 1, PMLOGKS("err", "no_app_description_factory_registered"), "");
    return;
  }
  if (target_dirs_.empty()) {
    LOG_ERROR(MSGID_APP_SCANNER, 1, PMLOGKS("err", "no_base_app_dir_registered"), "");
    return;
  }
  if (ScanMode::FULL_SCAN != mode && ScanMode::PARTIAL_SCAN != mode) return;

  if (ScanMode::FULL_SCAN == mode) {
    for (auto& it: target_dirs_) {
      it.is_scanned_ = false;
    }
  } else if(ScanMode::PARTIAL_SCAN == mode) {
    bool nothing_to_scan = true;
    for (auto& it: target_dirs_) {
      if (!it.is_scanned_) { nothing_to_scan = false; break; }
    }
    if (nothing_to_scan) return;
  }

  app_desc_maps_.clear();
  scan_mode_ = mode;

  RunScanner();
}

void AppScanner::RunScanner() {

  for (auto& it : target_dirs_) {
    if (it.is_scanned_) continue;

    ScanDir(it.path_, it.type_);
    it.is_scanned_ = true;
  }

  ScanMode scanned_mode = scan_mode_;
  scan_mode_ = ScanMode::NOT_RUNNING;
  signalAppScanFinished(scanned_mode, app_desc_maps_);

  app_desc_maps_.clear();
}

void AppScanner::ScanDir(std::string base_dir, AppTypeByDir type) {
  TrimPath(base_dir);

  int app_dir_num = 0;
  dirent** app_dir_list = NULL;

  app_dir_num = scandir(base_dir.c_str(), &app_dir_list, 0, alphasort);

  if (app_dir_list == NULL || app_dir_num == 0) return;

  for (int idx = 0 ; idx < app_dir_num ; ++idx) {
    if (app_dir_list[idx]) {
      if (app_dir_list[idx]->d_name[0] == '.') continue;

      std::string app_dir_path = base_dir + "/" + app_dir_list[idx]->d_name;
      AppDescPtr new_app_desc = ScanApp(app_dir_path, type);
      RegisterNewAppDesc(new_app_desc, app_desc_maps_);
    }
  }

  if (app_dir_list) {
    for (int idx = 0 ; idx < app_dir_num ; ++idx) {
      if (app_dir_list[idx]) free(app_dir_list[idx]);
    }
    free(app_dir_list);
    app_dir_list = NULL;
  }
}

AppDescPtr AppScanner::ScanApp(std::string& path, AppTypeByDir type) {
  TrimPath(path);

  if (!app_desc_factory_) {
    LOG_WARNING(MSGID_APP_SCANNER, 1, PMLOGKS("status", "ignore"), "no_app_description_factory_registered");
    return nullptr;
  }

  std::size_t pos = path.rfind("/");
  if (std::string::npos == pos || path.length() <= pos) {
    LOG_WARNING(MSGID_APP_SCANNER, 1, PMLOGKS("status", "ignore"), "invalid path: %s", path.c_str());
    return nullptr;
  }

  struct stat dir_stat;
  if (stat(path.c_str(), &dir_stat) != 0 || (dir_stat.st_mode & S_IFDIR) == 0) {
    LOG_WARNING(MSGID_APP_SCANNER, 1, PMLOGKS("status", "ignore"), "app dir not exist: %s", path.c_str());
    return nullptr;
  }

  std::string app_id = path.substr(pos+1);

  if (AppTypeByDir::System_BuiltIn == type) {
    if (SettingsImpl::instance().isDeletedSystemApp(app_id)) {
      LOG_INFO(MSGID_APP_SCANNER, 1, PMLOGKS("status", "skip"), "deleted system app: %s", path.c_str());
      return nullptr;
    }
  }

  if (AppTypeByDir::Dev == type && !(SettingsImpl::instance().isDevMode)) {
    LOG_INFO(MSGID_APP_SCANNER, 1, PMLOGKS("status", "skip"), "dev app, but not in devmode now: %s", path.c_str());
    return nullptr;
  }

  pbnjson::JValue jdesc = LoadAppInfo(path, type);
  if (jdesc.isNull()) {
    return nullptr;
  }
  AppDescPtr app_desc = app_desc_factory_->Create(jdesc, type);
  if (!app_desc) {
    return nullptr;
  }

  // [ Security check ]
  // * app id should be same with folder name
  if (!app_desc->securityChecksVerified()) {
    LOG_INFO(MSGID_APP_SCANNER, 1, PMLOGKS("status", "disallow"),
                                   "different app id/dir: %s:%s", app_id.c_str(), path.c_str());
    return nullptr;
  }

  // * ignore apps which has privileged ID, but this is not really privileged
  if (AppTypeByDir::Dev == type && app_desc->isPrivileged()) {
    LOG_INFO(MSGID_APP_SCANNER, 1, PMLOGKS("status", "disallow"),
                                   "invalid id for dev app: %s:%s", app_id.c_str(), path.c_str());
    return nullptr;
  }

  // [ run post filter ]
  if (app_scan_filter_ != NULL) {
    if (app_scan_filter_->ValidatePostCondition(type, app_desc) == false) {
      return nullptr;
    }
  }

  return app_desc;
}

void AppScanner::RegisterNewAppDesc(AppDescPtr new_desc, AppDescMaps& app_roster) {

  if (!new_desc) return;

  const std::string& app_id = new_desc->id();
  if (app_roster.count(app_id) == 0) {
    app_roster[app_id] = new_desc;
    return;
  }

  AppDescPtr current_desc = app_roster[app_id];
  if (current_desc->IsHigherVersionThanMe(current_desc, new_desc)) {
    app_roster[app_id] = new_desc;
  } else {

  }
}

AppDescPtr AppScanner::ScanForOneApp(const std::string& app_id) {

  if (app_id.empty()) return nullptr;

  AppDescMaps tmp_roster;
  for (const auto& it : target_dirs_) {
    std::string app_path = it.path_ + "/" + app_id;
    AppDescPtr new_desc = ScanApp(app_path, it.type_);
    if (new_desc) {
      RegisterNewAppDesc(new_desc, tmp_roster);
    }
  }
  return (tmp_roster.count(app_id) > 0) ? tmp_roster[app_id] : nullptr;
}

pbnjson::JValue AppScanner::LoadAppInfo(const std::string& app_dir_path, const AppTypeByDir& type_by_dir) {

  // Specify application description depending on available locale string.
  // This string is in BCP-47 format which can contain a language, language-region,
  // or language-region-script. When SAM loads the appinfo.json file,
  // it should look in resources/<language>/appinfo.json,
  // resources/<language>/<region>/appinfo.json,
  // or resources/<language>/<script>/<region>/appinfo.json respectively.
  // (Note that the script dir goes in between the language and region dirs.)

  const std::string default_appinfo_path  = app_dir_path + "/appinfo.json";

  pbnjson::JValue root = JUtil::parseFile(default_appinfo_path, "ApplicationDescription");

  if (root.isNull()) {
    LOG_INFO(MSGID_APP_SCANNER, 1, PMLOGKS("status", "ignore"),
                                   "failed_parse_json: %s", default_appinfo_path.c_str());
    return pbnjson::JValue();
  }

  /// Add folderPath to JSON
  root.put("folderPath", app_dir_path);

  if (AppTypeByDir::System_BuiltIn == type_by_dir ||
      AppTypeByDir::System_Updatable == type_by_dir ||
      AppTypeByDir::Store == type_by_dir ||
      AppTypeByDir::External_Store == type_by_dir) {

    struct stat file_info;
    if (stat(default_appinfo_path.c_str(), &file_info) != 0)
      root.put("installTime", -1);
    else
      root.put("installTime", (int32_t)(file_info.st_mtime));
  }

  // Localization (base dir is ./resources/)
  std::string resource_base_path = app_dir_path + "/resources/";
  static const std::vector<std::string> prohibited_props {"id", "type", "trustLevel"};
  static const std::vector<std::string> supporting_assets {"icon", "largeIcon", "bgImage", "splashBackground"};
  static const std::vector<std::string> img_props {"icon", "miniicon", "mediumIcon", "largeIcon",
                                               "splashicon", "splashBackground", "bgImage", "imageForRecents"};

  std::vector<std::string> localization_candidate_dirs;
  localization_candidate_dirs.push_back(resource_base_path + language_ + "/");
  localization_candidate_dirs.push_back(resource_base_path + language_ + "/" + region_ + "/");
  localization_candidate_dirs.push_back(resource_base_path + language_ + "/" + script_ + "/" + region_ + "/");

  // apply localization  (overwrite from low to high)
  for (const auto& localization_dir : localization_candidate_dirs) {
    std::string appinfo_path = localization_dir + "appinfo.json";
    std::string relative_path = localization_dir.substr(app_dir_path.length());

    struct stat file_stat;
    if (stat(appinfo_path.c_str(), &file_stat) != 0 || (file_stat.st_mode & S_IFREG) == 0) continue;

    pbnjson::JValue local_obj = JUtil::parseFile(appinfo_path, "");

    if (local_obj.isNull()) {
      LOG_INFO(MSGID_APP_SCANNER, 1, PMLOGKS("status", "ignore"),
                                     "failed_to_load_localication: %s", localization_dir.c_str());
      continue;
    }

    for (auto json_it : local_obj.children()) {
      std::string key = json_it.first.asString();

      if (!root.hasKey(key) || root[key].getType() != local_obj[key].getType()) {
        LOG_WARNING(MSGID_APP_SCANNER, 2, PMLOGKS("localization", "unmatchted_with_root"),
                                          PMLOGKS("key", key.c_str()),
                                          "file: %s", appinfo_path.c_str());
        continue;
      }

      if (std::find(prohibited_props.begin(), prohibited_props.end(), key) != prohibited_props.end()) {
        LOG_WARNING(MSGID_APP_SCANNER, 2, PMLOGKS("localization", "prohibited_props"),
                                          PMLOGKS("key", key.c_str()),
                                          "file: %s", appinfo_path.c_str());
        continue;
      }

      if (std::find (supporting_assets.begin(), supporting_assets.end(), key) != supporting_assets.end()) {
        // check asset variation rule with root value
        std::string root_asset_value = root[key].asString();
        std::string asset_value = local_obj[key].asString();
        // if assets variation rule is specified, dont support localization
        if (root_asset_value.length() > 0 && root_asset_value[0] == '$') continue;
        if (asset_value.length() > 0 && asset_value[0] == '$') continue;
      }

      auto img_prop = std::find (img_props.begin(), img_props.end(), key);
      if (img_prop != img_props.end()) {
        std::string img_value = relative_path + local_obj[key].asString();
        root.put(key, img_value);
      } else {
        root.put(key, local_obj[key]);
      }
    }
  }

  return root;
}
