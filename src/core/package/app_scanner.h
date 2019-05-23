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

#ifndef CORE_PACKAGE_APP_SCANNER_H_
#define CORE_PACKAGE_APP_SCANNER_H_

#include <boost/signals2.hpp>
#include <string>
#include <vector>

#include "core/package/application_description.h"
#include "interface/package/appscan_filter_interface.h"
#include "interface/package/app_description_factory_interface.h"

typedef std::vector<std::pair<std::string, AppTypeByDir>> BaseScanPaths;
typedef std::map<std::string, AppDescPtr> AppDescMaps;

enum class ScanMode
    : int {
        NOT_RUNNING = 0, FULL_SCAN, PARTIAL_SCAN,
};

class AppScanner {
public:

    AppScanner();
    ~AppScanner();

    void SetAppDescFactory(AppDescriptionFactoryInterface& factory)
    {
        app_desc_factory_ = &factory;
    }
    void SetScanFilter(AppScanFilterInterface& scan_filter)
    {
        app_scan_filter_ = &scan_filter;
    }
    void SetBCP47Info(const std::string& lang, const std::string& script, const std::string& region);

    void AddDirectory(std::string path, AppTypeByDir type);
    void RemoveDirectory(std::string path, AppTypeByDir type);

    bool isRunning() const;
    void Run(ScanMode mode);
    AppDescPtr ScanForOneApp(const std::string& app_id);

    boost::signals2::signal<void(AppDescPtr, ScanMode)> signalAppDetected;
    boost::signals2::signal<void(ScanMode, const AppDescMaps&)> signalAppScanFinished;

private:
    struct AppDir {
        std::string path_;
        AppTypeByDir type_;
        bool is_scanned_;
        AppDir(const std::string& path, const AppTypeByDir& type) :
                path_(path), type_(type), is_scanned_(false)
        {
        }
    };

    void RunScanner();
    void ScanDir(std::string base_dir, AppTypeByDir type);
    AppDescPtr ScanApp(std::string& path, AppTypeByDir type);
    pbnjson::JValue LoadAppInfo(const std::string& app_dir_path, const AppTypeByDir& type_by_dir);
    void RegisterNewAppDesc(AppDescPtr new_desc, AppDescMaps& app_roster);

    AppDescriptionFactoryInterface* app_desc_factory_;
    AppScanFilterInterface* app_scan_filter_;
    ScanMode scan_mode_;
    std::vector<AppDir> target_dirs_;
    AppDescMaps app_desc_maps_;
    std::string language_;
    std::string script_;
    std::string region_;
};

#endif // CORE_PACKAGE_APP_SCANNER_H_
