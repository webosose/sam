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

#include "MainDaemon.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory>
#include <glib.h>
#include <boost/bind.hpp>

#include "base/AppDescriptionList.h"
#include "bus/client/AppInstallService.h"
#include "bus/client/Bootd.h"
#include "bus/client/Configd.h"
#include "bus/client/DB8.h"
#include "bus/client/LSM.h"
#include "bus/client/SettingService.h"
#include "bus/client/WAM.h"
#include "bus/service/ApplicationManager.h"
#include "manager/LifecycleManager.h"
#include "setting/Settings.h"
#include "util/File.h"
#include "util/JValueUtil.h"


MainDaemon::MainDaemon()
    : m_isCBDGenerated(false),
      m_isConfigsReceived(false)
{
    setClassName("MainDaemon");

    m_mainLoop = g_main_loop_new(NULL, FALSE);

    // Load managers (lifecycle, package, launchpoint)
    LifecycleManager::getInstance().initialize();
    SettingService::getInstance().initialize();

    // Client
    Configd::getInstance().initialize();
    Bootd::getInstance().initialize();

    LSM::getInstance().initialize();
    AppInstallService::getInstance().initialize();

    Bootd::getInstance().EventGetBootStatus.connect(boost::bind(&MainDaemon::onGetBootStatus, this, _1));
    Configd::getInstance().EventGetConfigs.connect(boost::bind(&MainDaemon::onGetConfigs, this, _1));
}

MainDaemon::~MainDaemon()
{
    ApplicationManager::getInstance().detach();

    if (m_mainLoop) {
        g_main_loop_unref(m_mainLoop);
    }
}

void MainDaemon::start()
{
    Logger::info(getClassName(), __FUNCTION__, "Start event handler thread");
    g_main_loop_run(m_mainLoop);
}

void MainDaemon::stop()
{
    if (m_mainLoop)
        g_main_loop_quit(m_mainLoop);
}

void MainDaemon::onGetBootStatus(const JValue& subscriptionPayload)
{
    bool coreBootDone;
    if (!JValueUtil::getValue(subscriptionPayload, "signals", "core-boot-done", coreBootDone) && !coreBootDone) {
        return;
    }
    m_isCBDGenerated = true;
    checkPreconditions();
}

void MainDaemon::onGetConfigs(const JValue& responsePayload)
{
    JValue sysAssetFallbackPrecedence;
    JValue keepAliveApps;
    JValue lifeCycle;
    bool supportQmlBooster;

    if (JValueUtil::getValue(responsePayload, "configs", "system.sysAssetFallbackPrecedence", sysAssetFallbackPrecedence) && sysAssetFallbackPrecedence.isArray()) {
        SettingsImpl::getInstance().setSysAssetFallbackPrecedence(sysAssetFallbackPrecedence);
    }
    if (JValueUtil::getValue(responsePayload, "configs", "com.webos.applicationManager.keepAliveApps", keepAliveApps) && keepAliveApps.isArray()) {
        SettingsImpl::getInstance().setKeepAliveApps(keepAliveApps);
    }
    if (JValueUtil::getValue(responsePayload, "configs", "com.webos.applicationManager.supportQmlBooster", supportQmlBooster)) {
        SettingsImpl::getInstance().setSupportQMLBooster(supportQmlBooster);
    }
    m_isConfigsReceived = true;
    checkPreconditions();
}

void MainDaemon::checkPreconditions()
{
    if (!m_isConfigsReceived) {
        Logger::info(getClassName(), __FUNCTION__, "Cannot receive 'getConfigs' response");
        return;
    }
    if (!m_isCBDGenerated) {
        Logger::info(getClassName(), __FUNCTION__, "Cannot receive 'getBootStatus' response");
        return;
    }
    Logger::info(getClassName(), __FUNCTION__, "All initial components are ready");

    scan();
    DB8::getInstance().initialize();
    WAM::getInstance().initialize();

    ApplicationManager::getInstance().attach(m_mainLoop);
    ApplicationManager::getInstance().postListApps("", "initialized", "");
    // TODO : listLaunchPoint에 대한 subscription을 보내야 하는데
    // DB에서 읽어올 때까지 딜레이를 시켜야 하나? 아래는 관련코드
    ApplicationManager::getInstance().postListLaunchPoints(nullptr);

    // TODO : 아래는 자동으로 LP가 생성이 되어야지 명시적으로 생성하도록 하면 안좋다.
    // LaunchPointManager::getInstance().updateApps(apps);
}

void MainDaemon::scan()
{
    const map<std::string, AppLocation>& appDirs = Settings::getInstance().getAppDirs();

    for (auto& appDir : appDirs) {
        if (!File::isDirectory(appDir.first)) {
            Logger::info(getClassName(), __FUNCTION__, appDir.first, "Directory is not exist");
            continue;
        }
        if (appDir.second == AppLocation::AppLocation_Devmode && !SettingsImpl::getInstance().isDevmodeEnabled()) {
            Logger::info(getClassName(), __FUNCTION__, appDir.first, "Not in devmoe. Skipped");
            continue;
        }

        dirent** entries = NULL;
        int appCount = ::scandir(appDir.first.c_str(), &entries, 0, alphasort);
        if (entries == NULL || appCount == 0) {
            Logger::info(getClassName(), __FUNCTION__, appDir.first, "Directory is empty");
            continue;
        }

        for (int i = 0; i < appCount; ++i) {
            if (!entries[i] || entries[i]->d_name[0] == '.')
                continue;
            if (appDir.second == AppLocation::AppLocation_System_ReadOnly &&
                SettingsImpl::getInstance().isDeletedSystemApp(entries[i]->d_name)) {
                Logger::info(getClassName(), __FUNCTION__, entries[i]->d_name, "The app is in deleted system apps.");
                continue;
            }

            string appFullPath = File::join(appDir.first, entries[i]->d_name);
            if (!File::isDirectory(appFullPath)) {
                Logger::info(getClassName(), __FUNCTION__, entries[i]->d_name, "Application directory is not exist");
                continue;
            }

            AppDescriptionPtr appDesc = AppDescriptionList::getInstance().create(entries[i]->d_name);
            if (!appDesc) {
                Logger::warning(getClassName(), __FUNCTION__, entries[i]->d_name, "Cannot create application description");
                continue;
            }
            appDesc->setAppLocation(appDir.second);
            appDesc->setFolderPath(appFullPath);
            appDesc->loadAppinfo();
            AppDescriptionList::getInstance().add(appDesc);
        }

        if (entries) {
            for (int idx = 0; idx < appCount; ++idx) {
                if (entries[idx])
                    free(entries[idx]);
            }
            free(entries);
            entries = NULL;
        }
    }
}
