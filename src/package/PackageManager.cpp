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

#include <bus/appmgr_service.h>
#include <lifecycle/app_info_manager.h>
#include <lifecycle/lifecycle_manager.h>
#include <module/locale_preferences.h>
#include <package/PackageManager.h>
#include <setting/settings.h>
#include <algorithm>
#include <sys/types.h>
#include <util/call_chain.h>
#include <util/jutil.h>
#include <util/logging.h>
#include <util/lsutils.h>
#include <util/utils.h>
#include <utime.h>


const std::string APP_CHANGE_ADDED = "added";
const std::string APP_CHANGE_REMOVED = "removed";
const std::string APP_CHANGE_UPDATED = "updated";

namespace CallChainEventHandler {
class CheckAppLockStatus: public LSCallItem {
public:
    CheckAppLockStatus(const std::string& appId) :
            LSCallItem(AppMgrService::instance().serviceHandle(), "luna://com.webos.settingsservice/batch", "{}")
    {
        std::string query = std::string(
                        R"({"operations":[
          {"method":"getSystemSettings","params":{"category":"lock","key":"parentalControl"}},
          {"method":"getSystemSettings","params":{
            "category":"lock","key":"applockPerApp","app_id":")")
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
    CheckPin() :
            LSCallItem(AppMgrService::instance().serviceHandle(), "luna://com.webos.notification/createPincodePrompt", R"({"promptType":"parental"})")
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

PackageManager::PackageManager() :
        m_isFirstFullScanStarted(false)
{
    m_appScanner.signalAppScanFinished.connect(boost::bind(&PackageManager::onAppScanFinished, this, _1, _2));
}

PackageManager::~PackageManager()
{
    clear();
}

void PackageManager::clear()
{
    m_appDescMaps.clear();
}

void PackageManager::init()
{
    LocalePreferences::instance().signalLocaleChanged.connect(boost::bind(&PackageManager::onLocaleChanged, this, _1, _2, _3));

    AppinstalldSubscriber::instance().SubscribeInstallStatus(boost::bind(&PackageManager::onPackageStatusChanged, this, _1, _2));

    // required apps scanning : before rw filesystem ready
    const BaseScanPaths& base_dirs = SettingsImpl::instance().GetBaseAppDirs();
    for (auto& it : base_dirs) {
        m_appScanner.addDirectory(it.first, it.second);
    }
}

void PackageManager::startPostInit()
{
    // remaining apps scanning : after rw filesystem ready
    const BaseScanPaths& base_dirs = SettingsImpl::instance().GetBaseAppDirs();
    for (auto& it : base_dirs) {
        m_appScanner.addDirectory(it.first, it.second);
    }

    scan();
}

void PackageManager::scanInitialApps()
{
    auto boot_time_apps = SettingsImpl::instance().GetBootTimeApps();
    for (auto& app_id : boot_time_apps) {
        AppDescPtr app_desc = m_appScanner.scanForOneApp(app_id);
        if (app_desc) {
            m_appDescMaps[app_id] = app_desc;
        }
    }
}

void PackageManager::scan()
{
    if (!m_isFirstFullScanStarted) {
        LOG_INFO(MSGID_START_SCAN, 1, PMLOGKS("STATUS", "FIRST_FULL_SCAN"), "");
        m_isFirstFullScanStarted = true;
    }
    m_appScanner.run(ScanMode::FULL_SCAN);
}

void PackageManager::rescan(const std::vector<std::string>& reason)
{
    for (const auto& r : reason) {
        auto it = std::find(m_scanReason.begin(), m_scanReason.end(), r);
        if (it != m_scanReason.end())
            continue;
        m_scanReason.push_back(r);
    }

    if (!m_isFirstFullScanStarted)
        return;

    LOG_INFO(MSGID_START_SCAN, 1, PMLOGKS("STATUS", "RESCAN"), "");
    scan();
}

void PackageManager::onAppInstalled(const std::string& app_id)
{
    AppInfoManager::instance().setExecutionLock(app_id, false);

    AppDescPtr new_desc = m_appScanner.scanForOneApp(app_id);
    if (!new_desc) {
        return;
    }

    // skip if stub type, (stub apps will be loaded on next full scan)
    if (AppType::AppType_Stub == new_desc->type()) {
        return;
    }

    // disallow dev apps if current not in dev mode
    if (AppTypeByDir::AppTypeByDir_Dev == new_desc->getTypeByDir() && SettingsImpl::instance().isDevMode == false) {
        return;
    }

    addApp(app_id, new_desc, AppStatusChangeEvent::AppStatusChangeEvent_Installed);

    // clear web app cache
    if (AppTypeByDir::AppTypeByDir_Dev == new_desc->getTypeByDir() && AppType::AppType_Web == new_desc->type()) {
        (void) LSCallOneReply(AppMgrService::instance().serviceHandle(), "luna://com.palm.webappmanager/discardCodeCache", "{\"force\":true}", NULL, NULL, NULL, NULL);
    }
}

void PackageManager::onAppUninstalled(const std::string& app_id)
{
    removeApp(app_id, true, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
}

const AppDescMaps& PackageManager::allApps()
{
    return m_appDescMaps;
}

AppDescPtr PackageManager::getAppById(const std::string& app_id)
{
    if (m_appDescMaps.count(app_id) == 0)
        return NULL;
    return m_appDescMaps[app_id];
}

void PackageManager::replaceAppDesc(const std::string& app_id, AppDescPtr new_desc)
{
    if (m_appDescMaps.count(app_id) == 0) {
        m_appDescMaps[app_id] = new_desc;
        return;
    }

    m_appDescMaps[app_id] = new_desc;
}

void PackageManager::addApp(const std::string& appId, AppDescPtr newAppdescPtr, AppStatusChangeEvent event)
{
    AppDescPtr appDescPtr = getAppById(appId);

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

void PackageManager::removeApp(const std::string& app_id, bool rescan, AppStatusChangeEvent event)
{
    AppDescPtr current_desc = getAppById(app_id);
    if (!current_desc)
        return;

    AppInfoManager::instance().setExecutionLock(app_id, false);

    if (current_desc->isBuiltinBasedApp() && AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled == event) {
        LOG_INFO(MSGID_UNINSTALL_APP, 3,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("app_type", "system"),
                 PMLOGKS("location", "rw"), "remove system-app in read-write area");
        SettingsImpl::instance().setSystemAppAsRemoved(app_id);
    }

    // just remove current app without rescan
    if (!rescan) {
        publishOneAppChange(current_desc, APP_CHANGE_REMOVED, event);
        m_appDescMaps.erase(app_id);
        AppInfoManager::instance().removeAppInfo(app_id);
        return;
    }

    // rescan file system
    AppDescPtr rescanned_desc = m_appScanner.scanForOneApp(app_id);
    if (!rescanned_desc) {
        publishOneAppChange(current_desc, APP_CHANGE_REMOVED, event);
        m_appDescMaps.erase(app_id);
        AppInfoManager::instance().removeAppInfo(app_id);
        return;
    }

    // compare current app and rescanned app
    // if same
    if (current_desc->isSame(current_desc, rescanned_desc)) {
        // (still remains in file system)
        return;
        // if not same
    } else {
        replaceAppDesc(app_id, rescanned_desc);
        publishOneAppChange(rescanned_desc, APP_CHANGE_UPDATED, event);
    }

    LOG_INFO(MSGID_UNINSTALL_APP, 2,
             PMLOGKS("app_id", app_id.c_str()),
             PMLOGKS("status", "clear_memory"), "event: %d", (int ) event);
}

void PackageManager::reloadApp(const std::string& app_id)
{
    AppDescPtr current_desc = getAppById(app_id);
    if (!current_desc) {
        LOG_INFO(MSGID_PACKAGE_STATUS, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("action", "reload"), "no current description, just skip");
        return;
    }

    AppInfoManager::instance().setExecutionLock(app_id, false);

    AppDescPtr rescanned_desc = m_appScanner.scanForOneApp(app_id);
    if (!rescanned_desc) {
        publishOneAppChange(current_desc, APP_CHANGE_REMOVED, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
        m_appDescMaps.erase(app_id);
        AppInfoManager::instance().removeAppInfo(app_id);
        LOG_INFO(MSGID_PACKAGE_STATUS, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("action", "reload"), "no app package left, just remove");
        return;
    }

    if (current_desc->isSame(current_desc, rescanned_desc)) {
        LOG_INFO(MSGID_PACKAGE_STATUS, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("action", "reload"),
"no change, just skip");
        return;
    } else {
        replaceAppDesc(app_id, rescanned_desc);
        publishOneAppChange(rescanned_desc, APP_CHANGE_UPDATED, AppStatusChangeEvent::UPDATE_COMPLETED);
        LOG_INFO(MSGID_PACKAGE_STATUS, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("action", "reload"), "different package detected, update info now");
    }
}

bool PackageManager::lockAppForUpdate(const std::string& appId, bool lock, std::string& err_text)
{
    AppDescPtr desc = getAppById(appId);
    if (!desc) {
        err_text = appId + " was not found OR Unsupported Application Type";
        return false;
    }

    LOG_INFO(MSGID_APPLAUNCH_LOCK, 2,
             PMLOGKS("app_id", appId.c_str()),
             PMLOGKS("lock_status", (lock?"locked":"unlocked")), "");
    AppInfoManager::instance().setExecutionLock(appId, lock);
    return true;
}

void PackageManager::onLocaleChanged(const std::string& lang, const std::string& region, const std::string& script)
{
    m_appScanner.setBCP47Info(lang, region, script);
    rescan( { "LANG" });
}

void PackageManager::onAppScanFinished(ScanMode mode, const AppDescMaps& scanned_apps)
{
    if (ScanMode::FULL_SCAN == mode) {

        // check changes after rescan
        for (const auto& app : scanned_apps) {
            if (m_appDescMaps.count(app.first) == 0) {
                LOG_INFO(MSGID_PACKAGE_STATUS, 1, PMLOGKS("added_after_full_scan", app.first.c_str()), "");
                signalAppStatusChanged(AppStatusChangeEvent::AppStatusChangeEvent_Installed, app.second);
            }
        }
        std::vector<std::string> removed_apps;
        for (const auto& app : m_appDescMaps) {
            if (scanned_apps.count(app.first) == 0) {
                LOG_INFO(MSGID_PACKAGE_STATUS, 1, PMLOGKS("removed_after_full_scan", app.first.c_str()), "");
                removed_apps.push_back(app.first);
                signalAppStatusChanged(AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled, app.second);
            }
        }

        // close removed apps if it's running
        if (!removed_apps.empty())
            LifecycleManager::instance().closeApps(removed_apps, true);

        clear();
        m_appDescMaps = scanned_apps;
        signalAllAppRosterChanged(m_appDescMaps);
        publishListApps();
        m_scanReason.clear();
    } else if (ScanMode::PARTIAL_SCAN == mode) {

        for (auto& it : scanned_apps) {
            const std::string app_id = it.first;
            AppDescPtr new_desc = it.second;
            addApp(app_id, new_desc, AppStatusChangeEvent::STORAGE_ATTACHED);
        }
    }
}

void PackageManager::onReadyToUninstallSystemApp(pbnjson::JValue result, ErrorInfo err_info, void* user_data)
{
    std::string *ptrAppId = static_cast<std::string*>(user_data);
    std::string app_id = *ptrAppId;
    delete ptrAppId;

    if (app_id.empty())
        return;

    if (!result.hasKey("returnValue")) {
        LOG_ERROR(MSGID_CALLCHAIN_ERR, 1, PMLOGKS("reason", "key_does_not_exist"), "");
        return;
    }

    if (!result["returnValue"].asBool()) {
        LOG_INFO(MSGID_UNINSTALL_APP, 3,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("app_type", "system"),
                 PMLOGKS("status", "invalid_pincode"), "uninstallation is canceled");
        return;
    }

    removeApp(app_id, false, AppStatusChangeEvent::AppStatusChangeEvent_Uninstalled);
}

void PackageManager::uninstallApp(const std::string& id, std::string& errorReason)
{
    AppDescPtr appDesc = getAppById(id);
    if (!appDesc)
        return;

    if (!appDesc->isRemovable()) {
        errorReason = id + " is not removable";
        return;
    }

    LOG_INFO(MSGID_UNINSTALL_APP, 1, PMLOGKS("app_id", id.c_str()), "start_uninstall_app");

    if (AppTypeByDir::AppTypeByDir_System_BuiltIn == appDesc->getTypeByDir()) {
        CallChain& callchain = CallChain::acquire(
                boost::bind(&PackageManager::onReadyToUninstallSystemApp, this, _1, _2, _3),
        NULL, new std::string(appDesc->id()));

        if (appDesc->isVisible()) {
            LOG_DEBUG("%s : check parental lock to remove %s app", __FUNCTION__, id.c_str());
            auto settingValPtr = std::make_shared<CallChainEventHandler::CheckAppLockStatus>(appDesc->id());
            auto pinPtr = std::make_shared<CallChainEventHandler::CheckPin>();
            callchain.add(settingValPtr).add_if(settingValPtr, false, pinPtr);
        }
        callchain.run();
    } else {
        pbnjson::JValue payload = pbnjson::Object();
        payload.put("id", id);
        payload.put("subscribe", false);

        LSErrorSafe lserror;

        if (!LSCallOneReply(AppMgrService::instance().serviceHandle(),
                            "luna://com.webos.appInstallService/remove",
                            payload.stringify().c_str(),
                            cbAppUninstalled,
                            NULL,
                            NULL,
                            &lserror)) {
            errorReason = lserror.message;
            return;
        }

        LOG_INFO(MSGID_UNINSTALL_APP, 1, PMLOGKS("app_id", id.c_str()), "requested_to_appinstalld");
    }
}

bool PackageManager::cbAppUninstalled(LSHandle* lshandle, LSMessage *message, void* data)
{

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(message), std::string(""));

    if (jmsg.isNull()) {
        LOG_ERROR(MSGID_UNINSTALL_APP_ERR, 1, PMLOGKS("status", "received_return_false"), "null jmsg");
    }

    LOG_INFO(MSGID_UNINSTALL_APP, 1, PMLOGKS("status", "recieved_result"), "%s", jmsg.stringify().c_str());

    return true;
}

void PackageManager::onPackageStatusChanged(const std::string& app_id, const PackageStatus& status)
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

std::string PackageManager::toString(const AppStatusChangeEvent& event)
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

void PackageManager::publishListApps()
{
    pbnjson::JValue apps = pbnjson::Array();
    pbnjson::JValue dev_apps = pbnjson::Array();

    for (auto it : m_appDescMaps) {
        apps.append(it.second->toJValue());

        if (SettingsImpl::instance().isDevMode && AppTypeByDir::AppTypeByDir_Dev == it.second->getTypeByDir())
            dev_apps.append(it.second->toJValue());
    }

    signalListAppsChanged(apps, m_scanReason, false);

    if (SettingsImpl::instance().isDevMode)
        signalListAppsChanged(dev_apps, m_scanReason, true);
}

void PackageManager::publishOneAppChange(AppDescPtr app_desc, const std::string& change, AppStatusChangeEvent event)
{
    pbnjson::JValue app = app_desc->toJValue();
    std::string reason = (event != AppStatusChangeEvent::AppStatusChangeEvent_Nothing) ? toString(event) : "";

    signalOneAppChanged(app, change, reason, false);
    if (SettingsImpl::instance().isDevMode && AppTypeByDir::AppTypeByDir_Dev == app_desc->getTypeByDir())
        signalOneAppChanged(app, change, reason, true);

    signalAppStatusChanged(event, app_desc);
}
