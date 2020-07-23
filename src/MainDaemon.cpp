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
#include "bus/client/MemoryManager.h"
#include "bus/client/NativeContainer.h"
#include "bus/client/Notification.h"
#include "bus/client/SettingService.h"
#include "bus/client/WAM.h"
#include "bus/service/ApplicationManager.h"
#include "conf/RuntimeInfo.h"
#include "conf/SAMConf.h"
#include "util/File.h"
#include "util/JValueUtil.h"


MainDaemon::MainDaemon()
    : m_isCBDGenerated(false),
      m_isConfigsReceived(false)
{
    setClassName("MainDaemon");
    m_mainLoop = g_main_loop_new(NULL, FALSE);
}

MainDaemon::~MainDaemon()
{
    if (m_mainLoop) {
        g_main_loop_unref(m_mainLoop);
    }
}

void MainDaemon::initialize()
{
    SAMConf::getInstance().initialize();
    RuntimeInfo::getInstance().initialize();
    AppDescriptionList::getInstance().scanFull();

    if (!ApplicationManager::getInstance().attach(m_mainLoop))
        return;

    AppInstallService::getInstance().initialize();
    Bootd::getInstance().initialize();
    Configd::getInstance().initialize();
    DB8::getInstance().initialize();
    LSM::getInstance().initialize();
    MemoryManager::getInstance().initialize();
    NativeContainer::getInstance().initialize();
    Notification::getInstance().initialize();
    SettingService::getInstance().initialize();
    WAM::getInstance().initialize();

    Bootd::getInstance().EventGetBootStatus.connect(boost::bind(&MainDaemon::onGetBootStatus, this, boost::placeholders::_1));
    Configd::getInstance().EventGetConfigs.connect(boost::bind(&MainDaemon::onGetConfigs, this, boost::placeholders::_1));
}

void MainDaemon::finalize()
{
    AppInstallService::getInstance().finalize();
    Bootd::getInstance().finalize();
    Configd::getInstance().finalize();
    DB8::getInstance().finalize();
    LSM::getInstance().finalize();
    MemoryManager::getInstance().finalize();
    Notification::getInstance().finalize();
    SettingService::getInstance().finalize();
    WAM::getInstance().finalize();

    ApplicationManager::getInstance().detach();
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

    if (JValueUtil::getValue(responsePayload, "configs", "system.sysAssetFallbackPrecedence", sysAssetFallbackPrecedence) && sysAssetFallbackPrecedence.isArray()) {
        SAMConf::getInstance().setSysAssetFallbackPrecedence(sysAssetFallbackPrecedence);
    }
    if (JValueUtil::getValue(responsePayload, "configs", "com.webos.applicationManager.keepAliveApps", keepAliveApps) && keepAliveApps.isArray()) {
        SAMConf::getInstance().setKeepAliveApps(keepAliveApps);
    }
    m_isConfigsReceived = true;
    checkPreconditions();
}

void MainDaemon::checkPreconditions()
{
    static bool isFired = false;
    if (isFired)
        return;

    if (!m_isConfigsReceived) {
        Logger::info(getClassName(), __FUNCTION__, "Wait for receiving 'getConfigs' response");
        return;
    }
    if (!m_isCBDGenerated) {
        Logger::info(getClassName(), __FUNCTION__, "Wait for receiving 'getBootStatus' response");
        return;
    }
    Logger::info(getClassName(), __FUNCTION__, "All initial components are ready");
    isFired = true;

    ApplicationManager::getInstance().enablePosting();
}

