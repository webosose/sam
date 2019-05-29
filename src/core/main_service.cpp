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

#include "core/main_service.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <memory>

#include <glib.h>
#include <boost/bind.hpp>

#include "core/base/logging.h"
#include "core/base/prerequisite_monitor.h"
#include "core/bus/appmgr_service.h"
#include "core/bus/sysmgr_service.h"
#include "core/launch_point/launch_point_manager.h"
#include "core/lifecycle/app_life_manager.h"
#include "core/module/locale_preferences.h"
#include "core/module/service_observer.h"
#include "core/module/subscriber_of_bootd.h"
#include "core/module/subscriber_of_configd.h"
#include "core/module/subscriber_of_lsm.h"
#include "core/module/subscriber_of_appinstalld.h"
#include "core/package/application_manager.h"
#include "core/product/product_abstract_factory.h"
#include "core/setting/settings.h"

namespace CorePrerequisites {

const std::string CONFIGD_KEY_FALLBACK_PRECEDENCE = "system.sysAssetFallbackPrecedence";
const std::string CONFIGD_KEY_KEEPALIVE_APPS = "com.webos.applicationManager.keepAliveApps";
const std::string CONFIGD_KEY_SUPPORT_QML_BOOSTER = "com.webos.applicationManager.supportQmlBooster";
const std::string CONFIGD_KEY_LIFECYCLE_REASON = "com.webos.applicationManager.lifeCycle";

class ConfigInfo: public PrerequisiteItem {
public:
    ConfigInfo()
    {
    }
    virtual ~ConfigInfo()
    {
        config_info_connection_.disconnect();
    }

    virtual void Start()
    {
        ConfigdSubscriber::instance().AddRequiredKey(CONFIGD_KEY_FALLBACK_PRECEDENCE);
        ConfigdSubscriber::instance().AddRequiredKey(CONFIGD_KEY_KEEPALIVE_APPS);
        ConfigdSubscriber::instance().AddRequiredKey(CONFIGD_KEY_SUPPORT_QML_BOOSTER);
        ConfigdSubscriber::instance().AddRequiredKey(CONFIGD_KEY_LIFECYCLE_REASON);
        config_info_connection_ = ConfigdSubscriber::instance().SubscribeConfigInfo(boost::bind(&CorePrerequisites::ConfigInfo::HandleStatus, this, _1));
    }

    void HandleStatus(const pbnjson::JValue& jmsg)
    {
        LOG_INFO(MSGID_SAM_LOADING_SEQ, 1,
                 PMLOGKS("status", "received_configd_msg"),
                 "%s", jmsg.duplicate().stringify().c_str());

        if (!jmsg.hasKey("configs") || !jmsg["configs"].isObject()) {
            LOG_WARNING(MSGID_INTERNAL_ERROR, 2, PMLOGKS("func", __FUNCTION__), PMLOGKFV("line", "%d", __LINE__), "");
        }

        if (jmsg["configs"].hasKey(CONFIGD_KEY_FALLBACK_PRECEDENCE) && jmsg["configs"][CONFIGD_KEY_FALLBACK_PRECEDENCE].isArray())
            SettingsImpl::instance().setAssetFallbackKeys(jmsg["configs"][CONFIGD_KEY_FALLBACK_PRECEDENCE]);

        if (jmsg["configs"].hasKey(CONFIGD_KEY_KEEPALIVE_APPS) && jmsg["configs"][CONFIGD_KEY_KEEPALIVE_APPS].isArray())
            SettingsImpl::instance().setKeepAliveApps(jmsg["configs"][CONFIGD_KEY_KEEPALIVE_APPS]);

        if (jmsg["configs"].hasKey(CONFIGD_KEY_SUPPORT_QML_BOOSTER) && jmsg["configs"][CONFIGD_KEY_SUPPORT_QML_BOOSTER].isBoolean())
            SettingsImpl::instance().set_support_qml_booster(jmsg["configs"][CONFIGD_KEY_SUPPORT_QML_BOOSTER].asBool());

        if (jmsg["configs"].hasKey(CONFIGD_KEY_LIFECYCLE_REASON) && jmsg["configs"][CONFIGD_KEY_LIFECYCLE_REASON].isObject())
            SettingsImpl::instance().SetLifeCycleReason(jmsg["configs"][CONFIGD_KEY_LIFECYCLE_REASON]);

        if (jmsg.hasKey("missingConfigs") && jmsg["missingConfigs"].isArray() && jmsg["missingConfigs"].arraySize() > 0) {
            LOG_WARNING(MSGID_INTERNAL_ERROR, 2,
                        PMLOGKS("status", "missing_core_config_info"),
                        PMLOGJSON("missed_infos", jmsg["missingConfigs"].stringify().c_str()), "");
        }

        config_info_connection_.disconnect();
        SetStatus(PrerequisiteItemStatus::Passed);
    }

private:
    boost::signals2::connection config_info_connection_;
};

class BootStatus: public PrerequisiteItem {
public:
    BootStatus()
    {
    }
    virtual ~BootStatus()
    {
        boot_status_connection_.disconnect();
    }

    virtual void Start()
    {
        boot_status_connection_ = BootdSubscriber::instance().SubscribeBootStatus(boost::bind(&CorePrerequisites::BootStatus::HandleStatus, this, _1));
    }

    void HandleStatus(const pbnjson::JValue& jmsg)
    {
        LOG_INFO(MSGID_BOOTSTATUS_RECEIVED, 1, PMLOGJSON("Payload", jmsg.duplicate().stringify().c_str()), "");

        if (!jmsg.hasKey("signals") || !jmsg["signals"].isObject() || !jmsg["signals"].hasKey("core-boot-done") || !jmsg["signals"]["core-boot-done"].asBool())
            return;

        boot_status_connection_.disconnect();
        SetStatus(PrerequisiteItemStatus::Passed);
    }
private:
    boost::signals2::connection boot_status_connection_;
};

static void OnReady(PrerequisiteResult result)
{
    if (AppMgrService::instance().IsServiceReady())
        return;

    LOG_INFO(MSGID_SAM_LOADING_SEQ, 1, PMLOGKS("status", "all_precondition_ready"), "");

    SettingsImpl::instance().onRestLoad();
    LocalePreferences::instance().OnRestInit();
    ProductAbstractFactory::instance().OnReady();
    ApplicationManager::instance().StartPostInit();

    AppMgrService::instance().SetServiceStatus(true);
    AppMgrService::instance().OnServiceReady();
}

}   // namespace CorePrerequisites

////////////////////////////////////////////////////////////////////
// Main Service
////////////////////////////////////////////////////////////////////
MainService::~MainService()
{
}

bool MainService::initialize()
{
    // Load Settings (first!)
    SettingsImpl::instance().load(NULL);    //load default setting file

    // Load managers (lifecycle, package, launchpoint)
    ApplicationManager::instance().Init();
    AppLifeManager::instance().init();
    LaunchPointManager::instance().Init();
    LocalePreferences::instance().Init();   //load locale info

    // service attach
    SysMgrService::instance()->Attach(main_loop());
    AppMgrService::instance().Attach(main_loop());

    ConfigdSubscriber::instance().Init();
    BootdSubscriber::instance().Init();
    LSMSubscriber::instance().init();
    AppinstalldSubscriber::instance().Init();

    PrerequisiteMonitor& service_prerequisite_monitor = PrerequisiteMonitor::Create(CorePrerequisites::OnReady);

    PrerequisiteItemPtr core_ready_condition1 = std::make_shared<CorePrerequisites::ConfigInfo>();
    PrerequisiteItemPtr core_ready_condition2 = std::make_shared<CorePrerequisites::BootStatus>();
    service_prerequisite_monitor.AddItem(core_ready_condition1);
    service_prerequisite_monitor.AddItem(core_ready_condition2);

    // TODO: reorgarnize all instances according to their characteristics (core/extension)
    //       then, put product recipes codes into each extension main
    // Abstract Factory for product recipes
    ProductAbstractFactory::instance().Initialize(service_prerequisite_monitor);

    service_prerequisite_monitor.Run();
    ServiceObserver::instance().Run();

    ApplicationManager::instance().ScanInitialApps();

    return true;
}

bool MainService::terminate()
{
    ServiceObserver::instance().Stop();

    SysMgrService::instance()->Detach();
    AppMgrService::instance().Detach();

    ProductAbstractFactory::instance().Terminate();

    SingletonNS::destroyAll();
    return true;
}
