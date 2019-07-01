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

#include <dirent.h>
#include <package/AppPackageScanner.h>
#include <setting/Settings.h>
#include <sys/stat.h>
#include <util/JUtil.h>
#include <util/Logging.h>

static void TrimPath(std::string &path)
{
    if (path.back() == '/')
        path.erase(std::prev(path.end()));
}

AppPackageScanner::AppPackageScanner()
    : m_scanMode(ScanMode::NOT_RUNNING)
{
}

AppPackageScanner::~AppPackageScanner()
{
}

void AppPackageScanner::setBCP47Info(const std::string& lang, const std::string& script, const std::string& region)
{
    m_language = lang;
    m_script = script;
    m_region = region;
}

void AppPackageScanner::addDirectory(std::string path, AppTypeByDir type)
{
    TrimPath(path);
    auto it = std::find_if(m_targetDirs.begin(), m_targetDirs.end(), [&path,&type] (const AppDir& dir) {return (dir.m_path == path && dir.m_type == type);});
    if (it != m_targetDirs.end())
        return;
    m_targetDirs.push_back(AppDir(path, type));
}

void AppPackageScanner::removeDirectory(std::string path, AppTypeByDir type)
{
    TrimPath(path);
    auto it = std::find_if(m_targetDirs.begin(), m_targetDirs.end(), [&path,&type] (const AppDir& dir) {return (dir.m_path == path && dir.m_type == type);});
    if (it == m_targetDirs.end())
        return;
    m_targetDirs.erase(it);
}

bool AppPackageScanner::isRunning() const
{
    return (m_scanMode == ScanMode::FULL_SCAN || m_scanMode == ScanMode::PARTIAL_SCAN);
}

void AppPackageScanner::run(ScanMode mode)
{
    if (m_targetDirs.empty()) {
        LOG_ERROR(MSGID_APP_SCANNER, 1,
                  PMLOGKS("err", "no_base_app_dir_registered"), "");
        return;
    }
    if (ScanMode::FULL_SCAN != mode && ScanMode::PARTIAL_SCAN != mode)
        return;

    if (ScanMode::FULL_SCAN == mode) {
        for (auto& it : m_targetDirs) {
            it.m_isScanned = false;
        }
    } else if (ScanMode::PARTIAL_SCAN == mode) {
        bool nothing_to_scan = true;
        for (auto& it : m_targetDirs) {
            if (!it.m_isScanned) {
                nothing_to_scan = false;
                break;
            }
        }
        if (nothing_to_scan)
            return;
    }

    m_appDescMaps.clear();
    m_scanMode = mode;

    RunScanner();
}

void AppPackageScanner::RunScanner()
{

    for (auto& it : m_targetDirs) {
        if (it.m_isScanned)
            continue;

        scanDir(it.m_path, it.m_type);
        it.m_isScanned = true;
    }

    ScanMode scanned_mode = m_scanMode;
    m_scanMode = ScanMode::NOT_RUNNING;
    EventAppScanFinished(scanned_mode, m_appDescMaps);

    m_appDescMaps.clear();
}

void AppPackageScanner::scanDir(std::string base_dir, AppTypeByDir type)
{
    TrimPath(base_dir);

    int app_dir_num = 0;
    dirent** app_dir_list = NULL;

    app_dir_num = scandir(base_dir.c_str(), &app_dir_list, 0, alphasort);

    if (app_dir_list == NULL || app_dir_num == 0)
        return;

    for (int idx = 0; idx < app_dir_num; ++idx) {
        if (app_dir_list[idx]) {
            if (app_dir_list[idx]->d_name[0] == '.')
                continue;

            std::string app_dir_path = base_dir + "/" + app_dir_list[idx]->d_name;
            AppPackagePtr new_app_desc = scanApp(app_dir_path, type);
            registerNewAppDesc(new_app_desc, m_appDescMaps);
        }
    }

    if (app_dir_list) {
        for (int idx = 0; idx < app_dir_num; ++idx) {
            if (app_dir_list[idx])
                free(app_dir_list[idx]);
        }
        free(app_dir_list);
        app_dir_list = NULL;
    }
}

AppPackagePtr AppPackageScanner::scanApp(std::string& path, AppTypeByDir appTypeByDir)
{
    TrimPath(path);

    std::size_t pos = path.rfind("/");
    if (std::string::npos == pos || path.length() <= pos) {
        LOG_WARNING(MSGID_APP_SCANNER, 1,
                    PMLOGKS("status", "ignore"),
                    "invalid path: %s", path.c_str());
        return nullptr;
    }

    struct stat dirStat;
    if (stat(path.c_str(), &dirStat) != 0 || (dirStat.st_mode & S_IFDIR) == 0) {
        LOG_WARNING(MSGID_APP_SCANNER, 1,
                    PMLOGKS("status", "ignore"),
                    "app dir not exist: %s", path.c_str());
        return nullptr;
    }

    std::string appId = path.substr(pos + 1);
    if (AppTypeByDir::AppTypeByDir_System_BuiltIn == appTypeByDir) {
        if (SettingsImpl::getInstance().isDeletedSystemApp(appId)) {
            LOG_INFO(MSGID_APP_SCANNER, 1,
                     PMLOGKS("status", "skip"),
                     "deleted system app: %s", path.c_str());
            return nullptr;
        }
    }

    if (AppTypeByDir::AppTypeByDir_Dev == appTypeByDir && !(SettingsImpl::getInstance().m_isDevMode)) {
        LOG_INFO(MSGID_APP_SCANNER, 1,
                 PMLOGKS("status", "skip"),
                 "dev app, but not in devmode now: %s", path.c_str());
        return nullptr;
    }

    pbnjson::JValue appinfo = loadAppInfo(path, appTypeByDir);
    if (appinfo.isNull()) {
        return nullptr;
    }

    AppPackagePtr appPackagePtr = std::make_shared<AppPackage>();
    if (!appPackagePtr) {
        return nullptr;
    }
    if (appPackagePtr->loadJson(appinfo, appTypeByDir) == false) {
        return nullptr;
    }

    // [ Security check ]
    // * app id should be same with folder name
    if (!appPackagePtr->securityChecksVerified()) {
        LOG_INFO(MSGID_APP_SCANNER, 1,
                 PMLOGKS("status", "disallow"),
                 "different app id/dir: %s:%s", appId.c_str(), path.c_str());
        return nullptr;
    }

    // * ignore apps which has privileged ID, but this is not really privileged
    if (AppTypeByDir::AppTypeByDir_Dev == appTypeByDir && appPackagePtr->isPrivileged()) {
        LOG_INFO(MSGID_APP_SCANNER, 1,
                 PMLOGKS("status", "disallow"),
                 "invalid id for dev app: %s:%s", appId.c_str(), path.c_str());
        return nullptr;
    }

    return appPackagePtr;
}

void AppPackageScanner::registerNewAppDesc(AppPackagePtr new_desc, AppDescMaps& app_roster)
{

    if (!new_desc)
        return;

    const std::string& app_id = new_desc->getAppId();
    if (app_roster.count(app_id) == 0) {
        app_roster[app_id] = new_desc;
        return;
    }

    AppPackagePtr current_desc = app_roster[app_id];
    if (current_desc->isHigherVersionThanMe(current_desc, new_desc)) {
        app_roster[app_id] = new_desc;
    } else {

    }
}

AppPackagePtr AppPackageScanner::scanForOneApp(const std::string& app_id)
{

    if (app_id.empty())
        return nullptr;

    AppDescMaps tmp_roster;
    for (const auto& it : m_targetDirs) {
        std::string app_path = it.m_path + "/" + app_id;
        AppPackagePtr new_desc = scanApp(app_path, it.m_type);
        if (new_desc) {
            registerNewAppDesc(new_desc, tmp_roster);
        }
    }
    return (tmp_roster.count(app_id) > 0) ? tmp_roster[app_id] : nullptr;
}

pbnjson::JValue AppPackageScanner::loadAppInfo(const std::string& appDirPath, const AppTypeByDir& appTypeByDir)
{
    // Specify application description depending on available locale string.
    // This string is in BCP-47 format which can contain a language, language-region,
    // or language-region-script. When SAM loads the appinfo.json file,
    // it should look in resources/<language>/appinfo.json,
    // resources/<language>/<region>/appinfo.json,
    // or resources/<language>/<script>/<region>/appinfo.json respectively.
    // (Note that the script dir goes in between the language and region dirs.)

    const std::string default_appinfo_path = appDirPath + "/appinfo.json";

    JUtil::Error error;
    pbnjson::JValue root = JUtil::parseFile(default_appinfo_path, "ApplicationDescription", &error);

    if (root.isNull()) {
        LOG_INFO(MSGID_APP_SCANNER, 1,
                 PMLOGKS("status", "ignore"),
                 "failed_parse_json: %s / reason: %s", default_appinfo_path.c_str(), error.detail().c_str());
        return pbnjson::JValue();
    }

    /// Add folderPath to JSON
    root.put("folderPath", appDirPath);

    if (AppTypeByDir::AppTypeByDir_System_BuiltIn == appTypeByDir ||
        AppTypeByDir::AppTypeByDir_System_Updatable == appTypeByDir ||
        AppTypeByDir::AppTypeByDir_Store == appTypeByDir ||
        AppTypeByDir::AppTypeByDir_External_Store == appTypeByDir) {
    }

    // Localization (base dir is ./resources/)
    std::string resource_base_path = appDirPath + "/resources/";
    static const std::vector<std::string> prohibited_props { "id", "type", "trustLevel" };
    static const std::vector<std::string> supporting_assets { "icon", "largeIcon", "bgImage", "splashBackground" };
    static const std::vector<std::string> img_props { "icon", "miniicon", "mediumIcon", "largeIcon", "splashicon", "splashBackground", "bgImage", "imageForRecents" };

    std::vector<std::string> localization_candidate_dirs;
    localization_candidate_dirs.push_back(resource_base_path + m_language + "/");
    localization_candidate_dirs.push_back(resource_base_path + m_language + "/" + m_region + "/");
    localization_candidate_dirs.push_back(resource_base_path + m_language + "/" + m_script + "/" + m_region + "/");

    // apply localization  (overwrite from low to high)
    for (const auto& localization_dir : localization_candidate_dirs) {
        std::string appinfo_path = localization_dir + "appinfo.json";
        std::string relative_path = localization_dir.substr(appDirPath.length());

        struct stat file_stat;
        if (stat(appinfo_path.c_str(), &file_stat) != 0 || (file_stat.st_mode & S_IFREG) == 0)
            continue;

        pbnjson::JValue local_obj = JUtil::parseFile(appinfo_path, "");

        if (local_obj.isNull()) {
            LOG_INFO(MSGID_APP_SCANNER, 1,
                     PMLOGKS("status", "ignore"),
                     "failed_to_load_localication: %s", localization_dir.c_str());
            continue;
        }

        for (auto json_it : local_obj.children()) {
            std::string key = json_it.first.asString();

            if (!root.hasKey(key) || root[key].getType() != local_obj[key].getType()) {
                LOG_WARNING(MSGID_APP_SCANNER, 2,
                            PMLOGKS("localization", "unmatchted_with_root"),
                            PMLOGKS("key", key.c_str()),
                            "file: %s", appinfo_path.c_str());
                continue;
            }

            if (std::find(prohibited_props.begin(), prohibited_props.end(), key) != prohibited_props.end()) {
                LOG_WARNING(MSGID_APP_SCANNER, 2,
                            PMLOGKS("localization", "prohibited_props"),
                            PMLOGKS("key", key.c_str()),
                            "file: %s", appinfo_path.c_str());
                continue;
            }

            if (std::find(supporting_assets.begin(), supporting_assets.end(), key) != supporting_assets.end()) {
                // check asset variation rule with root value
                std::string root_asset_value = root[key].asString();
                std::string asset_value = local_obj[key].asString();
                // if assets variation rule is specified, dont support localization
                if (root_asset_value.length() > 0 && root_asset_value[0] == '$')
                    continue;
                if (asset_value.length() > 0 && asset_value[0] == '$')
                    continue;
            }

            auto img_prop = std::find(img_props.begin(), img_props.end(), key);
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
