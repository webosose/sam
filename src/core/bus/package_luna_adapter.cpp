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

#include "core/bus/package_luna_adapter.h"

#include "core/base/logging.h"
#include "core/bus/appmgr_service.h"
#include "core/bus/lunaservice_api.h"
#include "core/lifecycle/app_life_manager.h"
#include "core/package/mime_system.h"
#include "core/package/virtual_app_manager.h"

#define SUBSKEY_LIST_APPS             "listApps"
#define SUBSKEY_LIST_APPS_COMPACT     "listAppsCompact"
#define SUBSKEY_DEV_LIST_APPS         "listDevApps"
#define SUBSKEY_DEV_LIST_APPS_COMPACT "listDevAppsCompact"

PackageLunaAdapter::PackageLunaAdapter()
{
}

PackageLunaAdapter::~PackageLunaAdapter()
{
}

void PackageLunaAdapter::PackageLunaAdapter::Init()
{
    InitLunaApiHandler();

    ApplicationManager::instance().appScanner().signalAppScanFinished.connect(boost::bind(&PackageLunaAdapter::OnScanFinished, this, _1, _2));

    ApplicationManager::instance().signalListAppsChanged.connect(boost::bind(&PackageLunaAdapter::OnListAppsChanged, this, _1, _2, _3));

    ApplicationManager::instance().signalOneAppChanged.connect(boost::bind(&PackageLunaAdapter::OnOneAppChanged, this, _1, _2, _3, _4));

    ApplicationManager::instance().signal_app_status_changed.connect(boost::bind(&PackageLunaAdapter::OnAppStatusChanged, this, _1, _2));

    AppMgrService::instance().signalOnServiceReady.connect(std::bind(&PackageLunaAdapter::OnReady, this));
}

void PackageLunaAdapter::PackageLunaAdapter::InitLunaApiHandler()
{
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_LIST_APPS, "applicationManager.listApps", boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_GET_APP_STATUS, "applicationManager.getAppStatus", boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_GET_APP_INFO, "applicationManager.getAppInfo", boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_GET_APP_BASE_PATH, "applicationManager.getAppBasePath", boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_LAUNCH_VIRTUAL_APP, "applicationManager.launchVirtualApp", boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_ADD_VIRTUAL_APP, "applicationManager.addVirtualApp", boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_REMOVE_VIRTUAL_APP, "applicationManager.removeVirtualApp", boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_REGISTER_VERBS_FOR_REDIRECT, "", boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_REGISTER_VERBS_FOR_RESOURCE, "", boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_GET_HANDLER_FOR_EXTENSION, "applicationManager.getHandlerForExtension",
            boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_LIST_EXTENSION_MAP, "", boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_MIME_TYPE_FOR_EXTENSION, "applicationManager.mimeTypeForExtension",
            boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_GET_HANDLER_FOR_MIME_TYPE, "applicationManager.getHandlerForMimeType",
            boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_GET_HANDLER_FOR_MIME_TYPE_BY_VERB, "applicationManager.getHandlerForUrlByVerb",
            boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_GET_HANDLER_FOR_URL, "applicationManager.getHandlerForUrl", boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_GET_HANDLER_FOR_URL_BY_VERB, "applicationManager.getHandlerForUrlByVerb",
            boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_LIST_ALL_HANDLERS_FOR_MIME, "applicationManager.listAllHandlersForMime",
            boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_LIST_ALL_HANDLERS_FOR_MIME_BY_VERB, "applicationManager.listAllHandlersForUrlByVerb",
            boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_LIST_ALL_HANDLERS_FOR_URL, "applicationManager.listAllHandlersForUrl",
            boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_LIST_ALL_HANDLERS_FOR_URL_BY_VERB, "applicationManager.listAllHandlersForUrlByVerb",
            boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_LIST_ALL_HANDLERS_FOR_URL_PATTERN, "applicationManager.listAllHandlersForUrlPattern",
            boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_LIST_ALL_HANDLERS_FOR_MULTIPLE_MIME, "applicationManager.listAllHandlersForMultipleMime",
            boost::bind(&PackageLunaAdapter::RequestController, this, _1));
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_GENERAL, API_LIST_ALL_HANDLERS_FOR_MULTIPLE_URL_PATTERN, "applicationManager.listAllHandlersForMultipleUrlPattern",
            boost::bind(&PackageLunaAdapter::RequestController, this, _1));

    // dev category
    AppMgrService::instance().RegisterApiHandler(API_CATEGORY_DEV, API_LIST_APPS, "applicationManager.listApps", boost::bind(&PackageLunaAdapter::RequestController, this, _1));
}

void PackageLunaAdapter::RequestController(LunaTaskPtr task)
{
    if (API_GET_APP_INFO == task->method()) {
        if (!AppMgrService::instance().IsServiceReady()) {
            LOG_INFO(MSGID_API_REQUEST, 4, PMLOGKS("category", task->category().c_str()), PMLOGKS("method", task->method().c_str()), PMLOGKS("status", "pending"),
                    PMLOGKS("caller", task->caller().c_str()), "received message, but will handle later");
            pending_tasks_on_ready_.push_back(task);
            return;
        }
    } else {
        if (!AppMgrService::instance().IsServiceReady() || ApplicationManager::instance().appScanner().isRunning()) {
            LOG_INFO(MSGID_API_REQUEST, 4, PMLOGKS("category", task->category().c_str()), PMLOGKS("method", task->method().c_str()), PMLOGKS("status", "pending"),
                    PMLOGKS("caller", task->caller().c_str()), "received message, but will handle later");
            pending_tasks_on_scanner_.push_back(task);
            return;
        }
    }

    HandleRequest(task);
}

void PackageLunaAdapter::OnReady()
{
    auto it = pending_tasks_on_ready_.begin();
    while (it != pending_tasks_on_ready_.end()) {
        HandleRequest(*it);
        it = pending_tasks_on_ready_.erase(it);
    }
}

void PackageLunaAdapter::OnScanFinished(ScanMode mode, const AppDescMaps& scannced_apps)
{
    auto it = pending_tasks_on_scanner_.begin();
    while (it != pending_tasks_on_scanner_.end()) {
        HandleRequest(*it);
        it = pending_tasks_on_scanner_.erase(it);
    }
}

void PackageLunaAdapter::HandleRequest(LunaTaskPtr task)
{
    if (API_CATEGORY_GENERAL == task->category()) {
        if (API_LIST_APPS == task->method())
            ListApps(task);
        else if (API_GET_APP_STATUS == task->method())
            GetAppStatus(task);
        else if (API_GET_APP_INFO == task->method())
            GetAppInfo(task);
        else if (API_GET_APP_BASE_PATH == task->method())
            GetAppBasePath(task);
        else if (API_LAUNCH_VIRTUAL_APP == task->method())
            LaunchVirtualApp(task);
        else if (API_ADD_VIRTUAL_APP == task->method())
            AddVirtualApp(task);
        else if (API_REMOVE_VIRTUAL_APP == task->method())
            RemoveVirtualApp(task);
        else if (API_REGISTER_VERBS_FOR_REDIRECT == task->method())
            RegisterVerbsForRedirect(task);
        else if (API_REGISTER_VERBS_FOR_RESOURCE == task->method())
            RegisterVerbsForResource(task);
        else if (API_GET_HANDLER_FOR_EXTENSION == task->method())
            GetHandlerForExtension(task);
        else if (API_LIST_EXTENSION_MAP == task->method())
            ListExtensionMap(task);
        else if (API_MIME_TYPE_FOR_EXTENSION == task->method())
            MimeTypeForExtension(task);
        else if (API_GET_HANDLER_FOR_MIME_TYPE == task->method())
            GetHandlerForMimeType(task);
        else if (API_GET_HANDLER_FOR_MIME_TYPE_BY_VERB == task->method())
            GetHandlerForMimeTypeByVerb(task);
        else if (API_GET_HANDLER_FOR_URL == task->method())
            GetHandlerForUrl(task);
        else if (API_GET_HANDLER_FOR_URL_BY_VERB == task->method())
            GetHandlerForUrlByVerb(task);
        else if (API_LIST_ALL_HANDLERS_FOR_MIME == task->method())
            ListAllHandlersForMime(task);
        else if (API_LIST_ALL_HANDLERS_FOR_MIME_BY_VERB == task->method())
            ListAllHandlersForMimeByVerb(task);
        else if (API_LIST_ALL_HANDLERS_FOR_URL == task->method())
            ListAllHandlersForUrl(task);
        else if (API_LIST_ALL_HANDLERS_FOR_URL_BY_VERB == task->method())
            ListAllHandlersForUrlByVerb(task);
        else if (API_LIST_ALL_HANDLERS_FOR_URL_PATTERN == task->method())
            ListAllHandlersForUrlPattern(task);
        else if (API_LIST_ALL_HANDLERS_FOR_MULTIPLE_MIME == task->method())
            ListAllHandlersForMultipleMime(task);
        else if (API_LIST_ALL_HANDLERS_FOR_MULTIPLE_URL_PATTERN == task->method())
            ListAllHandlersForMultipleUrlPattern(task);
    } else if (API_CATEGORY_DEV == task->category()) {
        if (API_LIST_APPS == task->method())
            ListAppsForDev(task);
    }
}

void PackageLunaAdapter::ListApps(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();
    const std::map<std::string, AppDescPtr>& apps = ApplicationManager::instance().allApps();

    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue apps_info = pbnjson::Array();

    for (auto it : apps)
        apps_info.append(it.second->toJValue());

    bool is_full_list_client = true;
    if (jmsg.hasKey("properties") && jmsg["properties"].isArray()) {
        is_full_list_client = false;
        pbnjson::JValue apps_selected_info = pbnjson::Array();
        jmsg["properties"].append("id"); // id is required
        (void) ApplicationDescription::getSelectedPropsFromApps(apps_info, jmsg["properties"], apps_selected_info);
        payload.put("apps", apps_selected_info);
    } else {
        payload.put("apps", apps_info);
    }

    if (LSMessageIsSubscription(task->lsmsg())) {
        payload.put("subscribed", LSSubscriptionAdd(task->lshandle(), (is_full_list_client ? SUBSKEY_LIST_APPS : SUBSKEY_LIST_APPS_COMPACT), task->lsmsg(), NULL));
    } else {
        payload.put("subscribed", false);
    }

    task->ReplyResult(payload);
}

void PackageLunaAdapter::ListAppsForDev(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();
    const std::map<std::string, AppDescPtr>& apps = ApplicationManager::instance().allApps();

    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue apps_info = pbnjson::Array();

    for (auto it : apps) {
        if (AppTypeByDir::Dev == it.second->getTypeByDir()) {
            apps_info.append(it.second->toJValue());
        }
    }

    payload.put("returnValue", true);

    bool is_full_list_client = true;
    if (jmsg.hasKey("properties") && jmsg["properties"].isArray()) {
        is_full_list_client = false;
        pbnjson::JValue apps_selected_info = pbnjson::Array();
        jmsg["properties"].append("id"); // id is required
        (void) ApplicationDescription::getSelectedPropsFromApps(apps_info, jmsg["properties"], apps_selected_info);
        payload.put("apps", apps_selected_info);
    } else {
        payload.put("apps", apps_info);
    }

    if (LSMessageIsSubscription(task->lsmsg())) {
        payload.put("subscribed", LSSubscriptionAdd(task->lshandle(), (is_full_list_client ? SUBSKEY_DEV_LIST_APPS : SUBSKEY_DEV_LIST_APPS_COMPACT), task->lsmsg(), NULL));
    } else {
        payload.put("subscribed", false);
    }

    task->ReplyResult(payload);
}

void PackageLunaAdapter::GetAppStatus(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    pbnjson::JValue payload = pbnjson::Object();
    std::string app_id = jmsg["appId"].asString();
    bool require_app_info = (jmsg.hasKey("appInfo") && jmsg["appInfo"].isBoolean() && jmsg["appInfo"].asBool()) ? true : false;

    if (app_id.empty()) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified");
        return;
    }

    if (LSMessageIsSubscription(task->lsmsg())) {
        std::string subs_key = "getappstatus#" + app_id + "#" + (require_app_info ? "Y" : "N");
        if (LSSubscriptionAdd(task->lshandle(), subs_key.c_str(), task->lsmsg(), NULL)) {
            payload.put("subscribed", true);
        } else {
            LOG_WARNING(MSGID_LUNA_SUBSCRIPTION, 1, PMLOGKS("method", "getAppStatus"), "trace (%s:%d)", __FUNCTION__, __LINE__);
            payload.put("subscribed", false);
        }
    }

    // for first return (event: nothing)
    payload.put("appId", app_id);
    payload.put("event", "nothing");

    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if (!app_desc) {
        payload.put("status", "notExist");
        payload.put("exist", false);
        payload.put("launchable", false);
    } else {
        payload.put("exist", true);

        if (app_desc->canExecute()) {
            payload.put("status", "launchable");
            payload.put("launchable", true);
        } else {
            payload.put("status", "updating");
            payload.put("launchable", false);
        }

        if (require_app_info) {
            payload.put("appInfo", app_desc->toJValue());
        }
    }

    task->ReplyResult(payload);
}

void PackageLunaAdapter::GetAppInfo(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    pbnjson::JValue payload = pbnjson::Object();
    pbnjson::JValue app_info = pbnjson::Object();
    std::string app_id = jmsg["id"].asString();

    if (app_id.empty()) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified");
        return;
    }

    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if (!app_desc) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified OR Unsupported Application Type: " + app_id);
        return;
    }

    if (jmsg.hasKey("properties") && jmsg["properties"].isArray()) {
        const pbnjson::JValue& origin_app = app_desc->toJValue();
        if (!ApplicationDescription::getSelectedPropsFromAppInfo(origin_app, jmsg["properties"], app_info)) {
            task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Fail to get selected properties from AppInfo: " + app_id);
            return;
        }
    } else {
        app_info = app_desc->toJValue();
    }

    payload.put("appInfo", app_info);
    payload.put("appId", app_id);
    task->ReplyResult(payload);
}

void PackageLunaAdapter::GetAppBasePath(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    pbnjson::JValue payload = pbnjson::Object();
    std::string app_id = jmsg["appId"].asString();

    if (app_id.empty()) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified");
        return;
    }
    if (task->caller() != app_id) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Not allowed. Allow only for the info of calling app itself.");
        return;
    }

    AppDescPtr app_desc = ApplicationManager::instance().getAppById(app_id);
    if (!app_desc) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Invalid appId specified: " + app_id);
        return;
    }

    payload.put("appId", app_id);
    payload.put("basePath", app_desc->entryPoint());

    task->ReplyResult(payload);
}

void PackageLunaAdapter::LaunchVirtualApp(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();
    std::string app_id = !jmsg.hasKey("id") || !jmsg["id"].isString() ? "" : jmsg["id"].asString();

    LOG_NORMAL(NLID_APP_LAUNCH_BEGIN, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("caller_id", task->caller().c_str()), PMLOGKS("mode", "virtual"), "");

    VirtualAppManager::instance().InstallTmpVirtualAppOnLaunch(jmsg, task->lsmsg());
}

void PackageLunaAdapter::AddVirtualApp(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();
    std::string app_id = !jmsg.hasKey("id") || !jmsg["id"].isString() ? "" : jmsg["id"].asString();

    LOG_INFO(MSGID_ADD_VIRTUAL_APP, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "received_request"), PMLOGKS("caller_id", task->caller().c_str()), "");

    VirtualAppManager::instance().InstallVirtualApp(jmsg, task->lsmsg());
}

void PackageLunaAdapter::RemoveVirtualApp(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();
    std::string app_id = !jmsg.hasKey("id") || !jmsg["id"].isString() ? "" : jmsg["id"].asString();

    LOG_INFO(MSGID_REMOVE_VIRTUAL_APP, 3, PMLOGKS("app_id", app_id.c_str()), PMLOGKS("status", "received_request"), PMLOGKS("caller_id", task->caller().c_str()), "");

    VirtualAppManager::instance().UninstallVirtualApp(jmsg, task->lsmsg());
}

void PackageLunaAdapter::RegisterVerbsForRedirect(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string errorText;
    std::string appid;
    std::string mime;
    std::string url;
    std::map<std::string, std::string> verbs;

    // {"appId": string, "mime": string or "url": string }
    if (jmsg["appId"].asString(appid) != CONV_OK) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Missing appId parameter");
        return;
    }

    jmsg["mime"].asString(mime);
    jmsg["url"].asString(url);

    if ((url.empty()) && (mime.empty())) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Missing either the mime or the url parameter");
        return;
    }

    //grab the verbs
    //...even though this isn't a "handler entry" per-se, the verbs parameter is formatted the same way,
    //   so the MimeSystem::extractVerbsFromHandlerEntryJson() function doesn't care
    if (MimeSystem::extractVerbsFromHandlerEntryJson(jmsg, verbs) == 0) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "No verbs specified/invalid verb list format");
        return;
    }

    if (mime.empty() == false) {
        if (MimeSystemImpl::instance().addVerbsToResourceHandler(mime, appid, verbs) == 0) {
            task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("Adding verbs to APPID = ") + appid + std::string(" for MIMETYPE = ") + mime + std::string(" failed"));
            return;
        }
    } else {
        if (MimeSystemImpl::instance().addVerbsToRedirectHandler(url, appid, verbs) == 0) {
            task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("Adding verbs to APPID = ") + appid + std::string(" for URL = ") + url + std::string(" failed"));
            return;
        }
    }

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("subscribed", false);
    reply.put("returnValue", true);

    MimeSystemImpl::instance().saveMimeTableToActiveFile(errorText);    //ignore errors

    task->ReplyResult(reply);
}

void PackageLunaAdapter::RegisterVerbsForResource(LunaTaskPtr task)
{
    RegisterVerbsForRedirect(task);
}

void PackageLunaAdapter::GetHandlerForExtension(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string extension;
    std::string mime;
    ResourceHandler rsrcHandler;

    // {"extension": string}
    if (jmsg["extension"].asString(extension) != CONV_OK) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Missing extension parameter");
        return;
    }
    //map mime to extension
    else if (MimeSystemImpl::instance().getMimeTypeByExtension(extension, mime) == false) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("No mime type mapped to extension ") + extension);
        return;
    }

    rsrcHandler = MimeSystemImpl::instance().getActiveHandlerForResource(mime);

    if (rsrcHandler.valid() == false) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "No handler mapped to this mimeType");
        return;
    }

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("subscribed", false);

    reply.put("returnValue", true);
    reply.put("mimeType", mime);
    reply.put("appId", rsrcHandler.appId());
    reply.put("download", !(rsrcHandler.stream()));

    task->ReplyResult(reply);
}

void PackageLunaAdapter::ListExtensionMap(LunaTaskPtr task)
{
    // {}
    pbnjson::JValue reply = pbnjson::Object();
    reply.put("subscribed", false);
    reply.put("returnValue", true);
    reply.put("extensionMap", MimeSystemImpl::instance().extensionMapAsJsonArray());

    task->ReplyResult(reply);
}

void PackageLunaAdapter::MimeTypeForExtension(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string mime;
    std::string extension;

    // {"extension": string}
    if (jmsg["extension"].asString(extension) != CONV_OK) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Missing extension parameter");
        return;
    } else if (MimeSystemImpl::instance().getMimeTypeByExtension(extension, mime) == false) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "No mime mapped to this extension");
        return;
    }

    pbnjson::JValue reply = pbnjson::Object();
    //TODO: Need to remove this (subscribed parameter) after clarification. Do we need this at all.
    reply.put("subscribed", false);
    reply.put("returnValue", true);
    reply.put("mimeType", mime);
    reply.put("extension", extension);

    task->ReplyResult(reply);
}

void PackageLunaAdapter::GetHandlerForMimeType(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string mime;
    ResourceHandler rsrcHandler;

    // {"mimeType": string}
    if (jmsg["mimeType"].asString(mime) != CONV_OK) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Missing mimeType parameter");
        return;
    }

    rsrcHandler = MimeSystemImpl::instance().getActiveHandlerForResource(mime);

    if (rsrcHandler.valid() == false) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "No handler mapped to this mimeType");
        return;
    }

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("subscribed", false);
    reply.put("returnValue", true);
    reply.put("mimeType", mime);
    reply.put("appId", rsrcHandler.appId());
    reply.put("download", !(rsrcHandler.stream()));

    task->ReplyResult(reply);
}

void PackageLunaAdapter::GetHandlerForMimeTypeByVerb(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string url;
    std::string mime;
    std::string verb;
    ResourceHandler rsrcHandler;
    RedirectHandler redirHandler;

    // {"url": string or "mime": string, "verb": string}
    if (jmsg["url"].asString(url) != CONV_OK && (jmsg["mime"].asString(mime) != CONV_OK)) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Must have either an url or a mime parameter");
        return;
    } else if (jmsg["verb"].asString(verb) != CONV_OK) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Missing verb parameter");
        return;
    } else if (!url.empty()) {
        //validate it
        if (!isUrlValid(url)) {
            task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Invalid URI format");
            return;
        } else {
            //try the redirect forms first
            redirHandler = MimeSystemImpl::instance().getHandlerByVerbForRedirect(url, true, verb);
            if (redirHandler.valid() == false) {
                //try the scheme forms
                redirHandler = MimeSystemImpl::instance().getHandlerByVerbForRedirect(url, false, verb);
                if (redirHandler.valid() == false) {
                    //none found
                    task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("No redirect handler found for ") + url);
                    return;
                }
            }
        }
    } else {
        //must be mime
        rsrcHandler = MimeSystemImpl::instance().getHandlerByVerbForResource(mime, verb);
        if (rsrcHandler.valid() == false) {
            task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("No resource handler found for ") + mime);
            return;
        }
    }

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("subscribed", false);
    reply.put("returnValue", true);
    if (rsrcHandler.valid()) {
        reply.put("mimeType", mime);
        reply.put("appId", rsrcHandler.appId());
        reply.put("download", !(rsrcHandler.stream()));
        reply.put("index", rsrcHandler.index());
    } else {
        reply.put("appId", redirHandler.appId());
        reply.put("download", false);
        reply.put("index", redirHandler.index());
    }

    task->ReplyResult(reply);
}

void PackageLunaAdapter::GetHandlerForUrl(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string url;
    std::string mime;
    std::string extension;

    ResourceHandler rsrcHandler;
    RedirectHandler redirHandler;

    // {"url": string}
    if (jmsg["url"].asString(url) != CONV_OK) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Missing url parameter");
        return;
    }

    if (!isUrlValid(url)) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Invalid URI format");
        return;
    }

    //try and find a redirect handler for it...disallow scheme forms of url matching (i.e. matching only on http: , https:, etc)
    redirHandler = MimeSystemImpl::instance().getActiveHandlerForRedirect(url, false, true);
    if (!redirHandler.valid()) {
        //this is what I'll answer with
        //try and extract the extension
        if (getExtensionFromUrl(url, extension)) {
            //map mime to extension
            if (MimeSystemImpl::instance().getMimeTypeByExtension(extension, mime)) {
                rsrcHandler = MimeSystemImpl::instance().getActiveHandlerForResource(mime);
                //answer with this
            }
        }

        if (!rsrcHandler.valid()) {
            //finally, try to find a scheme form of a redirect handler
            redirHandler = MimeSystemImpl::instance().getActiveHandlerForRedirect(url, false, false);

            if (!redirHandler.valid()) {
                task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("No handler found for url [") + url + "]");
                return;
            }
        }
    }

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("subscribed", false);

    reply.put("returnValue", true);
    if (rsrcHandler.valid()) {
        reply.put("mimeType", mime);
        reply.put("appId", rsrcHandler.appId());
        reply.put("download", !(rsrcHandler.stream()));
    } else {
        reply.put("appId", redirHandler.appId());
        reply.put("download", false);
    }

    task->ReplyResult(reply);
}

void PackageLunaAdapter::GetHandlerForUrlByVerb(LunaTaskPtr task)
{
    GetHandlerForMimeTypeByVerb(task);
}

void PackageLunaAdapter::ListAllHandlersForMime(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string mime;
    ResourceHandler activeRsrcHandler;
    std::vector<ResourceHandler> rsrcHandlers;

    // {"mime": string}
    if (jmsg["mime"].asString(mime) != CONV_OK) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Must have a mime parameter");
        return;
    }

    //must be mime
    if (MimeSystemImpl::instance().getAllHandlersForResource(mime, activeRsrcHandler, rsrcHandlers) == 0) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("No handlers found for ") + mime);
        return;
    } else if (activeRsrcHandler.valid() == false) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("Error getting handlers for ") + mime);
        return;
    }

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("subscribed", false);

    if (!mime.empty()) {
        reply.put("mime", mime);
    }

    pbnjson::JValue jinnerobj = pbnjson::Object();
    reply.put("returnValue", true);

    pbnjson::JValue handlerJobj = activeRsrcHandler.toJValue();
    AppDescPtr appDesc = ApplicationManager::instance().getAppById(activeRsrcHandler.appId());
    if (appDesc != NULL) {
        handlerJobj.put("appName", appDesc->title());
    }
    jinnerobj.put("activeHandler", handlerJobj);
    if (!rsrcHandlers.empty()) {
        pbnjson::JValue jarray = pbnjson::Array();
        for (auto it = rsrcHandlers.begin(); it != rsrcHandlers.end(); ++it) {
            handlerJobj = (*it).toJValue();
            appDesc = ApplicationManager::instance().getAppById((*it).appId());
            if (appDesc != NULL) {
                handlerJobj.put("appName", appDesc->title());
            }
            jarray.append(handlerJobj);
        }
        jinnerobj.put("alternates", jarray);
    }
    reply.put("resourceHandlers", jinnerobj);

    task->ReplyResult(reply);
}

void PackageLunaAdapter::ListAllHandlersForMimeByVerb(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string url;
    std::string mime;
    std::string verb;
    std::vector<RedirectHandler> redirHandlers;
    std::vector<ResourceHandler> rsrcHandlers;

    // {"verb": string, "url": string or "mime": string}
    if (!jmsg.hasKey("verb")) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Missing 'verb' array parameter");
        return;
    } else if (!jmsg.hasKey("url") && !jmsg.hasKey("mime")) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Must have either an 'url'' or a 'mime' parameter");
        return;
    }

    jmsg["verb"].asString(verb);
    jmsg["url"].asString(url);
    jmsg["mime"].asString(mime);

    if (!url.empty()) {
        //validate it
        if (!isUrlValid(url)) {
            task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Invalid URI format");
            return;
        } else if (MimeSystemImpl::instance().getAllHandlersByVerbForRedirect(url, verb, redirHandlers) == 0) {
            task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("No handlers found for ") + url);
            return;
        }
    } else {
        //must be mime
        if (MimeSystemImpl::instance().getAllHandlersByVerbForResource(mime, verb, rsrcHandlers) == 0) {
            task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("No handlers found for ") + mime);
            return;
        }
    }

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("subscribed", false);

    if (!url.empty()) {
        reply.put("url", url);
    }
    if (!mime.empty()) {
        reply.put("mime", mime);
    }
    if (!verb.empty()) {
        reply.put("verb", verb);
    }

    reply.put("returnValue", true);
    pbnjson::JValue jinnerobj = pbnjson::Object();
    if (!rsrcHandlers.empty()) {
        ///mime handlers
        pbnjson::JValue jarray = pbnjson::Array();
        for (auto it = rsrcHandlers.begin(); it != rsrcHandlers.end(); ++it) {
            jarray.append((*it).toJValue());
        }
        jinnerobj.put("alternates", jarray);
        reply.put("resourceHandlers", jinnerobj);
    } else {
        ///redirect handlers
        pbnjson::JValue jarray = pbnjson::Array();
        for (auto it = redirHandlers.begin(); it != redirHandlers.end(); ++it) {
            jarray.append((*it).toJValue());
        }
        jinnerobj.put("alternates", jarray);
        reply.put("redirectHandlers", jinnerobj);
    }

    task->ReplyResult(reply);
}

void PackageLunaAdapter::ListAllHandlersForUrl(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string url;
    RedirectHandler activeRedirHandler;
    std::vector<RedirectHandler> redirHandlers;

    // {"url": string}
    if (jmsg["url"].asString(url) != CONV_OK) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Must have an url parameter");
        return;
    } else if (!url.empty()) {
        //validate it
        if (!isUrlValid(url)) {
            task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Invalid URI format");
            return;
        } else if (MimeSystemImpl::instance().getAllHandlersForRedirect(url, false, activeRedirHandler, redirHandlers) == 0) {
            task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("No handlers found for ") + url);
            return;
        } else if (activeRedirHandler.valid() == false) {
            task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("Error getting handlers for ") + url);
            return;
        }
    }

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("subscribed", false);

    if (!url.empty()) {
        reply.put("url", url);
    }

    pbnjson::JValue jinnerobj = pbnjson::Object();
    reply.put("returnValue", true);

    //redirect handlers were found
    pbnjson::JValue handlerJobj = activeRedirHandler.toJValue();
    AppDescPtr appDesc = ApplicationManager::instance().getAppById(activeRedirHandler.appId());
    if (appDesc != NULL) {
        handlerJobj.put("appName", appDesc->title());
    }
    jinnerobj.put("activeHandler", handlerJobj);
    if (redirHandlers.size() > 0) {
        pbnjson::JValue jarray = pbnjson::Array();
        for (auto it = redirHandlers.begin(); it != redirHandlers.end(); ++it) {
            handlerJobj = (*it).toJValue();
            appDesc = ApplicationManager::instance().getAppById((*it).appId());
            if (appDesc != NULL) {
                handlerJobj.put("appName", appDesc->title());
            }
            jarray.append(handlerJobj);
        }
        jinnerobj.put("alternates", jarray);
    }
    reply.put("redirectHandlers", jinnerobj);

    task->ReplyResult(reply);
}

void PackageLunaAdapter::ListAllHandlersForUrlByVerb(LunaTaskPtr task)
{
    ListAllHandlersForMimeByVerb(task);
}

void PackageLunaAdapter::ListAllHandlersForUrlPattern(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::string url;
    RedirectHandler activeRedirHandler;
    ResourceHandler activeRsrcHandler;
    std::vector<RedirectHandler> redirHandlers;
    std::vector<ResourceHandler> rsrcHandlers;

    // {"urlPattern": url}
    if (!jmsg.hasKey("urlPattern") || jmsg["urlPattern"].asString(url) != CONV_OK) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Missing 'urlPattern' parameter");
        return;
    } else if (MimeSystemImpl::instance().getAllHandlersForRedirect(url, true, activeRedirHandler, redirHandlers) == 0) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("No handlers found for ") + url);
        return;
    } else if (activeRedirHandler.valid() == false) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, std::string("Error getting handlers for ") + url);
        return;
    }

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("subscribed", false);

    if (!url.empty()) {
        reply.put("urlPattern", url);
    }

    pbnjson::JValue jinnerobj = pbnjson::Object();
    reply.put("returnValue", true);
    if (activeRsrcHandler.valid()) {
        jinnerobj.put("activeHandler", activeRsrcHandler.toJValue());
        if (rsrcHandlers.size() > 0) {
            pbnjson::JValue jarray = pbnjson::Array();
            for (auto it = rsrcHandlers.begin(); it != rsrcHandlers.end(); ++it) {
                jarray.append((*it).toJValue());
            }
            jinnerobj.put("alternates", jarray);
        }
        reply.put("resourceHandlers", jinnerobj);
    } else {
        //redirect handlers were found
        jinnerobj.put("activeHandler", activeRedirHandler.toJValue());
        if (redirHandlers.size() > 0) {
            pbnjson::JValue jarray = pbnjson::Array();
            for (auto it = redirHandlers.begin(); it != redirHandlers.end(); ++it) {
                jarray.append((*it).toJValue());
            }
            jinnerobj.put("alternates", jarray);
        }
        reply.put("redirectHandlers", jinnerobj);
    }

    task->ReplyResult(reply);
}

void PackageLunaAdapter::ListAllHandlersForMultipleMime(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::map<std::string, ResourceHandler> activeHandlersMap;
    std::map<std::string, std::vector<ResourceHandler> > altHandlersMap;

    // {"mimes": array}
    if (!jmsg.hasKey("mimes")) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Missing 'mimes' array parameter");
        return;
    }

    //extract the mimes array:
    //  "mimes":["mime1","mime2",...]
    pbnjson::JValue mimeArray = jmsg["mimes"];
    if (mimeArray.arraySize() <= 0) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Bad 'mimes' array parameter specified");
        return;
    }

    for (int i = 0; i < mimeArray.arraySize(); i++) {
        pbnjson::JValue obj = mimeArray[i];
        std::string mime;
        if (obj.asString(mime) == CONV_OK && !mime.empty()) {
            ResourceHandler active;
            std::vector<ResourceHandler> alts;
            if (MimeSystemImpl::instance().getAllHandlersForResource(mime, active, alts) != 0) {
                activeHandlersMap[mime] = active;
                altHandlersMap[mime] = alts;
            }
        }
    }

    if (activeHandlersMap.empty()) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "No handlers found");
        return;
    }

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("subscribed", false);

    reply.put("returnValue", true);
    for (auto active_map_it = activeHandlersMap.begin(); active_map_it != activeHandlersMap.end(); ++active_map_it) {
        pbnjson::JValue jinnerobj = pbnjson::Object();
        pbnjson::JValue handlerJobj = active_map_it->second.toJValue();
        AppDescPtr appDesc = ApplicationManager::instance().getAppById(active_map_it->second.appId());
        if (appDesc != NULL) {
            handlerJobj.put("appName", appDesc->title());
        }
        jinnerobj.put("activeHandler", handlerJobj);
        auto alt_map_it = altHandlersMap.find(active_map_it->first);
        if (alt_map_it != altHandlersMap.end()) {
            if (alt_map_it->second.size() > 0) {
                pbnjson::JValue jarray = pbnjson::Array();
                for (auto it = alt_map_it->second.begin(); it != alt_map_it->second.end(); ++it) {
                    handlerJobj = (*it).toJValue();
                    appDesc = ApplicationManager::instance().getAppById((*it).appId());
                    if (appDesc != NULL) {
                        handlerJobj.put("appName", appDesc->title());
                    }
                    jarray.append(handlerJobj);
                }
                jinnerobj.put("alternates", jarray);
            }
            reply.put(active_map_it->first, jinnerobj);
        }
    } //end activeHandlersMap for loop

    task->ReplyResult(reply);
}

void PackageLunaAdapter::ListAllHandlersForMultipleUrlPattern(LunaTaskPtr task)
{
    const pbnjson::JValue& jmsg = task->jmsg();

    std::map<std::string, RedirectHandler> activeHandlersMap;
    std::map<std::string, std::vector<RedirectHandler> > altHandlersMap;

    // {"urls": array}
    if (!jmsg.hasKey("urls")) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Missing 'urls' array parameter");
        return;
    } else if (jmsg["urls"].arraySize() <= 0) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "Bad 'urls' array parameter specified");
        return;
    }

    pbnjson::JValue urlArray = jmsg["urls"];

    for (ssize_t i = 0; i < urlArray.arraySize(); i++) {
        pbnjson::JValue obj = urlArray[i];
        std::string url;
        obj.asString(url);
        if (obj.asString(url) == CONV_OK && url.length()) {
            RedirectHandler active;
            std::vector<RedirectHandler> alts;
            if (MimeSystemImpl::instance().getAllHandlersForRedirect(url, true, active, alts) != 0) {
                activeHandlersMap[url] = active;
                altHandlersMap[url] = alts;
            }
        }
    }

    if (activeHandlersMap.empty()) {
        task->ReplyResultWithError(API_ERR_CODE_GENERAL, "No handlers found");
        return;
    }

    pbnjson::JValue reply = pbnjson::Object();
    reply.put("subscribed", false);

    reply.put("returnValue", true);
    AppDescPtr appDesc;
    for (auto active_map_it = activeHandlersMap.begin(); active_map_it != activeHandlersMap.end(); ++active_map_it) {
        pbnjson::JValue jinnerobj = pbnjson::Object();
        pbnjson::JValue handlerJobj = active_map_it->second.toJValue();
        appDesc = ApplicationManager::instance().getAppById(active_map_it->second.appId());
        if (appDesc != NULL) {
            handlerJobj.put("appName", appDesc->title());
        }
        jinnerobj.put("activeHandler", handlerJobj);
        auto alt_map_it = altHandlersMap.find(active_map_it->first);
        if (alt_map_it != altHandlersMap.end()) {
            if (alt_map_it->second.size() > 0) {
                pbnjson::JValue jarray = pbnjson::Array();
                for (auto it = alt_map_it->second.begin(); it != alt_map_it->second.end(); ++it) {
                    handlerJobj = (*it).toJValue();
                    appDesc = ApplicationManager::instance().getAppById((*it).appId());
                    if (appDesc != NULL) {
                        handlerJobj.put("appName", appDesc->title());
                    }
                    jarray.append(handlerJobj);
                }
                jinnerobj.put("alternates", jarray);
            }
            reply.put(active_map_it->first, jinnerobj);
        }
    } //end activeHandlersMap for loop

    task->ReplyResult(reply);
}

void PackageLunaAdapter::OnListAppsChanged(const pbnjson::JValue& apps, const std::vector<std::string>& changes, bool dev)
{

    std::string subs_key = dev ? SUBSKEY_DEV_LIST_APPS : SUBSKEY_LIST_APPS;
    std::string subs_key4compact = dev ? SUBSKEY_DEV_LIST_APPS_COMPACT : SUBSKEY_LIST_APPS_COMPACT;
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);
    payload.put("subscribed", true);
    payload.put("apps", apps);

    // reply for clients wanted full properties
    if (!LSSubscriptionReply(AppMgrService::instance().ServiceHandle(), subs_key.c_str(), JUtil::jsonToString(payload).c_str(), NULL)) {
        LOG_WARNING(MSGID_LSCALL_ERR, 1, PMLOGKS("type", "subscription_reply"), "%s: %d", __FUNCTION__, __LINE__);
    }

    // reply for clients wanted partial properties
    LSSubscriptionIter *iter = NULL;
    if (!LSSubscriptionAcquire(AppMgrService::instance().ServiceHandle(), subs_key4compact.c_str(), &iter, NULL))
        return;

    while (LSSubscriptionHasNext(iter)) {
        LSMessage* message = LSSubscriptionNext(iter);
        pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(message), std::string("applicationManager.listApps"));
        if (jmsg.isNull())
            continue;

        // not a clients wanted partial properties
        if (!jmsg.hasKey("properties") || !jmsg["properties"].isArray()) {
            continue;
        }

        pbnjson::JValue new_apps = pbnjson::Array();

        // id is required
        jmsg["properties"].append("id");
        if (!ApplicationDescription::getSelectedPropsFromApps(apps, jmsg["properties"], new_apps)) {
            LOG_WARNING(MSGID_FAIL_GET_SELECTED_PROPS, 1, PMLOGKS("where", __FUNCTION__), "");
            continue;
        }

        payload.put("apps", new_apps);

        if (!LSMessageRespond(message, JUtil::jsonToString(payload).c_str(), NULL)) {
            LOG_ERROR(MSGID_LSCALL_ERR, 1, PMLOGKS("type", "respond"), "%s: %d", __FUNCTION__, __LINE__);
        }
    }
    LSSubscriptionRelease(iter);
    iter = NULL;
}

void PackageLunaAdapter::OnOneAppChanged(const pbnjson::JValue& app, const std::string& change, const std::string& reason, bool dev)
{

    std::string subs_key = dev ? SUBSKEY_DEV_LIST_APPS : SUBSKEY_LIST_APPS;
    std::string subs_key4compact = dev ? SUBSKEY_DEV_LIST_APPS_COMPACT : SUBSKEY_LIST_APPS_COMPACT;
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);
    payload.put("subscribed", true);
    payload.put("change", change);
    payload.put("changeReason", reason);
    payload.put("app", app);

    // reply for clients wanted full properties
    if (!LSSubscriptionReply(AppMgrService::instance().ServiceHandle(), subs_key.c_str(), JUtil::jsonToString(payload).c_str(), NULL)) {
        LOG_WARNING(MSGID_LSCALL_ERR, 1, PMLOGKS("type", "subscription_reply"), "%s: %d", __FUNCTION__, __LINE__);
    }

    // reply for clients wanted partial properties
    LSSubscriptionIter *iter = NULL;
    if (!LSSubscriptionAcquire(AppMgrService::instance().ServiceHandle(), subs_key4compact.c_str(), &iter, NULL))
        return;

    while (LSSubscriptionHasNext(iter)) {
        LSMessage* message = LSSubscriptionNext(iter);
        pbnjson::JValue jmsg = JUtil::parse(LSMessageGetPayload(message), std::string("applicationManager.listApps"));

        if (jmsg.isNull())
            continue;

        // not a clients wanted partial properties
        if (!jmsg.hasKey("properties") || !jmsg["properties"].isArray()) {
            continue;
        }

        pbnjson::JValue new_props = pbnjson::Object();
        // id is required
        jmsg["properties"].append("id");
        if (!ApplicationDescription::getSelectedPropsFromAppInfo(app, jmsg["properties"], new_props)) {
            LOG_WARNING(MSGID_FAIL_GET_SELECTED_PROPS, 1, PMLOGKS("where", __FUNCTION__), "");
            continue;
        }

        payload.put("app", new_props);

        if (!LSMessageRespond(message, JUtil::jsonToString(payload).c_str(), NULL)) {
            LOG_ERROR(MSGID_LSCALL_ERR, 1, PMLOGKS("type", "respond"), "%s: %d", __FUNCTION__, __LINE__);
        }
    }

    LSSubscriptionRelease(iter);
    iter = NULL;
}

void PackageLunaAdapter::OnAppStatusChanged(AppStatusChangeEvent event, AppDescPtr app_desc)
{

    if (!app_desc) {
        // leave error
        return;
    }

    pbnjson::JValue payload = pbnjson::Object();
    std::string str_event = ApplicationManager::instance().AppStatusChangeEventToString(event);

    switch (event) {
    case AppStatusChangeEvent::APP_INSTALLED:
    case AppStatusChangeEvent::STORAGE_ATTACHED:
    case AppStatusChangeEvent::UPDATE_COMPLETED:
        payload.put("status", "launchable");
        payload.put("exist", true);
        payload.put("launchable", true);
        break;
    case AppStatusChangeEvent::STORAGE_DETACHED:
    case AppStatusChangeEvent::APP_UNINSTALLED:
        payload.put("status", "notExist");
        payload.put("exist", false);
        payload.put("launchable", false);
        break;
    default:
        // leave error
        return;
        break;
    }

    payload.put("appId", app_desc->id());
    payload.put("event", str_event);
    payload.put("returnValue", true);

    std::string subs_key = "getappstatus#" + app_desc->id() + "#N";
    std::string subs_key_w_appinfo = "getappstatus#" + app_desc->id() + "#Y";
    std::string str_payload = JUtil::jsonToString(payload);

    switch (event) {
    case AppStatusChangeEvent::APP_INSTALLED:
    case AppStatusChangeEvent::STORAGE_ATTACHED:
    case AppStatusChangeEvent::UPDATE_COMPLETED:
        payload.put("appInfo", app_desc->toJValue());
        break;
    default:
        break;
    }

    std::string str_payload_w_appinfo = JUtil::jsonToString(payload);

    if (!LSSubscriptionReply(AppMgrService::instance().ServiceHandle(), subs_key.c_str(), str_payload.c_str(), NULL)) {

        LOG_WARNING(MSGID_SUBSCRIPTION_REPLY_ERR, 1, PMLOGKS("key", subs_key.c_str()), "trace(%s:%d)", __FUNCTION__, __LINE__);
    }

    if (!LSSubscriptionReply(AppMgrService::instance().ServiceHandle(), subs_key_w_appinfo.c_str(), str_payload_w_appinfo.c_str(), NULL)) {
        LOG_WARNING(MSGID_SUBSCRIPTION_REPLY_ERR, 1, PMLOGKS("key", subs_key_w_appinfo.c_str()), "trace(%s:%d)", __FUNCTION__, __LINE__);
    }
}
