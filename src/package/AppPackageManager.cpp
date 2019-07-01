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

#include <bus/client/SettingService.h>
#include <bus/service/ApplicationManager.h>
#include <lifecycle/LifecycleManager.h>
#include <lifecycle/RunningInfoManager.h>
#include <package/AppPackageManager.h>
#include <setting/Settings.h>
#include <algorithm>
#include <sys/types.h>
#include <util/CallChain.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/LSUtils.h>
#include <util/Utils.h>
#include <utime.h>


const std::string APP_CHANGE_ADDED = "added";
const std::string APP_CHANGE_REMOVED = "removed";
const std::string APP_CHANGE_UPDATED = "updated";

namespace CallChainEventHandler {
class CheckAppLockStatus: public LSCallItem {
public:
    CheckAppLockStatus(const std::string& appId) :
            LSCallItem(ApplicationManager::getInstance().get(), "luna://com.webos.settingsservice/batch", "{}")
    {
        std::string query = std::string(R"({"operations":[
          {"method":"getSystemSettings","params":{"category":"lock","key":"parentalControl"}},
          {"method":"getSystemSettings","params":{
            "category":"lock","key":"applockPerApp",LOG_KEY_APPID:")")
                        + appId + std::string(R"("}}
        ]})");

        setPayload(query.c_str());
    }

protected:
    virtual bool onReceiveCall(pbnjson::JValue jmsg)
    {
        bool received_valid_parental_mode_info = false;
        bool received_valid_lock_info = false;
        bool parental_control_mode_on = true;
        bool app_locked = true;

        if (!jmsg.hasKey("results") || !jmsg["results"].isArray()) {
            LOG_DEBUG("result fail: %s", jmsg.stringify().c_str());
            setError(PARENTAL_ERR_GET_SETTINGVAL, "invalid return message");
            return false;
        }

        for (int i = 0; i < jmsg["results"].arraySize(); ++i) {
            if (!jmsg["results"][i].hasKey("settings") || !jmsg["results"][i]["settings"].isObject())
                continue;

            if (jmsg["results"][i]["settings"].hasKey("parentalControl") && jmsg["results"][i]["settings"]["parentalControl"].isBoolean()) {
                parental_control_mode_on = jmsg["results"][i]["settings"]["parentalControl"].asBool();
                received_valid_parental_mode_info = true;
            }

            if (jmsg["results"][i]["settings"].hasKey("applockPerApp") && jmsg["results"][i]["settings"]["applockPerApp"].isBoolean()) {
                app_locked = jmsg["results"][i]["settings"]["applockPerApp"].asBool();
                received_valid_lock_info = true;
            }
        }

        if (!received_valid_parental_mode_info || !received_valid_lock_info) {
            LOG_DEBUG("receiving valid result fail: %s", jmsg.stringify().c_str());
            setError(PARENTAL_ERR_GET_SETTINGVAL, "not found valid lock info");
            return false;
        }

        if (parental_control_mode_on && app_locked)
            return false;

        return true;
    }
};

class CheckPin: public LSCallItem {
public:
    CheckPin()
        : LSCallItem(ApplicationManager::getInstance().get(), "luna://com.webos.notification/createPincodePrompt", R"({"promptType":"parental"})")
    {
    }
protected:
    virtual bool onReceiveCall(pbnjson::JValue jmsg)
    {
        //{"returnValue": true, "matched" : true/false }
        bool matched = false;
        if (!jmsg.hasKey("matched") ||
            !jmsg["matched"].isBoolean() ||
            jmsg["matched"].asBool(matched) != CONV_OK ||
            matched == false) {
            LOG_DEBUG("Pincode is not matched: %s", jmsg.stringify().c_str());
            setError(PARENTAL_ERR_UNMATCH_PINCODE, "pincode is not matched");
            return false;
        }

        return true;
    }
};
}

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
    for (auto& app_id : boot_time_apps) {
        AppPackagePtr app_desc = m_appScanner.scanForOneApp(app_id);
        if (app_desc) {
            m_appDescMaps[app_id] = app_desc;
        }
    }
}

void AppPackageManager::scan()
{
    if (!m_isFirstFullScanStarted) {
        LOG_INFO(MSGID_START_SCAN, 1,
                 PMLOGKS("STATUS", "FIRST_FULL_SCAN"), "");
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

    LOG_INFO(MSGID_START_SCAN, 1,
             PMLOGKS("STATUS", "RESCAN"), "");
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

void AppPackageManager::onAppUninstalled(const std::string& app_id)
{
    removeApp(app_id, true, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
}

const AppDescMaps& AppPackageManager::allApps()
{
    return m_appDescMaps;
}

AppPackagePtr AppPackageManager::getAppById(const std::string& app_id)
{
    if (m_appDescMaps.count(app_id) == 0)
        return NULL;
    return m_appDescMaps[app_id];
}

void AppPackageManager::replaceAppDesc(const std::string& app_id, AppPackagePtr new_desc)
{
    if (m_appDescMaps.count(app_id) == 0) {
        m_appDescMaps[app_id] = new_desc;
        return;
    }

    m_appDescMaps[app_id] = new_desc;
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
        LOG_INFO(MSGID_UNINSTALL_APP, 3,
                 PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                 PMLOGKS("app_type", "system"),
                 PMLOGKS("location", "rw"),
                 "remove system-app in read-write area");
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

    LOG_INFO(MSGID_UNINSTALL_APP, 2,
             PMLOGKS(LOG_KEY_APPID, appId.c_str()),
             PMLOGKS("status", "clear_memory"), "event: %d", (int ) event);
}

void AppPackageManager::reloadApp(const std::string& appId)
{
    AppPackagePtr oldAppDescPtr = getAppById(appId);
    if (!oldAppDescPtr) {
        LOG_INFO(MSGID_PACKAGE_STATUS, 2,
                 PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                 PMLOGKS(LOG_KEY_ACTION, "reload"), "no current description, just skip");
        return;
    }

    oldAppDescPtr->lock();
    AppPackagePtr newAppDescPtr = m_appScanner.scanForOneApp(appId);
    if (!newAppDescPtr) {
        publishOneAppChange(oldAppDescPtr, APP_CHANGE_REMOVED, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
        m_appDescMaps.erase(appId);
        RunningInfoManager::getInstance().removeRunningInfo(appId);
        LOG_INFO(MSGID_PACKAGE_STATUS, 2,
                 PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                 PMLOGKS(LOG_KEY_ACTION, "reload"), "no app package left, just remove");
        return;
    }

    if (AppPackage::isSame(oldAppDescPtr, newAppDescPtr)) {
        LOG_INFO(MSGID_PACKAGE_STATUS, 2,
                 PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                 PMLOGKS(LOG_KEY_ACTION, "reload"),
                 "no change, just skip");
        return;
    } else {
        replaceAppDesc(appId, newAppDescPtr);
        publishOneAppChange(newAppDescPtr, APP_CHANGE_UPDATED, AppStatusChangeEvent::UPDATE_COMPLETED);
        LOG_INFO(MSGID_PACKAGE_STATUS, 2,
                 PMLOGKS(LOG_KEY_APPID, appId.c_str()),
                 PMLOGKS(LOG_KEY_ACTION, "reload"), "different package detected, update info now");
    }
}

bool AppPackageManager::lockAppForUpdate(const std::string& appId, bool lock, std::string& errorText)
{
    AppPackagePtr desc = getAppById(appId);
    if (!desc) {
        errorText = appId + " was not found OR Unsupported Application Type";
        return false;
    }

    LOG_INFO(MSGID_APPLAUNCH_LOCK, 2,
             PMLOGKS(LOG_KEY_APPID, appId.c_str()),
             PMLOGKS("lock_status", (lock ? "locked" : "unlocked")), "");

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
                LOG_INFO(MSGID_PACKAGE_STATUS, 1,
                         PMLOGKS("added_after_full_scan", app.first.c_str()), "");
                EventAppStatusChanged(AppStatusChangeEvent::AppStatusChangeEvent_Installed, app.second);
            }
        }
        std::vector<std::string> removed_apps;
        for (const auto& app : m_appDescMaps) {
            if (scanned_apps.count(app.first) == 0) {
                LOG_INFO(MSGID_PACKAGE_STATUS, 1,
                         PMLOGKS("removed_after_full_scan", app.first.c_str()), "");
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
            const std::string app_id = it.first;
            AppPackagePtr new_desc = it.second;
            addApp(app_id, new_desc, AppStatusChangeEvent::STORAGE_ATTACHED);
        }
    }
}

void AppPackageManager::onReadyToUninstallSystemApp(pbnjson::JValue result, ErrorInfo err_info, void* user_data)
{
    std::string *ptrAppId = static_cast<std::string*>(user_data);
    std::string app_id = *ptrAppId;
    delete ptrAppId;

    if (app_id.empty())
        return;

    if (!result.hasKey("returnValue")) {
        LOG_ERROR(MSGID_CALLCHAIN_ERR, 1,
                  PMLOGKS(LOG_KEY_REASON, "key_does_not_exist"), "");
        return;
    }

    if (!result["returnValue"].asBool()) {
        LOG_INFO(MSGID_UNINSTALL_APP, 3,
                 PMLOGKS(LOG_KEY_APPID, app_id.c_str()),
                 PMLOGKS("app_type", "system"),
                 PMLOGKS("status", "invalid_pincode"), "uninstallation is canceled");
        return;
    }

    removeApp(app_id, false, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
}

void AppPackageManager::uninstallApp(const std::string& id, std::string& errorReason)
{
    AppPackagePtr appDesc = getAppById(id);
    if (!appDesc)
        return;

    if (!appDesc->isRemovable()) {
        errorReason = id + " is not removable";
        return;
    }

    LOG_INFO(MSGID_UNINSTALL_APP, 1,
             PMLOGKS(LOG_KEY_APPID, id.c_str()),
             "start_uninstall_app");

    if (AppTypeByDir::AppTypeByDir_System_BuiltIn == appDesc->getTypeByDir()) {
        CallChain& callchain = CallChain::acquire(
                boost::bind(&AppPackageManager::onReadyToUninstallSystemApp, this, _1, _2, _3),
        NULL, new std::string(appDesc->getAppId()));

        if (appDesc->isVisible()) {
            LOG_DEBUG("%s : check parental lock to remove %s app", __FUNCTION__, id.c_str());
            auto settingValPtr = std::make_shared<CallChainEventHandler::CheckAppLockStatus>(appDesc->getAppId());
            auto pinPtr = std::make_shared<CallChainEventHandler::CheckPin>();
            callchain.add(settingValPtr).addIf(settingValPtr, false, pinPtr);
        }
        callchain.run();
    } else {
        pbnjson::JValue payload = pbnjson::Object();
        payload.put("id", id);
        payload.put("subscribe", false);

        LSErrorSafe lserror;

        if (!LSCallOneReply(ApplicationManager::getInstance().get(),
                            "luna://com.webos.appInstallService/remove",
                            payload.stringify().c_str(),
                            cbAppUninstalled,
                            NULL,
                            NULL,
                            &lserror)) {
            errorReason = lserror.message;
            return;
        }

        LOG_INFO(MSGID_UNINSTALL_APP, 1,
                 PMLOGKS(LOG_KEY_APPID, id.c_str()),
                 "requested_to_appinstalld");
    }
}

bool AppPackageManager::cbAppUninstalled(LSHandle* lshandle, LSMessage *message, void* data)
{

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(message), std::string(""));

    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_UNINSTALL_APP_ERR, 1,
                  PMLOGKS("status", "received_return_false"),
                  "null jmsg");
    }

    LOG_INFO(MSGID_UNINSTALL_APP, 1,
             PMLOGKS("status", "recieved_result"), "%s", jmsg.stringify().c_str());

    return true;
}

void AppPackageManager::onPackageStatusChanged(const std::string& app_id, const PackageStatus& status)
{
    if (PackageStatus::Installed == status)
        onAppInstalled(app_id);
    else if (PackageStatus::InstallFailed == status)
        reloadApp(app_id);
    else if (PackageStatus::Uninstalled == status)
        onAppUninstalled(app_id);
    else if (PackageStatus::UninstallFailed == status)
        reloadApp(app_id);
    else if (PackageStatus::ResetToDefault == status)
        reloadApp(app_id);
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
