// Copyright (c) 2012-2019 LG Electronics, Inc.
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

#ifndef PACKAGE_APPPACKAGESCANNER_H_
#define PACKAGE_APPPACKAGESCANNER_H_

#include <boost/signals2.hpp>
#include <package/AppPackage.h>
#include "interface/IClassName.h"
#include <iostream>
#include <string>
#include <vector>
#include "util/Logger.h"

using namespace std;
using namespace pbnjson;

typedef std::vector<std::pair<std::string, AppTypeByDir>> BaseScanPaths;
typedef std::map<std::string, AppPackagePtr> AppDescMaps;

enum class ScanMode : int {
    NOT_RUNNING = 0,
    FULL_SCAN,
    PARTIAL_SCAN,
};

class ApplicationScannerListener {
public:
    ApplicationScannerListener() {};
    virtual ~ApplicationScannerListener() {};

};

class AppPackageScanner : public IClassName {
public:
    AppPackageScanner();
    virtual ~AppPackageScanner();

    void setBCP47Info(const std::string& lang, const std::string& script, const std::string& region);

    void addDirectory(std::string path, AppTypeByDir type);
    void removeDirectory(std::string path, AppTypeByDir type);

    bool isRunning() const;
    void run(ScanMode mode);
    AppPackagePtr scanForOneApp(const std::string& appId);

    boost::signals2::signal<void(ScanMode, const AppDescMaps&)> EventAppScanFinished;

private:
    struct AppDir {
        std::string m_path;
        AppTypeByDir m_type;
        bool m_isScanned;
        AppDir(const std::string& path, const AppTypeByDir& type)
            : m_path(path),
              m_type(type),
              m_isScanned(false)
        {
        }
    };

    void RunScanner();
    void scanDir(std::string base_dir, AppTypeByDir type);
    AppPackagePtr scanApp(std::string& path, AppTypeByDir type);
    pbnjson::JValue loadAppInfo(const std::string& app_dir_path, const AppTypeByDir& type_by_dir);
    void registerNewAppDesc(AppPackagePtr new_desc, AppDescMaps& app_roster);

    ScanMode m_scanMode;
    std::vector<AppDir> m_targetDirs;
    AppDescMaps m_appDescMaps;
    std::string m_language;
    std::string m_script;
    std::string m_region;
};

#endif // PACKAGE_APPPACKAGESCANNER_H_
