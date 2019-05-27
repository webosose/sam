// Copyright (c) 2017-2018 LG Electronics, Inc.
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

#include "core/package/virtual_app_manager.h"

#include "core/base/logging.h"
#include "core/base/lsutils.h"
#include "core/bus/appmgr_service.h"
#include "core/lifecycle/app_life_manager.h"
#include "core/package/application_manager.h"
#include "core/setting/settings.h"

// virtual app related
#define DONOT_REPLY false
const int ERROR_CODE_COMMON = -1;

const std::string VIRTUALAPP_ERR_TXT_INVALID_PARAM = "invalid params";
const std::string VIRTUALAPP_ERR_TXT_INVALID_HOSTAPP = "invalid host app id";
const std::string VIRTUALAPP_ERR_TXT_INTERNAL_ERR = "internal error";
const std::string VIRTUALAPP_ERR_TXT_PRIVILEGED_APP_ID = "cannot use privileged app id";
const std::string VIRTUALAPP_ERR_TXT_CREATING_APP_INFO_FAIL = "failed to create app info";
const std::string VIRTUALAPP_ERR_TXT_INSTALL_LSCONFIG_FAIL = "failed to install lsconfig files";
const std::string VIRTUALAPP_ERR_TXT_ALREADY_EXIST = "already exist";

VirtualAppManager::VirtualAppManager()
{
}

VirtualAppManager::~VirtualAppManager()
{
}

void VirtualAppManager::InstallTmpVirtualAppOnLaunch(const pbnjson::JValue& jmsg, LSMessage* lsmsg)
{

    if (jmsg.isNull()) {
        ReplyForRequest(lsmsg, ERROR_CODE_COMMON, VIRTUALAPP_ERR_TXT_INVALID_PARAM);
        return;
    }

    // create new request to handle
    VirtualAppRequestPtr new_request = MakeVirtualAppRequest(VirtualAppType::TEMP, jmsg, lsmsg);
    if (new_request == NULL) {
        ReplyForRequest(lsmsg, ERROR_CODE_COMMON, "internal error (memory)");
        return;
    }

    work_items_.push_back(new_request);

    std::string app_id = new_request->jmsg["id"].asString();
    std::string host_id = new_request->jmsg["hostId"].asString();

    // validate host app
    if (!ValidataHostApp(host_id)) {
        LOG_INFO(MSGID_VIRTUAL_APP_WARNING, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("host_id", host_id.c_str()), PMLOGKS("status", "invalid_host_app"), "");
        new_request->error_code = ERROR_CODE_COMMON;
        new_request->error_text = VIRTUALAPP_ERR_TXT_INVALID_HOSTAPP;
        FinishJob(new_request);
        return;
    }

    // just launch if exists
    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if (app_desc) {
        LOG_INFO(MSGID_TMP_VIRTUAL_APP_LAUNCH, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "just_launch"), "already exist");

        AppLifeManager::instance().launch(AppLaunchRequestType::EXTERNAL_FOR_VIRTUALAPP, app_id, new_request->jmsg, new_request->lsmsg);

        // don't reply here. app life manager will reply when launching done
        FinishJob(new_request, DONOT_REPLY);
    } else {
        // create temp virtual app package if not exists
        // 1. remove luna-configuration first
        // 2. create virtual app
        std::string device_id = GetDeviceId(VirtualAppType::TEMP);

        // remove ls conf -> create package -> create ls conf -> scan -> launch
        if (!RemoveLSConfig(app_id, device_id, VirtualAppManager::OnLSConfigRemoved, &(new_request->async_job_token))) {
            new_request->error_code = ERROR_CODE_COMMON;
            new_request->error_text = VIRTUALAPP_ERR_TXT_INTERNAL_ERR;
            FinishJob(new_request);
        }
    }
}

bool VirtualAppManager::OnTempPackageReady(LSHandle* lshandle, LSMessage* lsmsg, void* user_data)
{

    LSMessageToken token = LSMessageGetResponseToken(lsmsg);
    VirtualAppRequestPtr item = VirtualAppManager::instance().GetWorkItemByToken(token);

    if (item == NULL) {
        LOG_ERROR(MSGID_VIRTUAL_APP_WARNING, 2, PMLOGKFV("token", "%d", (int) token), PMLOGKS("status", "failed_to_get_data_from_async_return"), "where: %s", __FUNCTION__);
        return true;
    }

    std::string app_id = item->jmsg["id"].asString();

    LOG_INFO(MSGID_TMP_VIRTUAL_APP_LAUNCH, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "received_async_return"), "token: %d", (int )token);

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull() || !jmsg.hasKey("returnValue") || !jmsg["returnValue"].asBool()) {
        LOG_ERROR(MSGID_VIRTUAL_APP_WARNING, 2,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("status", "received_false"),
                  "return: %s", jmsg.stringify().c_str());
        item->error_code = ERROR_CODE_COMMON;
        item->error_text = VIRTUALAPP_ERR_TXT_INSTALL_LSCONFIG_FAIL;
        VirtualAppManager::instance().FinishJob(item);
        return true;
    }

    AppDescPtr current_app_desc = ApplicationManager::instance().getAppById(app_id);
    if (current_app_desc && current_app_desc->getTypeByDir() == AppTypeByDir::Alias) {
        // now launch existing virtual app
        AppLifeManager::instance().launch(AppLaunchRequestType::EXTERNAL_FOR_VIRTUALAPP, app_id, item->jmsg, item->lsmsg);
        return true;
    } else if (current_app_desc && current_app_desc->getTypeByDir() == AppTypeByDir::TempAlias) {
        // remove existing tmp virtual app info
        LOG_INFO(MSGID_TMP_VIRTUAL_APP_LAUNCH, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "remove_existing_app_desc"), "");
        ApplicationManager::instance().RemoveApp(app_id);
    }

    // scan new app info
    ApplicationManager::instance().OnAppInstalled(app_id);

    // now launch tmp virtual app which is just created
    AppLifeManager::instance().launch(AppLaunchRequestType::EXTERNAL_FOR_VIRTUALAPP, app_id, item->jmsg, item->lsmsg);

    // don't reply here. app life manager will reply when launching done
    VirtualAppManager::instance().FinishJob(item, DONOT_REPLY);

    LOG_INFO(MSGID_TMP_VIRTUAL_APP_LAUNCH, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "done"), "");

    return true;
}

void VirtualAppManager::InstallVirtualApp(const pbnjson::JValue& jmsg, LSMessage* lsmsg)
{

    if (jmsg.isNull()) {
        ReplyForRequest(lsmsg, ERROR_CODE_COMMON, VIRTUALAPP_ERR_TXT_INVALID_PARAM);
        return;
    }

    VirtualAppRequestPtr new_request = MakeVirtualAppRequest(VirtualAppType::REGULAR, jmsg, lsmsg);
    if (new_request == NULL) {
        ReplyForRequest(lsmsg, ERROR_CODE_COMMON, "internal error (memory)");
        return;
    }

    work_items_.push_back(new_request);

    std::string app_id = new_request->jmsg["id"].asString();
    std::string host_id = new_request->jmsg["hostId"].asString();

    // validate host app
    if (!ValidataHostApp(host_id)) {
        LOG_INFO(MSGID_VIRTUAL_APP_WARNING, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("host_id", host_id.c_str()), PMLOGKS("status", "invalid_host_app"), "");
        new_request->error_code = ERROR_CODE_COMMON;
        new_request->error_text = VIRTUALAPP_ERR_TXT_INVALID_HOSTAPP;
        FinishJob(new_request);
        return;
    }

    // just return false if exists
    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if (app_desc && app_desc->getTypeByDir() == AppTypeByDir::Alias) {
        LOG_INFO(MSGID_ADD_VIRTUAL_APP, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("host_id", host_id.c_str()), PMLOGKS("status", "finish_handling"), "already exist");
        new_request->error_code = ERROR_CODE_COMMON;
        new_request->error_text = VIRTUALAPP_ERR_TXT_ALREADY_EXIST;
        FinishJob(new_request);
    } else if (app_desc && app_desc->getTypeByDir() == AppTypeByDir::TempAlias) {
        // remove existing tmp virtual app info
        LOG_INFO(MSGID_ADD_VIRTUAL_APP, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "remove_existing_app_desc"), "");

        std::string device_id = GetDeviceId(VirtualAppType::TEMP);
        if (!RemoveLSConfig(app_id, device_id, VirtualAppManager::OnLSConfigRemoved, &(new_request->async_job_token))) {
            new_request->error_code = ERROR_CODE_COMMON;
            new_request->error_text = VIRTUALAPP_ERR_TXT_INTERNAL_ERR;
            FinishJob(new_request);
            return;
        }
        ApplicationManager::instance().RemoveApp(app_id);
    } else {
        CreateVirtualApp(new_request);
    }
}

void VirtualAppManager::CreateVirtualApp(VirtualAppRequestPtr request)
{

    if (request == NULL)
        return;

    std::string app_id = request->jmsg["id"].asString();
    std::string host_id = request->jmsg["hostId"].asString();
    VirtualAppType app_type = request->app_type;
    std::string error_text;

    if (!CreateVirtualAppPackage(app_id, host_id, request->jmsg["appInfo"], request->jmsg["params"], app_type, error_text)) {
        LOG_INFO(MSGID_VIRTUAL_APP_WARNING, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("host_id", host_id.c_str()), PMLOGKS("status", "failed_to_create_virtual_app_package"), "");
        request->error_code = ERROR_CODE_COMMON;
        request->error_text = error_text;
        FinishJob(request);
        return;
    }

    // now request to create ls config files to appinstallservice
    std::string device_id = GetDeviceId(app_type);
    bool ret_val = false;
    if (app_type == VirtualAppType::REGULAR)
        ret_val = CreateLSConfig(app_id, device_id, VirtualAppManager::OnPackageReady, request->async_job_token);
    else if (app_type == VirtualAppType::TEMP)
        ret_val = CreateLSConfig(app_id, device_id, VirtualAppManager::OnTempPackageReady, request->async_job_token);

    if (!ret_val) {
        LOG_INFO(MSGID_VIRTUAL_APP_WARNING, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("host_id", host_id.c_str()), PMLOGKS("status", "failed_to_request_creating_ls_config"), "");
        request->error_code = ERROR_CODE_COMMON;
        request->error_text = VIRTUALAPP_ERR_TXT_INSTALL_LSCONFIG_FAIL;
        FinishJob(request);
        return;
    }

    LOG_INFO(MSGID_ADD_VIRTUAL_APP, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "requested_creating_ls_config"), "token: %d", (int ) request->async_job_token);
}

bool VirtualAppManager::OnPackageReady(LSHandle* lshandle, LSMessage* lsmsg, void* user_data)
{

    LSMessageToken token = LSMessageGetResponseToken(lsmsg);
    VirtualAppRequestPtr item = VirtualAppManager::instance().GetWorkItemByToken(token);
    if (item == NULL) {
        LOG_ERROR(MSGID_VIRTUAL_APP_WARNING, 2, PMLOGKFV("token", "%d", (int) token), PMLOGKS("status", "failed_to_get_data_from_async_return"), "where: %s", __FUNCTION__);
        return true;
    }

    std::string app_id = item->jmsg["id"].asString();

    LOG_INFO(MSGID_ADD_VIRTUAL_APP, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "received_async_return"), "token: %d", (int )token);

    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));
    if (jmsg.isNull() || !jmsg.hasKey("returnValue") || !jmsg["returnValue"].asBool()) {
        LOG_ERROR(MSGID_VIRTUAL_APP_WARNING, 2,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("status", "received_false"),
                  "return: %s", jmsg.stringify().c_str());
        item->error_code = ERROR_CODE_COMMON;
        item->error_text = VIRTUALAPP_ERR_TXT_INSTALL_LSCONFIG_FAIL;
        VirtualAppManager::instance().FinishJob(item);
        return true;
    }

    // return false if exists (it's possible in async sequence)
    AppDescPtr current_app_desc = ApplicationManager::instance().getAppById(app_id);
    if (current_app_desc && current_app_desc->getTypeByDir() == AppTypeByDir::Alias) {
        LOG_INFO(MSGID_ADD_VIRTUAL_APP, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "finish_handling"), "already exist");
        item->error_code = ERROR_CODE_COMMON;
        item->error_text = VIRTUALAPP_ERR_TXT_ALREADY_EXIST;
        VirtualAppManager::instance().FinishJob(item);
        return true;
    } else if (current_app_desc && current_app_desc->getTypeByDir() == AppTypeByDir::TempAlias) {
        LOG_INFO(MSGID_ADD_VIRTUAL_APP, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "remove_existing_app_desc"), "");
        // remove existing tmp virtual app
        ApplicationManager::instance().RemoveApp(app_id);
    }

    // scan new app info
    ApplicationManager::instance().OnAppInstalled(app_id);

    VirtualAppManager::instance().FinishJob(item);

    LOG_INFO(MSGID_ADD_VIRTUAL_APP, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "done"), "");

    return true;
}

void VirtualAppManager::UninstallVirtualApp(const pbnjson::JValue& jmsg, LSMessage* lsmsg)
{

    if (jmsg.isNull()) {
        ReplyForRequest(lsmsg, ERROR_CODE_COMMON, VIRTUALAPP_ERR_TXT_INVALID_PARAM);
        return;
    }

    std::string app_id = jmsg["id"].asString();
    std::string error_text = "";

    // load app description
    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if (!app_desc) {
        ReplyForRequest(lsmsg, ERROR_CODE_COMMON, "app does not exist");
        return;
    }

    // need to check app lock status (like downloadable apps)
    if (AppTypeByDir::Alias == app_desc->getTypeByDir()) {
        ApplicationManager::instance().UninstallApp(app_id, error_text);
        if (!error_text.empty()) {
            ReplyForRequest(lsmsg, ERROR_CODE_COMMON, error_text);
            return;
        }
    }
    // don't need to check app lock, just remove it
    else if (AppTypeByDir::TempAlias == app_desc->getTypeByDir()) {
        error_text = "";
        AppLifeManager::instance().closeByAppId(app_id, "", "", error_text, false);

        RemoveVirtualAppPackage(app_id, VirtualAppType::TEMP);
    }
    // only allowed to proceed virtual app related request
    else {
        ReplyForRequest(lsmsg, ERROR_CODE_COMMON, "app is not virtul type");
        return;
    }

    // return success
    ReplyForRequest(lsmsg, 0, "");
}

bool VirtualAppManager::OnLSConfigRemoved(LSHandle* lshandle, LSMessage* lsmsg, void* user_data)
{
    LSMessageToken token = LSMessageGetResponseToken(lsmsg);
    VirtualAppRequestPtr item = VirtualAppManager::instance().GetWorkItemByToken(token);
    pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(lsmsg), std::string(""));

    // removeVirtualApp do nothing
    if (item == NULL)
        return true;

    if (jmsg.isNull() || !jmsg.hasKey("returnValue") || !jmsg["returnValue"].asBool()) {
        item->error_code = ERROR_CODE_COMMON;
        item->error_text = VIRTUALAPP_ERR_TXT_INSTALL_LSCONFIG_FAIL;
        VirtualAppManager::instance().FinishJob(item);
        return true;
    }

    // addVirtualApp
    VirtualAppManager::instance().CreateVirtualApp(item);
    return true;
}

////////////////////////////////////////////////////////////////
// TODO: Generalize making request item for all API requests
//       Also generalize replying for requests
VirtualAppRequestPtr VirtualAppManager::MakeVirtualAppRequest(VirtualAppType app_type, const pbnjson::JValue& jmsg, LSMessage* lsmsg)
{
    VirtualAppRequestPtr new_request = std::make_shared<VirtualAppRequest>(app_type, jmsg, lsmsg);
    if (new_request == NULL)
        return NULL;

    return new_request;
}

VirtualAppRequestPtr VirtualAppManager::GetWorkItemByToken(LSMessageToken token)
{
    auto it = std::find_if(work_items_.begin(), work_items_.end(), [&token](VirtualAppRequestPtr request) {return (request->async_job_token == token);});
    if (it == work_items_.end())
        return NULL;
    return (*it);
}

std::string VirtualAppManager::GetAppBasePath(VirtualAppType app_type)
{
    if (app_type == VirtualAppType::REGULAR)
        return SettingsImpl::instance().aliasAppBasePath + std::string("/") + SettingsImpl::instance().appInstallRelative;
    else if (app_type == VirtualAppType::TEMP)
        return SettingsImpl::instance().tempAliasAppBasePath + std::string("/") + SettingsImpl::instance().appInstallRelative;
    else
        return "";
}

std::string VirtualAppManager::GetDeviceId(VirtualAppType app_type)
{
    if (app_type == VirtualAppType::REGULAR)
        return "INTERNAL_STORAGE_ALIAS";
    else if (app_type == VirtualAppType::TEMP)
        return "INTERNAL_STORAGE_ALIASTMP";
    else
        return "";
}

void VirtualAppManager::FinishJob(VirtualAppRequestPtr va_request, bool need_reply)
{
    if (need_reply)
        ReplyForRequest(va_request->lsmsg, va_request->error_code, va_request->error_text);

    for (auto it = work_items_.begin(); it != work_items_.end(); ++it) {
        if ((*it) == va_request) {
            work_items_.erase(it);
            break;
        }
    }
}

void VirtualAppManager::ReplyForRequest(LSMessage* lsmsg, int error_code, const std::string& error_text)
{
    if (lsmsg == NULL)
        return;

    pbnjson::JValue payload = pbnjson::Object();
    if (error_text.empty()) {
        payload.put("returnValue", true);
    } else {
        payload.put("returnValue", false);
        payload.put("errorCode", error_code);
        payload.put("errorText", error_text);
    }

    LOG_INFO(MSGID_VIRTUAL_APP_RETURN, 1, PMLOGJSON("payload", payload.stringify().c_str()), "");

    LSErrorSafe lserror;
    if (!LSMessageRespond(lsmsg, payload.stringify().c_str(), &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 2,
                  PMLOGKS("type", "respond"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  "err: %s", lserror.message);
    }
}

bool VirtualAppManager::ValidataHostApp(const std::string& host_id)
{

    if (!SettingsImpl::instance().is_in_host_apps_list(host_id)) {
        LOG_INFO(MSGID_VIRTUAL_APP_WARNING, 2, PMLOGKS("host_id", host_id.c_str()), PMLOGKS("status", "not_in_host_apps_list"), "");
        return false;
    }

    AppDescPtr host_app_desc = ApplicationManager::instance().getAppById(host_id);
    if (!host_app_desc) {
        LOG_INFO(MSGID_VIRTUAL_APP_WARNING, 2, PMLOGKS("host_id", host_id.c_str()), PMLOGKS("status", "does_not_exist"), "");
        return false;
    }

    return true;
}

bool VirtualAppManager::CreateVirtualAppPackage(const std::string& app_id, const std::string& host_id, const pbnjson::JValue& app_info, const pbnjson::JValue& launch_params, VirtualAppType app_type,
        std::string& error_text)
{

    if (app_id.find("com.palm.") == 0 || app_id.find("com.webos.") == 0 || app_id.find("com.lge.") == 0) {
        LOG_INFO(MSGID_VIRTUAL_APP_WARNING, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "cannot_use_privileged_app_id"), "");
        error_text = VIRTUALAPP_ERR_TXT_PRIVILEGED_APP_ID;
        return false;
    }

    AppDescPtr host_desc = ApplicationManager::instance().getAppById(host_id);
    if (!host_desc) {
        LOG_WARNING(MSGID_VIRTUAL_APP_WARNING, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("host_id", host_id.c_str()), PMLOGKS("status", "host_app_does_not_exist"), "");
        error_text = VIRTUALAPP_ERR_TXT_INVALID_HOSTAPP;
        return false;
    }

    // load default info from webapphost
    pbnjson::JValue new_app_info = host_desc->toJValue().duplicate();

    LOG_DEBUG("[host_app_info] %s", new_app_info.stringify().c_str());

    // overwrite appInfo received from caller
    // TODO: This might cause serious security issue
    //       Make a filter to limit properties which a caller can set
    if (app_info.isObject()) {
        for (auto it : app_info.children()) {
            std::string key = it.first.asString();
            if (key.empty())
                continue;
            LOG_DEBUG("key: %s, value: %s", key.c_str(), app_info[key].stringify().c_str());
            new_app_info.put(key, app_info[key]);
        }
    }

    // post setting
    new_app_info.remove("mimeTypes");
    new_app_info.remove("keywords");
    new_app_info.put("id", app_id);
    new_app_info.put("launchParams", launch_params);
    new_app_info.put("hostFolderPath", host_desc->folderPath());
    new_app_info.put("removable", true);

    LOG_DEBUG("[virtual_app_info] %s", new_app_info.stringify().c_str());

    if (VirtualAppType::REGULAR == app_type) {
        new_app_info.put("visible", true);
        new_app_info.put("lockable", true);
    } else {
        new_app_info.put("visible", false);
        new_app_info.put("lockable", false);
    }

    LOG_INFO(MSGID_VIRTUAL_APP_INFO_CREATED, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("host_id", host_id.c_str()), PMLOGKFV("app_type", "%d", (int)app_type), "");

    std::string base_path = GetAppBasePath(app_type);
    std::string app_root_path = base_path + "/" + app_id;
    std::string app_info_path = app_root_path + "/" + "appinfo.json";

    if (0 != g_mkdir_with_parents(app_root_path.c_str(), 0700)) {
        LOG_ERROR(MSGID_VIRTUAL_APP_WARNING, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("app_root_path", app_root_path.c_str()), PMLOGKS("status", "failed_to_create_base_app_directory"), "");
        error_text = VIRTUALAPP_ERR_TXT_CREATING_APP_INFO_FAIL;
        return false;
    }

    std::string str_app_info = new_app_info.stringify();
    if (!g_file_set_contents(app_info_path.c_str(), str_app_info.c_str(), str_app_info.length(), NULL)) {
        LOG_ERROR(MSGID_VIRTUAL_APP_WARNING, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("app_info_path", app_info_path.c_str()), PMLOGKS("status", "failed_to_write_app_info"), "");
        error_text = VIRTUALAPP_ERR_TXT_CREATING_APP_INFO_FAIL;
        return false;
    }

    return true;
}

bool VirtualAppManager::CreateLSConfig(const std::string& app_id, const std::string& device_id, LSFilterFunc cb_func, LSMessageToken& msg_token)
{
    std::string payload = std::string("{\"id\":\"") + app_id + std::string("\",\"target\":{\"deviceId\":\"") + device_id + std::string("\"}}");
    msg_token = 0;
    LSErrorSafe lserror;
    if (!LSCallOneReply(AppMgrService::instance().serviceHandle(), "luna://com.webos.appInstallService/createLSConfigFiles", payload.c_str(), cb_func, NULL, &msg_token, &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "lscallonereply"), PMLOGKS("service_name", "com.webos.appInstallService"), PMLOGKS("request", "create_ls_config"), "err: %s", lserror.message);
        return false;
    }
    return true;
}

bool VirtualAppManager::RemoveLSConfig(const std::string& app_id, const std::string& device_id, LSFilterFunc cb_func, LSMessageToken* msg_token)
{
    std::string payload = std::string("{\"id\":\"") + app_id + std::string("\",\"target\":{\"deviceId\":\"") + device_id + std::string("\"}}");
    LSErrorSafe lserror;
    if (!LSCallOneReply(AppMgrService::instance().serviceHandle(), "luna://com.webos.appInstallService/removeLSConfigFiles", payload.c_str(), cb_func, NULL, msg_token, &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3, PMLOGKS("type", "lscallonereply"), PMLOGKS("service_name", "com.webos.appInstallService"), PMLOGKS("request", "remove_ls_config"), "err: %s", lserror.message);
        return false;
    }
    return true;
}

void VirtualAppManager::RemoveVirtualAppPackage(const std::string& app_id, VirtualAppType app_type)
{
    std::string device_id = GetDeviceId(app_type);
    (void) RemoveLSConfig(app_id, device_id, VirtualAppManager::OnLSConfigRemoved, NULL);

    std::string path = GetAppBasePath(app_type) + "/" + app_id;
    if (!removeDir(path)) {
        LOG_ERROR(MSGID_VIRTUAL_APP_WARNING, 4, PMLOGKS("app_id", app_id.c_str()), PMLOGKFV("app_type", "%d", (int)app_type), PMLOGKS("path", path.c_str()), PMLOGKS("status", "failed_to_remove_dir"),
                "");
        return;
    }

    LOG_INFO(MSGID_UNINSTALL_APP, 2, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("app_type", app_type == VirtualAppType::REGULAR ? "virtual_app":"tmp_virtual_app"), "path: %s", path.c_str());

    ApplicationManager::instance().RemoveApp(app_id);
}
