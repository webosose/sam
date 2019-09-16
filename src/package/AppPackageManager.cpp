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

#include <bus/client/SettingService.h>
#include <bus/service/ApplicationManager.h>
#include <bus/client/Notification.h>
#include <lifecycle/LifecycleManager.h>
#include <lifecycle/RunningInfoManager.h>
#include <package/AppPackageManager.h>
#include <setting/Settings.h>
#include <algorithm>
#include <sys/types.h>
#include <util/LSUtils.h>
#include <util/JValueUtil.h>
#include <utime.h>

const std::string APP_CHANGE_ADDED = "added";
const std::string APP_CHANGE_REMOVED = "removed";
const std::string APP_CHANGE_UPDATED = "updated";

AppPackageManager::AppPackageManager() :
        m_isFirstFullScanStarted(false)
{
    m_appScanner.EventAppScanFinished.connect(boost::bind(&AppPackageManager::onAppScanFinished, this, _1, _2));
}

AppPackageManager::~AppPackageManager()
{
    clear();
}

void AppPackageManager::clear()
{
    m_appDescMaps.clear();
}

void AppPackageManager::initialize()
{
    SettingService::getInstance().EventLocaleChanged.connect(boost::bind(&AppPackageManager::onLocaleChanged, this, _1, _2, _3));
    AppInstallService::getInstance().EventStatusChanged.connect(boost::bind(&AppPackageManager::onPackageStatusChanged, this, _1, _2));

    // required apps scanning : before rw filesystem ready
    const BaseScanPaths& base_dirs = SettingsImpl::getInstance().getBaseAppDirs();
    for (auto& it : base_dirs) {
        m_appScanner.addDirectory(it.first, it.second);
    }
}

void AppPackageManager::startPostInit()
{
    // remaining apps scanning : after rw filesystem ready
    const BaseScanPaths& base_dirs = SettingsImpl::getInstance().getBaseAppDirs();
    for (auto& it : base_dirs) {
        m_appScanner.addDirectory(it.first, it.second);
    }

    scan();
}

void AppPackageManager::scanInitialApps()
{
    auto boot_time_apps = SettingsImpl::getInstance().getBootTimeApps();
    for (auto& appId : boot_time_apps) {
        AppPackagePtr app_desc = m_appScanner.scanForOneApp(appId);
        if (app_desc) {
            m_appDescMaps[appId] = app_desc;
        }
    }
}

void AppPackageManager::scan()
{
    if (!m_isFirstFullScanStarted) {
        Logger::info(getClassName(), __FUNCTION__, "first full scan");
        m_isFirstFullScanStarted = true;
    }
    m_appScanner.run(ScanMode::FULL_SCAN);
}

void AppPackageManager::rescan(const std::vector<std::string>& reason)
{
    for (const auto& r : reason) {
        auto it = std::find(m_scanReason.begin(), m_scanReason.end(), r);
        if (it != m_scanReason.end())
            continue;
        m_scanReason.push_back(r);
    }

    if (!m_isFirstFullScanStarted)
        return;

    Logger::info(getClassName(), __FUNCTION__, "rescan");
    scan();
}

void AppPackageManager::onAppInstalled(const std::string& appId)
{
    AppPackagePtr ptr = getAppById(appId);
    if (ptr != nullptr)
        ptr->unlock();
    AppPackagePtr AppDescPtr = m_appScanner.scanForOneApp(appId);
    if (!AppDescPtr) {
        return;
    }

    // skip if stub type, (stub apps will be loaded on next full scan)
    if (AppType::AppType_Stub == AppDescPtr->getAppType()) {
        return;
    }

    // disallow dev apps if current not in dev mode
    if (AppTypeByDir::AppTypeByDir_Dev == AppDescPtr->getTypeByDir() && SettingsImpl::getInstance().m_isDevMode == false) {
        return;
    }

    addApp(appId, AppDescPtr, AppStatusChangeEvent::AppStatusChangeEvent_Installed);

    // clear web app cache
    if (AppTypeByDir::AppTypeByDir_Dev == AppDescPtr->getTypeByDir() && AppType::AppType_Web == AppDescPtr->getAppType()) {
        LSCallOneReply(ApplicationManager::getInstance().get(),
                       "luna://com.palm.webappmanager/discardCodeCache",
                       "{\"force\":true}",
                       NULL,
                       NULL,
                       NULL,
                       NULL
        );
    }
}

void AppPackageManager::onAppUninstalled(const std::string& appId)
{
    removeApp(appId, true, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
}

const AppDescMaps& AppPackageManager::allApps()
{
    return m_appDescMaps;
}

AppPackagePtr AppPackageManager::getAppById(const std::string& appId)
{
    if (m_appDescMaps.count(appId) == 0)
        return NULL;
    return m_appDescMaps[appId];
}

void AppPackageManager::replaceAppDesc(const std::string& appId, AppPackagePtr new_desc)
{
    if (m_appDescMaps.count(appId) == 0) {
        m_appDescMaps[appId] = new_desc;
        return;
    }

    m_appDescMaps[appId] = new_desc;
}

void AppPackageManager::addApp(const std::string& appId, AppPackagePtr newAppdescPtr, AppStatusChangeEvent event)
{
    AppPackagePtr appDescPtr = getAppById(appId);

    // new app
    if (!appDescPtr) {
        m_appDescMaps[appId] = newAppdescPtr;
        publishOneAppChange(newAppdescPtr, APP_CHANGE_ADDED, event);
        return;
    }

    if (AppStatusChangeEvent::STORAGE_ATTACHED == event &&
        appDescPtr->isHigherVersionThanMe(appDescPtr, newAppdescPtr) == false) {
        return;
    }

    (void) replaceAppDesc(appId, newAppdescPtr);

    if (AppStatusChangeEvent::AppStatusChangeEvent_Installed == event)
        event = AppStatusChangeEvent::UPDATE_COMPLETED;

    publishOneAppChange(newAppdescPtr, APP_CHANGE_UPDATED, event);
}

void AppPackageManager::removeApp(const std::string& appId, bool rescan, AppStatusChangeEvent event)
{
    AppPackagePtr appPackagePtr = getAppById(appId);
    if (!appPackagePtr)
        return;

    appPackagePtr->lock();
    if (appPackagePtr->isBuiltinBasedApp() && AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled == event) {
        Logger::info(getClassName(), __FUNCTION__, appId, "remove system-app in read-write area");
        SettingsImpl::getInstance().setSystemAppAsRemoved(appId);
    }

    // just remove current app without rescan
    if (!rescan) {
        publishOneAppChange(appPackagePtr, APP_CHANGE_REMOVED, event);
        m_appDescMaps.erase(appId);
        RunningInfoManager::getInstance().removeRunningInfo(appId);
        return;
    }

    // rescan file system
    AppPackagePtr rescanned_desc = m_appScanner.scanForOneApp(appId);
    if (!rescanned_desc) {
        publishOneAppChange(appPackagePtr, APP_CHANGE_REMOVED, event);
        m_appDescMaps.erase(appId);
        RunningInfoManager::getInstance().removeRunningInfo(appId);
        return;
    }

    // compare current app and rescanned app
    // if same
    if (appPackagePtr->isSame(appPackagePtr, rescanned_desc)) {
        // (still remains in file system)
        return;
        // if not same
    } else {
        replaceAppDesc(appId, rescanned_desc);
        publishOneAppChange(rescanned_desc, APP_CHANGE_UPDATED, event);
    }

    Logger::info(getClassName(), __FUNCTION__, appId, Logger::format("clear_memory: event: %d", event));
}

void AppPackageManager::reloadApp(const std::string& appId)
{
    AppPackagePtr oldAppDescPtr = getAppById(appId);
    if (!oldAppDescPtr) {
        Logger::info(getClassName(), __FUNCTION__, appId, "reload: no current description, just skip");
        return;
    }

    oldAppDescPtr->lock();
    AppPackagePtr newAppDescPtr = m_appScanner.scanForOneApp(appId);
    if (!newAppDescPtr) {
        publishOneAppChange(oldAppDescPtr, APP_CHANGE_REMOVED, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
        m_appDescMaps.erase(appId);
        RunningInfoManager::getInstance().removeRunningInfo(appId);
        Logger::info(getClassName(), __FUNCTION__, appId, "reload: no app package left, just remove");
        return;
    }

    if (AppPackage::isSame(oldAppDescPtr, newAppDescPtr)) {
        Logger::info(getClassName(), __FUNCTION__, appId, "reload: no change, just skip");
        return;
    } else {
        replaceAppDesc(appId, newAppDescPtr);
        publishOneAppChange(newAppDescPtr, APP_CHANGE_UPDATED, AppStatusChangeEvent::UPDATE_COMPLETED);
        Logger::info(getClassName(), __FUNCTION__, appId, "reload: different package detected, update info now");
    }
}

bool AppPackageManager::lockAppForUpdate(const std::string& appId, bool lock, std::string& errorText)
{
    AppPackagePtr desc = getAppById(appId);
    if (!desc) {
        errorText = appId + " was not found OR Unsupported Application Type";
        return false;
    }

    Logger::info(getClassName(), __FUNCTION__, appId, Logger::format("lock(%B)", lock));
    desc->lock();
    return true;
}

void AppPackageManager::onLocaleChanged(const std::string& lang, const std::string& region, const std::string& script)
{
    m_appScanner.setBCP47Info(lang, region, script);
    rescan( { "LANG" });
}

void AppPackageManager::onAppScanFinished(ScanMode mode, const AppDescMaps& scanned_apps)
{
    if (ScanMode::FULL_SCAN == mode) {

        // check changes after rescan
        for (const auto& app : scanned_apps) {
            if (m_appDescMaps.count(app.first) == 0) {
                Logger::info(getClassName(), __FUNCTION__, app.first, "added_after_full_scan");
                EventAppStatusChanged(AppStatusChangeEvent::AppStatusChangeEvent_Installed, app.second);
            }
        }
        std::vector<std::string> removed_apps;
        for (const auto& app : m_appDescMaps) {
            if (scanned_apps.count(app.first) == 0) {
                Logger::info(getClassName(), __FUNCTION__, app.first, "removed_after_full_scan");
                removed_apps.push_back(app.first);
                EventAppStatusChanged(AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled, app.second);
            }
        }

        // close removed apps if it's running
        if (!removed_apps.empty())
            LifecycleManager::getInstance().closeApps(removed_apps, true);

        clear();
        m_appDescMaps = scanned_apps;
        publishListApps();
        m_scanReason.clear();
    } else if (ScanMode::PARTIAL_SCAN == mode) {

        for (auto& it : scanned_apps) {
            const std::string appId = it.first;
            AppPackagePtr new_desc = it.second;
            addApp(appId, new_desc, AppStatusChangeEvent::STORAGE_ATTACHED);
        }
    }
}

bool AppPackageManager::onBatch(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = pbnjson::JDomParser::fromString(LSMessageGetPayload(message));
    bool returnValue = false;
    string errorText;

    bool isParentalControlValid = false;
    bool parentalControl = false;
    bool isApplockPerAppValid = false;
    bool applockPerApp = false;

    JValueUtil::getValue(responsePayload, "returnValue", returnValue);
    JValueUtil::getValue(responsePayload, "errorText", errorText);

    if (!responsePayload.hasKey("results") || !responsePayload["results"].isArray()) {
        Logger::debug(getInstance().getClassName(), __FUNCTION__, Logger::format("result fail: %s", responsePayload.stringify().c_str()));
        goto Done;
    }

    for (int i = 0; i < responsePayload["results"].arraySize(); ++i) {
        if (!responsePayload["results"][i].hasKey("settings") || !responsePayload["results"][i]["settings"].isObject())
            continue;

        if (responsePayload["results"][i]["settings"].hasKey("parentalControl") && responsePayload["results"][i]["settings"]["parentalControl"].isBoolean()) {
            parentalControl = responsePayload["results"][i]["settings"]["parentalControl"].asBool();
            isParentalControlValid = true;
        }

        if (responsePayload["results"][i]["settings"].hasKey("applockPerApp") && responsePayload["results"][i]["settings"]["applockPerApp"].isBoolean()) {
            applockPerApp = responsePayload["results"][i]["settings"]["applockPerApp"].asBool();
            isApplockPerAppValid = true;
        }
    }

    if (!isParentalControlValid || !isApplockPerAppValid) {
        Logger::debug("CheckAppLockStatus", __FUNCTION__, Logger::format("receiving valid result fail: %s", responsePayload.stringify().c_str()));
        goto Done;
    }

    if (parentalControl && applockPerApp) {
        Notification::getInstance().createPincodePrompt(onCreatePincodePrompt);
        return true;
    }

Done:
    // TODO: appId should be passed
    // AppPackageManager::getInstance().removeApp(appId, false, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
    return true;
}

bool AppPackageManager::onCreatePincodePrompt(LSHandle* sh, LSMessage* message, void* context)
{
    pbnjson::JValue responsePayload = pbnjson::JDomParser::fromString(LSMessageGetPayload(message));
    bool returnValue = false;
    bool matched = false;

    if (!JValueUtil::getValue(responsePayload, "returnValue", returnValue) || !JValueUtil::getValue(responsePayload, "matched", matched)) {
        Logger::error(getInstance().getClassName(), __FUNCTION__, "Failed to get required params");
        return true;
    }


    if (matched == false) {
        Logger::info(AppPackageManager::getInstance().getClassName(), __FUNCTION__, "uninstallation is canceled because of invalid pincode");
    } else {
        // TODO: appId should be passed
        // AppPackageManager::getInstance().removeApp(appId, false, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
    }
    return true;
}

void AppPackageManager::uninstallApp(const std::string& appId, std::string& errorText)
{
    AppPackagePtr appDesc = getAppById(appId);
    if (!appDesc)
        return;

    if (!appDesc->isRemovable()) {
        errorText = appId + " is not removable";
        return;
    }

    Logger::info(getClassName(), __FUNCTION__, appId, "start_uninstall_app");
    if (AppTypeByDir::AppTypeByDir_System_BuiltIn == appDesc->getTypeByDir()) {

        if (!appDesc->isVisible()) {
            removeApp(appId, false, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
        } else {
            Logger::debug(getClassName(), __FUNCTION__, appId, "check parental lock to remove app");
            Call call = SettingService::getInstance().batch(onBatch, appId);
        }
    } else {
        pbnjson::JValue requestPayload = pbnjson::Object();
        requestPayload.put("id", appId);
        requestPayload.put("subscribe", false);

        LSErrorSafe lserror;

        if (!LSCallOneReply(ApplicationManager::getInstance().get(),
                            "luna://com.webos.appInstallService/remove",
                            requestPayload.stringify().c_str(),
                            onAppUninstalled,
                            NULL,
                            NULL,
                            &lserror)) {
            errorText = lserror.message;
            return;
        }

        Logger::info(getClassName(), __FUNCTION__, appId, "requested_to_appinstalld");
    }
}

bool AppPackageManager::onAppUninstalled(LSHandle* sh, LSMessage *message, void* context)
{
    pbnjson::JValue responsePayload = JDomParser::fromString(LSMessageGetPayload(message));

    if (responsePayload.isNull()) {
        Logger::error(AppPackageManager::getInstance().getClassName(), __FUNCTION__, "Null response");
    }

    Logger::info(AppPackageManager::getInstance().getClassName(), __FUNCTION__, responsePayload.stringify());

    return true;
}

void AppPackageManager::onPackageStatusChanged(const std::string& appId, const PackageStatus& status)
{
    if (PackageStatus::Installed == status)
        onAppInstalled(appId);
    else if (PackageStatus::InstallFailed == status)
        reloadApp(appId);
    else if (PackageStatus::Uninstalled == status)
        onAppUninstalled(appId);
    else if (PackageStatus::UninstallFailed == status)
        reloadApp(appId);
    else if (PackageStatus::ResetToDefault == status)
        reloadApp(appId);
}

std::string AppPackageManager::toString(const AppStatusChangeEvent& event)
{
    switch (event) {
    case AppStatusChangeEvent::AppStatusChangeEvent_Nothing:
        return "nothing";
        break;
    case AppStatusChangeEvent::AppStatusChangeEvent_Installed:
        return "appInstalled";
        break;
    case AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled:
        return "appUninstalled";
        break;
    case AppStatusChangeEvent::STORAGE_ATTACHED:
        return "storageAttached";
        break;
    case AppStatusChangeEvent::STORAGE_DETACHED:
        return "storageDetached";
        break;
    case AppStatusChangeEvent::UPDATE_COMPLETED:
        return "updateCompleted";
        break;
    default:
        return "unknown";
        break;
    }
}

void AppPackageManager::publishListApps()
{
    pbnjson::JValue apps = pbnjson::Array();
    pbnjson::JValue dev_apps = pbnjson::Array();

    for (auto it : m_appDescMaps) {
        apps.append(it.second->toJValue());

        if (SettingsImpl::getInstance().m_isDevMode && AppTypeByDir::AppTypeByDir_Dev == it.second->getTypeByDir())
            dev_apps.append(it.second->toJValue());
    }

    EventListAppsChanged(apps, m_scanReason, false);

    if (SettingsImpl::getInstance().m_isDevMode)
        EventListAppsChanged(dev_apps, m_scanReason, true);
}

void AppPackageManager::publishOneAppChange(AppPackagePtr app_desc, const std::string& change, AppStatusChangeEvent event)
{
    pbnjson::JValue app = app_desc->toJValue();
    std::string reason = (event != AppStatusChangeEvent::AppStatusChangeEvent_Nothing) ? toString(event) : "";

    EventOneAppChanged(app, change, reason, false);
    if (SettingsImpl::getInstance().m_isDevMode && AppTypeByDir::AppTypeByDir_Dev == app_desc->getTypeByDir())
        EventOneAppChanged(app, change, reason, true);

    EventAppStatusChanged(event, app_desc);
}
