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

#include "core/lifecycle/life_handler/nativeapp_life_handler.h"

#include <ext/stdio_filebuf.h>
#include <proc/readproc.h>
#include <boost/lexical_cast.hpp>
#include <core/package/package_manager.h>
#include <core/util/jutil.h>
#include <core/util/logging.h>
#include <core/util/lsutils.h>
#include <core/util/utils.h>

#include "core/lifecycle/app_info_manager.h"
#include "core/lifecycle/application_errors.h"
#include "core/package/application_description.h"
#include "core/setting/settings.h"

#define TIMEOUT_FOR_FORCE_KILL      1000  // 1 sec
#define TIMEOUT_FOR_NOT_RESPONDING  10000 // 10 secs
#define TIMEOUT_FOR_REGISTER_V2     3000  // 3 secs
#define TIME_LIMIT_OF_APP_LAUNCHING 3000000000u // 3 secs

static PidVector FindChildPids(const std::string& pid);
static std::string PidsToString(const PidVector& pids);
static pid_t fork_process(const char **argv, const char **envp);
static bool kill_processes(const PidVector& pids, int signame);

static NativeAppLifeHandler* g_this = NULL;

NativeAppLifeCycleInterface::NativeAppLifeCycleInterface(NativeAppLifeHandler* parent)
    : m_parent(parent)
{
}

NativeAppLifeCycleInterface::~NativeAppLifeCycleInterface()
{
}

void NativeAppLifeCycleInterface::addLaunchHandler(RuntimeStatus status, NativeAppLaunchHandler handler)
{
    m_launchHandlerMap[status] = handler;
}

void NativeAppLifeCycleInterface::launch(NativeClientInfoPtr client, AppLaunchingItemPtr item)
{
    // Check launch condition
    if (checkLaunchCondition(item) == false) {
        parent()->signal_launching_done(item->uid());
        return;
    }

    RuntimeStatus life_status = AppInfoManager::instance().runtimeStatus(item->appId());
    if (!m_launchHandlerMap.count(life_status)) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", item->appId().c_str()),
                  PMLOGKS("reason", "invalid_status_to_handle"),
                  PMLOGKS("where", __FUNCTION__), "");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS("app_id", item->appId().c_str()),
             PMLOGKS("status", "start_launch_handler"), "life_status: %d", (int )life_status);

    NativeAppLaunchHandler handler = m_launchHandlerMap[life_status];
    handler(client, item);
}

void NativeAppLifeCycleInterface::close(NativeClientInfoPtr client, AppCloseItemPtr item, std::string& err_text)
{
    closeAsPolicy(client, item, err_text);
}

void NativeAppLifeCycleInterface::pause(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& err_text, bool send_life_event)
{
    pauseAsPolicy(client, params, err_text, send_life_event);
}

void NativeAppLifeCycleInterface::launchAsCommon(NativeClientInfoPtr client, AppLaunchingItemPtr item)
{
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2, PMLOGKS("reason", "null_poiner"), PMLOGKS("where", __FUNCTION__), "");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        return;
    }

    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->AppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->InterfaceVersion());

    AppDescPtr app_desc = PackageManager::instance().getAppById(item->appId());
    if (app_desc == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "null_description"),
                  PMLOGKS("where", __FUNCTION__), "");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        parent()->signal_launching_done(item->uid());
        return;
    }

    // construct the path for launching
    std::string path = app_desc->entryPoint();
    if (path.find("file://", 0) != std::string::npos)
        path = path.substr(7);

    // [ Template method ]
    // Handle Differently based on Ver1/Ver2 policy
    std::string str_params = makeForkArguments(item, app_desc);

    const char* fork_params[10] = { 0, };
    const char* jailer_type = "";
    bool is_on_nojail_list = SettingsImpl::instance().checkAppAgainstNoJailAppList(app_desc->id());

    if (AppType::Native_AppShell == app_desc->type()) {
        fork_params[0] = SettingsImpl::instance().appshellRunnerPath.c_str();
        fork_params[1] = "--appid";
        fork_params[2] = app_desc->id().c_str();
        fork_params[3] = "--folder";
        fork_params[4] = app_desc->folderPath().c_str();
        fork_params[5] = "--params";
        fork_params[6] = str_params.c_str();
        fork_params[7] = NULL;
        LOG_INFO(MSGID_APPLAUNCH, 4,
                 PMLOGKS("appshellRunnderPath", SettingsImpl::instance().appshellRunnerPath.c_str()),
                 PMLOGKS("app_id", app_desc->id().c_str()),
                 PMLOGKS("app_folder_path", app_desc->folderPath().c_str()),
                 PMLOGJSON("params", str_params.c_str()), "launch with appshell_runner");
    } else if (AppType::Native_Qml == app_desc->type()) {
        fork_params[0] = SettingsImpl::instance().qmlRunnerPath.c_str();
        fork_params[1] = str_params.c_str();
        fork_params[2] = NULL;

        LOG_INFO(MSGID_APPLAUNCH, 3,
                 PMLOGKS("app_id", app_desc->id().c_str()),
                 PMLOGKS("app_path", path.c_str()),
                 PMLOGJSON("params", str_params.c_str()), "launch with qml_runner");
    } else if (SettingsImpl::instance().isJailMode && !is_on_nojail_list) {
        if (AppTypeByDir::Dev == app_desc->getTypeByDir()) {
            jailer_type = "native_devmode";
        } else {
            switch (app_desc->type()) {
            case AppType::Native: {
                jailer_type = "native";
                break;
            }
            case AppType::Native_Builtin: {
                jailer_type = "native_builtin";
                break;
            }
            case AppType::Native_Mvpd: {
                jailer_type = "native_mvpd";
                break;
            }
            default:
                jailer_type = "default";
                break;
            }
        }

        fork_params[0] = SettingsImpl::instance().jailerPath.c_str();
        fork_params[1] = "-t";
        fork_params[2] = jailer_type;
        fork_params[3] = "-i";
        fork_params[4] = app_desc->id().c_str();
        fork_params[5] = "-p";
        fork_params[6] = app_desc->folderPath().c_str();
        fork_params[7] = path.c_str();
        fork_params[8] = str_params.c_str();
        fork_params[9] = NULL;

        // This log shows whether native app's launched via Jailer or not
        // Do not remove this log, until jailer become stable
        LOG_INFO(MSGID_APPLAUNCH, 6,
                 PMLOGKS("jailer_path", SettingsImpl::instance().jailerPath.c_str()),
                 PMLOGKS("jailer_type", jailer_type),
                 PMLOGKS("app_id", app_desc->id().c_str()),
                 PMLOGKS("app_folder_path", app_desc->folderPath().c_str()),
                 PMLOGKS("app_path", path.c_str()), PMLOGJSON("params", str_params.c_str()), "launch with jail");
    } else {
        // This log shows whether native app's launched via Jailer or not
        // Do not remove this log, until jailer become stable
        fork_params[0] = path.c_str();
        fork_params[1] = str_params.c_str();
        fork_params[2] = NULL;

        LOG_INFO(MSGID_APPLAUNCH, 3,
                 PMLOGKS("app_id", app_desc->id().c_str()),
                 PMLOGKS("app_path", path.c_str()),
                 PMLOGJSON("params", str_params.c_str()), "launch as root");

    }

    if (item->preload().empty())
        parent()->signal_app_life_status_changed(item->appId(), item->uid(), RuntimeStatus::LAUNCHING);
    else
        parent()->signal_app_life_status_changed(item->appId(), item->uid(), RuntimeStatus::PRELOADING);

    int pid = fork_process(fork_params, NULL);

    if (pid <= 0) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("app_id", app_desc->id().c_str()),
                  PMLOGKS("path", path.c_str()), "forked_pid: %d", pid);
        parent()->signal_app_life_status_changed(item->appId(), item->uid(), RuntimeStatus::STOP);
        item->setErrCodeText(APP_ERR_NATIVE_SPAWN, "Failed to launch process");
        parent()->signal_launching_done(item->uid());
        return;
    }

    // set watcher for the child's
    g_child_watch_add(pid, (GChildWatchFunc) NativeAppLifeHandler::ChildProcessWatcher, NULL);

    double current_time = get_current_time();
    double elapsed_time = current_time - item->launchStartTime();
    LOG_INFO(MSGID_APP_LAUNCHED, 5,
             PMLOGKS("app_id", app_desc->id().c_str()),
             PMLOGKS("type", "native"),
             PMLOGKFV("pid", "%d", pid),
             PMLOGKFV("start_time", "%f", item->launchStartTime()),
             PMLOGKFV("collapse_time", "%f", elapsed_time), "");

    std::string new_pid = boost::lexical_cast<std::string>(pid);
    item->setPid(new_pid);
    client->SetPid(new_pid);
    client->StartTimerForCheckingRegistration();
    AppInfoManager::instance().setLastLaunchTime(item->appId(), current_time);
    parent()->signal_running_app_added(item->appId(), new_pid, "");
    parent()->signal_app_life_status_changed(item->appId(), item->uid(), RuntimeStatus::RUNNING);
    parent()->signal_launching_done(item->uid());
}

void NativeAppLifeCycleInterface::relaunchAsCommon(NativeClientInfoPtr client, AppLaunchingItemPtr item)
{

    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->AppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->InterfaceVersion());

    pbnjson::JValue payload = pbnjson::Object();
    makeRelaunchParams(item, payload);

    parent()->signal_app_life_status_changed(item->appId(), item->uid(), RuntimeStatus::LAUNCHING);

    if (client->SendEvent(payload) == false) {
        LOG_WARNING(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", client->AppId().c_str()), "failed_to_send_relaunch_event");
    }

    parent()->signal_launching_done(item->uid());
}

////////////////////////////////////////////////////////////////
// NativeAppLifeCycleInterface version1

NativeAppLifeCycleInterfaceVer1::NativeAppLifeCycleInterfaceVer1(NativeAppLifeHandler* parent)
    : NativeAppLifeCycleInterface(parent)
{
    addLaunchHandler(RuntimeStatus::STOP, boost::bind(&NativeAppLifeCycleInterface::launchAsCommon, this, _1, _2));

    addLaunchHandler(RuntimeStatus::RUNNING, boost::bind(&NativeAppLifeCycleInterfaceVer1::launchNotRegisteredAppAsPolicy, this, _1, _2));

    addLaunchHandler(RuntimeStatus::REGISTERED, boost::bind(&NativeAppLifeCycleInterface::relaunchAsCommon, this, _1, _2));

    addLaunchHandler(RuntimeStatus::CLOSING, boost::bind(&NativeAppLifeCycleInterfaceVer1::launchAfterClosedAsPolicy, this, _1, _2));
}

std::string NativeAppLifeCycleInterfaceVer1::makeForkArguments(AppLaunchingItemPtr item, AppDescPtr app_desc)
{
    pbnjson::JValue payload = pbnjson::Object();

    if (AppType::Native_Qml == app_desc->type()) {
        payload.put("main", app_desc->entryPoint());
        payload.put("appId", app_desc->id());
        payload.put("params", item->params());
    } else {
        payload = item->params().duplicate();
        payload.put("nid", item->appId());
        payload.put("@system_native_app", true);
        if (!item->preload().empty())
            payload.put("preload", item->preload());
    }

    return payload.stringify();
}

bool NativeAppLifeCycleInterfaceVer1::checkLaunchCondition(AppLaunchingItemPtr item)
{
    if (parent()->GetLaunchPendingItem(item->appId()) != NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", item->appId().c_str()),
                  PMLOGKS("reason", "already_in_queue"),
                  PMLOGKS("where", "launch_nativeapp"), "");
        item->setErrCodeText(APP_ERR_NATIVE_IS_LAUNCHING, item->appId() + std::string(" is already launching"));
        return false;
    }
    return true;
}

void NativeAppLifeCycleInterfaceVer1::makeRelaunchParams(AppLaunchingItemPtr item, pbnjson::JValue& payload)
{
    payload.put("message", "relaunch");
    payload.put("parameters", item->params());
    payload.put("returnValue", true);
}

void NativeAppLifeCycleInterfaceVer1::launchAfterClosedAsPolicy(NativeClientInfoPtr client, AppLaunchingItemPtr item)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->AppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->InterfaceVersion());

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", item->appId().c_str()),
             PMLOGKS("status", "app_is_closing"),
             PMLOGKS("action", "wait_until_being_closed"),
             "life_status: %d", (int )AppInfoManager::instance().runtimeStatus(item->appId()));

    parent()->AddLaunchingItemIntoPendingQ(item);
}

void NativeAppLifeCycleInterfaceVer1::launchNotRegisteredAppAsPolicy(NativeClientInfoPtr client, AppLaunchingItemPtr item)
{

    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->AppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->InterfaceVersion());

    double last_launch_time = AppInfoManager::instance().lastLaunchTime(item->appId());
    double current_time = get_current_time();
    bool preload_mode_on = AppInfoManager::instance().preloadModeOn(item->appId());

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", item->appId().c_str()),
             PMLOGKS("preload_mode_on", (preload_mode_on ? "on":"off")),
             PMLOGKS("preload_mode", (item->preload().empty() ? "empty":item->preload().c_str())),
             "in close_and_launch");

    // if less than 3 sec, just skip
    if ((current_time - last_launch_time) < TIME_LIMIT_OF_APP_LAUNCHING) {
        if (preload_mode_on && item->preload().empty()) {
            // should close and launch if currently being launched in hidden mode
        } else {
            LOG_INFO(MSGID_APPLAUNCH, 3,
                     PMLOGKS("app_id", item->appId().c_str()),
                     PMLOGKS("status", "running"),
                     PMLOGKS("action", "skip_by_launching_time"),
                     "life_cycle: %d", (int )AppInfoManager::instance().runtimeStatus(item->appId()));
            item->setPid(AppInfoManager::instance().pid(item->appId()));
            parent()->signal_launching_done(item->uid());
            return;
        }
    }

    std::string pid = AppInfoManager::instance().pid(item->appId());
    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", item->appId().c_str()),
             PMLOGKS("status", "running"),
             PMLOGKS("action", "close_and_launch"),
             "life_cycle: %d, pid: %s", (int )AppInfoManager::instance().runtimeStatus(item->appId()), pid.c_str());

    PidVector all_pids = FindChildPids(client->Pid());
    (void) parent()->SendSystemSignal(all_pids, SIGTERM);
    parent()->signal_app_life_status_changed(item->appId(), item->uid(), RuntimeStatus::CLOSING);
    parent()->StartTimerToKillApp(client->AppId(), client->Pid(), all_pids, TIMEOUT_FOR_FORCE_KILL);

    parent()->AddLaunchingItemIntoPendingQ(item);
}

void NativeAppLifeCycleInterfaceVer1::closeAsPolicy(NativeClientInfoPtr client, AppCloseItemPtr item, std::string& err_text)
{

    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->AppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->InterfaceVersion());

    RuntimeStatus current_status = AppInfoManager::instance().runtimeStatus(client->AppId());
    if (RuntimeStatus::CLOSING == current_status) {
        LOG_INFO(MSGID_APPCLOSE, 3,
                 PMLOGKS("app_id", client->AppId().c_str()),
                 PMLOGKS("pid", client->Pid().c_str()),
                 PMLOGKS("action", "wait_closing"), "already being closed");
        return;
    }

    if (client->Pid().empty()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2, PMLOGKS("reason", "empty_pid"), PMLOGKS("where", __FUNCTION__), "");
        err_text = "empty pid";
        parent()->signal_app_life_status_changed(client->AppId(), "", RuntimeStatus::STOP);
        return;
    }

    PidVector all_pids = FindChildPids(client->Pid());
    if (!parent()->SendSystemSignal(all_pids, SIGTERM)) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS("reason", "empty_pids"),
                  PMLOGKS("where", __FUNCTION__), "");
        err_text = "not found any pids to kill";
        parent()->signal_app_life_status_changed(client->AppId(), "", RuntimeStatus::STOP);
        return;
    }

    parent()->signal_app_life_status_changed(client->AppId(), "", RuntimeStatus::CLOSING);

    parent()->StartTimerToKillApp(client->AppId(), client->Pid(), all_pids, TIMEOUT_FOR_FORCE_KILL);

    LOG_INFO(MSGID_APPCLOSE, 3,
             PMLOGKS("app_id", client->AppId().c_str()),
             PMLOGKS("pid", client->Pid().c_str()),
             PMLOGKS("action", "sigterm"), "");
}

void NativeAppLifeCycleInterfaceVer1::pauseAsPolicy(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& err_text, bool send_life_event)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->AppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->InterfaceVersion());

    LOG_INFO(MSGID_APPCLOSE, 3,
             PMLOGKS("app_id", client->AppId().c_str()),
             PMLOGKS("pid", client->Pid().c_str()),
             PMLOGKS("action", "sigterm_to_pause"), "");

    PidVector all_pids = FindChildPids(client->Pid());
    (void) parent()->SendSystemSignal(all_pids, SIGTERM);
    if (send_life_event)
        parent()->signal_app_life_status_changed(client->AppId(), "", RuntimeStatus::CLOSING);
    parent()->StartTimerToKillApp(client->AppId(), client->Pid(), all_pids, TIMEOUT_FOR_FORCE_KILL);
}

NativeAppLifeCycleInterfaceVer2::NativeAppLifeCycleInterfaceVer2(NativeAppLifeHandler* parent) :
        NativeAppLifeCycleInterface(parent)
{
    addLaunchHandler(RuntimeStatus::STOP, boost::bind(&NativeAppLifeCycleInterface::launchAsCommon, this, _1, _2));

    addLaunchHandler(RuntimeStatus::RUNNING, boost::bind(&NativeAppLifeCycleInterfaceVer2::launchNotRegisteredAppAsPolicy, this, _1, _2));

    addLaunchHandler(RuntimeStatus::REGISTERED, boost::bind(&NativeAppLifeCycleInterface::relaunchAsCommon, this, _1, _2));

    addLaunchHandler(RuntimeStatus::CLOSING, boost::bind(&NativeAppLifeCycleInterfaceVer2::launchAfterClosedAsPolicy, this, _1, _2));
}

std::string NativeAppLifeCycleInterfaceVer2::makeForkArguments(AppLaunchingItemPtr item, AppDescPtr app_desc)
{
    pbnjson::JValue payload = pbnjson::Object();
    payload.put("event", "launch");
    payload.put("reason", item->launchReason());
    payload.put("appId", item->appId());
    payload.put("interfaceVersion", 2);
    payload.put("interfaceMethod", "registerApp");
    payload.put("parameters", item->params());
    payload.put("@system_native_app", true);

    if (!item->preload().empty())
        payload.put("preload", item->preload());

    if (AppType::Native_Qml == app_desc->type())
        payload.put("main", app_desc->entryPoint());

    return payload.stringify();
}

bool NativeAppLifeCycleInterfaceVer2::checkLaunchCondition(AppLaunchingItemPtr item)
{
    return true;
}

void NativeAppLifeCycleInterfaceVer2::makeRelaunchParams(AppLaunchingItemPtr item, pbnjson::JValue& payload)
{
    payload.put("event", "relaunch");
    payload.put("reason", item->launchReason());
    payload.put("appId", item->appId());
    payload.put("parameters", item->params());
    payload.put("returnValue", true);
}

void NativeAppLifeCycleInterfaceVer2::launchAfterClosedAsPolicy(NativeClientInfoPtr client, AppLaunchingItemPtr item)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->AppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->InterfaceVersion());

    LOG_INFO(MSGID_APPLAUNCH, 3,
             PMLOGKS("app_id", item->appId().c_str()),
             PMLOGKS("status", "app_is_closing"),
             PMLOGKS("action", "wait_until_being_closed"),
             "life_status: %d", (int )AppInfoManager::instance().runtimeStatus(item->appId()));

    parent()->AddLaunchingItemIntoPendingQ(item);
}

void NativeAppLifeCycleInterfaceVer2::launchNotRegisteredAppAsPolicy(NativeClientInfoPtr client, AppLaunchingItemPtr item)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2, PMLOGKS("app_id", client->AppId().c_str()), PMLOGKS("start_native_handler", __FUNCTION__), "ver: %d", client->InterfaceVersion());

    if (client->IsRegistrationExpired()) {
        // clean up previous launch reqeust from pending queue
        parent()->CancelLaunchPendingItemAndMakeItDone(client->AppId());

        parent()->signal_app_life_status_changed(item->appId(), item->uid(), RuntimeStatus::CLOSING);

        (void) parent()->FindPidsAndSendSystemSignal(client->Pid(), SIGKILL);
    }

    parent()->AddLaunchingItemIntoPendingQ(item);
}

void NativeAppLifeCycleInterfaceVer2::closeAsPolicy(NativeClientInfoPtr client, AppCloseItemPtr item, std::string& err_text)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->AppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->InterfaceVersion());

    parent()->signal_app_life_status_changed(client->AppId(), "", RuntimeStatus::CLOSING);

    // if registered
    if (client->IsRegistered()) {
        pbnjson::JValue payload = pbnjson::Object();
        payload.put("event", "close");
        payload.put("reason", item->getReason());
        payload.put("returnValue", true);

        if (client->SendEvent(payload) == false) {
            LOG_WARNING(MSGID_APPCLOSE_ERR, 1,
                        PMLOGKS("app_id", client->AppId().c_str()),
                        "failed_to_send_close_event");
        }

        PidVector all_pids = FindChildPids(client->Pid());
        if (item->IsMemoryReclaim()) {
            //start force kill timer()
            parent()->StartTimerToKillApp(client->AppId(), client->Pid(), all_pids, TIMEOUT_FOR_FORCE_KILL);
        } else {
            parent()->StartTimerToKillApp(client->AppId(), client->Pid(), all_pids, TIMEOUT_FOR_NOT_RESPONDING);
        }
        // if not registered
    } else {
        //  clean up pending queue
        parent()->CancelLaunchPendingItemAndMakeItDone(client->AppId());

        // send sigkill
        PidVector all_pids = FindChildPids(client->Pid());
        (void) parent()->SendSystemSignal(all_pids, SIGKILL);
    }
}

void NativeAppLifeCycleInterfaceVer2::pauseAsPolicy(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& err_text, bool send_life_event)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->AppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->InterfaceVersion());

    if (client->IsRegistered()) {
        pbnjson::JValue payload = pbnjson::Object();
        payload.put("event", "pause");
        payload.put("reason", "keepAlive");
        payload.put("parameters", params);
        payload.put("returnValue", true);

        if (send_life_event)
            parent()->signal_app_life_status_changed(client->AppId(), "", RuntimeStatus::PAUSING);

        if (client->SendEvent(payload) == false) {
            LOG_WARNING(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", client->AppId().c_str()), "failed_to_send_pause_event");
        }
    } else {
        // clean up pending queue
        parent()->CancelLaunchPendingItemAndMakeItDone(client->AppId());

        if (send_life_event)
            parent()->signal_app_life_status_changed(client->AppId(), "", RuntimeStatus::CLOSING);

        (void) parent()->FindPidsAndSendSystemSignal(client->Pid(), SIGKILL);
    }
}

NativeClientInfo::NativeClientInfo(const std::string& app_id)
    : m_appId(app_id),
      m_interfaceVersion(1),
      m_isRegistered(false),
      m_isRegistrationExpired(false),
      m_lsmsg(NULL),
      m_registrationCheckTimerSource(0),
      m_registrationCheckStartTime(0),
      m_lifeCycleHandler(NULL)
{
    LOG_INFO(MSGID_NATIVE_CLIENT_INFO, 1, PMLOGKS("app_id", m_appId.c_str()), "client_info_created");
}

NativeClientInfo::~NativeClientInfo()
{
    if (m_lsmsg != NULL)
        LSMessageUnref(m_lsmsg);
    StopTimerForCheckingRegistration();

    LOG_INFO(MSGID_NATIVE_CLIENT_INFO, 1, PMLOGKS("app_id", m_appId.c_str()), "client_info_removed");
}

void NativeClientInfo::Register(LSMessage* lsmsg)
{
    if (lsmsg == NULL) {
        // leave warning log
        return;
    }

    if (m_lsmsg != NULL) {
        // leave log for release previous connection
        LSMessageUnref(m_lsmsg);
        m_lsmsg = NULL;
    }

    StopTimerForCheckingRegistration();

    m_lsmsg = lsmsg;
    LSMessageRef(m_lsmsg);
    m_isRegistered = true;
    m_isRegistrationExpired = false;
}

void NativeClientInfo::Unregister()
{
    if (m_lsmsg != NULL) {
        LSMessageUnref(m_lsmsg);
        m_lsmsg = NULL;
    }
    m_isRegistered = false;
}

void NativeClientInfo::StartTimerForCheckingRegistration()
{

    StopTimerForCheckingRegistration();

    m_registrationCheckTimerSource = g_timeout_add( TIMEOUT_FOR_REGISTER_V2, NativeClientInfo::CheckRegistration, (gpointer) (this));
    m_registrationCheckStartTime = get_current_time();
    m_isRegistrationExpired = false;
}

void NativeClientInfo::StopTimerForCheckingRegistration()
{

    if (m_registrationCheckTimerSource != 0) {
        g_source_remove(m_registrationCheckTimerSource);
        m_registrationCheckTimerSource = 0;
    }
}

gboolean NativeClientInfo::CheckRegistration(gpointer user_data)
{
    NativeClientInfo* native_client = static_cast<NativeClientInfo*>(user_data);

    if (!native_client->IsRegistered()) {
        native_client->m_isRegistrationExpired = true;
    }

    native_client->StopTimerForCheckingRegistration();
    return FALSE;
}

void NativeClientInfo::SetLifeCycleHandler(int ver, NativeAppLifeCycleInterface* handler)
{
    m_interfaceVersion = ver;
    m_lifeCycleHandler = handler;
}

bool NativeClientInfo::SendEvent(pbnjson::JValue& payload)
{

    if (!m_isRegistered) {
        LOG_WARNING(MSGID_NATIVE_APP_LIFE_CYCLE_EVENT, 1,
                    PMLOGKS("reason", "app_is_not_registered"),
                    "payload: %s", payload.stringify().c_str());
        return false;
    }

    if (payload.isObject() && payload.hasKey("returnValue") == false) {
        payload.put("returnValue", true);
    }

    LSErrorSafe lserror;
    if (!LSMessageRespond(m_lsmsg, payload.stringify().c_str(), &lserror)) {
        LOG_ERROR(MSGID_LSCALL_ERR, 3,
                  PMLOGKS("type", "respond"),
                  PMLOGJSON("payload", payload.stringify().c_str()),
                  PMLOGKS("where", __FUNCTION__), "err: %s", lserror.message);
        return false;
    }

    return true;
}

NativeAppLifeHandler::NativeAppLifeHandler()
    : m_nativeHandlerV1(this),
      m_nativeHandlerV2(this)
{
    g_this = this;
}

NativeAppLifeHandler::~NativeAppLifeHandler()
{
}

NativeClientInfoPtr NativeAppLifeHandler::MakeNewClientInfo(const std::string& app_id)
{
    AppDescPtr app_desc = PackageManager::instance().getAppById(app_id);
    if (app_desc == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("reason", "not_existing_app"),
                  PMLOGKS("where", __FUNCTION__), "");
        return nullptr;
    }

    NativeClientInfoPtr new_client_info = std::make_shared<NativeClientInfo>(app_id);

    switch (app_desc->NativeInterfaceVersion()) {
    case 1:
        new_client_info->SetLifeCycleHandler(1, &m_nativeHandlerV1);
        break;
    case 2:
        new_client_info->SetLifeCycleHandler(2, &m_nativeHandlerV2);
        break;
    default:
        new_client_info->SetLifeCycleHandler(1, &m_nativeHandlerV1);
        break;
    }

    m_activeClients.push_back(new_client_info);

    PrintNativeClients();

    return new_client_info;
}

NativeClientInfoPtr NativeAppLifeHandler::GetNativeClientInfo(const std::string& app_id, bool make_new)
{
    auto it = std::find_if(m_activeClients.begin(), m_activeClients.end(), [&app_id](NativeClientInfoPtr client) {
        return app_id == client->AppId();
    });

    if (it == m_activeClients.end()) {
        return make_new ? MakeNewClientInfo(app_id) : nullptr;
    }

    return (*it);
}

NativeClientInfoPtr NativeAppLifeHandler::GetNativeClientInfoByPid(const std::string& pid)
{

    auto it = std::find_if(m_activeClients.begin(), m_activeClients.end(), [&pid](NativeClientInfoPtr client) {
        return pid == client->Pid();
    });

    if (it == m_activeClients.end())
        return nullptr;

    return (*it);
}

NativeClientInfoPtr NativeAppLifeHandler::GetNativeClientInfoOrMakeNew(const std::string& app_id)
{
    return GetNativeClientInfo(app_id, true);
}

void NativeAppLifeHandler::RemoveNativeClientInfo(const std::string& app_id)
{

    auto it = std::find_if(m_activeClients.begin(), m_activeClients.end(), [&app_id](NativeClientInfoPtr client) {
        return app_id == client->AppId();
    });
    if (it != m_activeClients.end()) {
        m_activeClients.erase(it);
    }
}

void NativeAppLifeHandler::PrintNativeClients()
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 1, PMLOGKFV("native_app_clients_total", "%d", (int)m_activeClients.size()), "");
    for (auto client : m_activeClients) {
        LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
                 PMLOGKS("app_id", client->AppId().c_str()),
                 PMLOGKS("pid", client->Pid().c_str()), "current_client");
    }
}

void NativeAppLifeHandler::launch(AppLaunchingItemPtr item)
{
    NativeClientInfoPtr client_info = GetNativeClientInfoOrMakeNew(item->appId());

    if (client_info == nullptr) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("app_id", item->appId().c_str()),
                  PMLOGKS("reason", "no_client"), "%s:%d", __FUNCTION__, __LINE__);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        signal_launching_done(item->uid());
        return;
    }

    client_info->GetLifeCycleHandler()->launch(client_info, item);
}

void NativeAppLifeHandler::close(AppCloseItemPtr item, std::string& err_text)
{
    NativeClientInfoPtr client_info = GetNativeClientInfo(item->getAppId());

    if (client_info == nullptr) {
        LOG_INFO(MSGID_APPCLOSE_ERR, 2,
                 PMLOGKS("app_id", item->getAppId().c_str()),
                 PMLOGKS("reason", "no_client"), "%s:%d", __FUNCTION__, __LINE__);
        err_text = "native app is not running";
        return;
    }
    RuntimeStatus life_status = AppInfoManager::instance().runtimeStatus(item->getAppId());
    if (RuntimeStatus::STOP == life_status) {
        LOG_INFO(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", item->getAppId().c_str()), "native app is not running");
        err_text = "native app is not running";
        return;
    }

    client_info->GetLifeCycleHandler()->close(client_info, item, err_text);
}

void NativeAppLifeHandler::pause(const std::string& app_id, const pbnjson::JValue& params, std::string& err_text, bool send_life_event)
{
    NativeClientInfoPtr client_info = GetNativeClientInfo(app_id);

    if (client_info == nullptr) {
        LOG_INFO(MSGID_APPPAUSE_ERR, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("reason", "no_client"), "%s:%d", __FUNCTION__, __LINE__);
        err_text = "no_handling_info";
        return;
    }

    client_info->GetLifeCycleHandler()->pause(client_info, params, err_text, send_life_event);
}

void NativeAppLifeHandler::RegisterApp(const std::string& app_id, LSMessage* lsmsg, std::string& err_text)
{
    NativeClientInfoPtr client_info = GetNativeClientInfo(app_id);

    if (client_info == nullptr) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("app_id", app_id.c_str()),
                  PMLOGKS("reason", "no_handling_info"), "%s", __FUNCTION__);
        err_text = "no_handling_info";
        return;
    }

    client_info->Register(lsmsg);

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);

    if (client_info->InterfaceVersion() == 2)
        payload.put("event", "registered");
    else
        payload.put("message", "registered");

    if (client_info->SendEvent(payload) == false) {
        LOG_WARNING(MSGID_APPLAUNCH_ERR, 1, PMLOGKS("app_id", app_id.c_str()), "failed_to_send_registered_event");
    }

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS("app_id", app_id.c_str()),
             PMLOGKS("status", "connected"), "");

    // update life status
    signal_app_life_status_changed(app_id, "", RuntimeStatus::REGISTERED);

    HandlePendingQOnRegistered(app_id);
}

bool NativeAppLifeHandler::ChangeRunningAppId(const std::string& current_id, const std::string& target_id, ErrorInfo& err_info)
{
    NativeClientInfoPtr client_info = GetNativeClientInfo(current_id);

    if (client_info == nullptr) {
        LOG_ERROR(MSGID_CHANGE_APPID, 1, PMLOGKS("from", current_id.c_str()), "not running");
        err_info.setError(APP_IS_NOT_RUNNING, current_id + " doesn't exist in running list");
        return false;
    }

    // return error if target_id is already running
    if (AppInfoManager::instance().isRunning(target_id)) {
        LOG_ERROR(MSGID_CHANGE_APPID, 1, PMLOGKS("to", target_id.c_str()), "already running");
        err_info.setError(APP_IS_ALREADY_RUNNING, target_id + " already exists in running list");
        return false;
    }

    // remove connection data if already connected
    // target app will connect again
    client_info->Unregister();

    signal_app_life_status_changed(current_id, "", RuntimeStatus::STOP);
    signal_running_app_removed(current_id);
    client_info->SetAppId(target_id);
    signal_running_app_added(target_id, client_info->Pid(), "");
    signal_app_life_status_changed(target_id, "", RuntimeStatus::RUNNING);

    LOG_INFO(MSGID_CHANGE_APPID, 4,
             PMLOGKS("status", "changed"),
             PMLOGKS("prev_app_id", current_id.c_str()),
             PMLOGKS("new_app_id", target_id.c_str()),
             PMLOGKS("new_app_pid", client_info->Pid().c_str()), "");

    return true;
}

void NativeAppLifeHandler::AddLaunchingItemIntoPendingQ(AppLaunchingItemPtr item)
{
    m_launchPendingQueue.push_back(item);
}

AppLaunchingItemPtr NativeAppLifeHandler::GetLaunchPendingItem(const std::string& app_id)
{
    auto it = std::find_if(m_launchPendingQueue.begin(), m_launchPendingQueue.end(), [&app_id](AppLaunchingItemPtr item) {
        return (item->appId() == app_id);
    });
    if (it == m_launchPendingQueue.end())
        return nullptr;
    return (*it);
}

void NativeAppLifeHandler::RemoveLaunchPendingItem(const std::string& app_id)
{
    auto it = std::find_if(m_launchPendingQueue.begin(), m_launchPendingQueue.end(), [&app_id](AppLaunchingItemPtr item) {
        return (item->appId() == app_id);
    });
    if (it != m_launchPendingQueue.end())
        m_launchPendingQueue.erase(it);
}

pid_t fork_process(const char **argv, const char **envp)
{
    //TODO : Set child's working path
    GPid pid = -1;
    GError* gerr = NULL;
    GSpawnFlags flags = (GSpawnFlags) (G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_DO_NOT_REAP_CHILD);

    gboolean result = g_spawn_async_with_pipes(NULL,
                                               const_cast<char**>(argv),  // cmd arguments
                                               const_cast<char**>(envp),  // environment variables
                                               flags,
                                               NULL,
                                               NULL, &pid,
                                               NULL,
                                               NULL,
                                               NULL,
                                               &gerr);
    if (gerr) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "fork_fail"),
                  PMLOGKS("where", "fork_process"),
                  "returned_pid: %d, err_text: %s", (int )pid, gerr->message);
        g_error_free(gerr);
        gerr = NULL;
        return -1;
    }

    if (!result) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "return_false_from_gspawn"),
                  PMLOGKS("where", "fork_process"), "returned_pid: %d", pid);
        return -1;
    }

    return pid;
}

bool kill_processes(const PidVector& pids, int sig)
{
    auto it = pids.begin();
    if (it == pids.end())
        return true;

    // first process is parent process, killing child processes later can fail if parent itself terminates them
    bool success = kill(*it, sig) == 0;
    while (++it != pids.end()) {
        kill(*it, sig);
    }
    return success;
}

bool NativeAppLifeHandler::FindPidsAndSendSystemSignal(const std::string& pid, int signame)
{
    PidVector pids = FindChildPids(pid);
    return SendSystemSignal(pids, signame);
}

bool NativeAppLifeHandler::SendSystemSignal(const PidVector& pids, int signame)
{

    LOG_INFO(MSGID_APPCLOSE, 1,
             PMLOGKS("status", __FUNCTION__),
             "signame: %d, pids: %s", signame, PidsToString(pids).c_str());

    if (pids.empty()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS("reason", "empty_pids"),
                  PMLOGKS("where", __FUNCTION__), "");
        return false;
    }

    if (!kill_processes(pids, signame)) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS("reason", "seding_signal_error"),
                  PMLOGKS("where", __FUNCTION__), "signame: %d", signame);
        return false;
    }

    return true;
}

void NativeAppLifeHandler::StartTimerToKillApp(const std::string& app_id, const std::string& pid, const PidVector& all_pids, guint timeout)
{

    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", app_id.c_str()),
             PMLOGKS("status", "start_kill_timer"), "pid: %s", pid.c_str());

    KillingDataPtr target_item = std::make_shared<KillingData>(app_id, pid, all_pids);
    target_item->m_timerSource = g_timeout_add(timeout, NativeAppLifeHandler::KillAppOnTimeout, (gpointer) target_item.get());
    m_killingList.push_back(target_item);
}

void NativeAppLifeHandler::StopTimerToKillApp(const std::string& app_id)
{

    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", app_id.c_str()),
             PMLOGKS("status", "cancel_kill_timer"), "");

    KillingDataPtr item = GetKillingDataByAppId(app_id);
    if (item == nullptr) {
        return;
    }

    RemoveKillingData(app_id);
}

gboolean NativeAppLifeHandler::KillAppOnTimeout(gpointer user_data)
{

    if (user_data == NULL) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS("status", "null_user_data"),
                  PMLOGKS("where", __FUNCTION__), "");
        return FALSE;
    }

    KillingData* killing_item = static_cast<KillingData*>(user_data);
    if (killing_item == NULL) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS("status", "null_killing_item"),
                  PMLOGKS("where", __FUNCTION__), "");
        return FALSE;
    }

    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", killing_item->m_appId.c_str()),
             PMLOGKS("status", "kill_process"), "");
    g_this->SendSystemSignal(killing_item->m_allPids, SIGKILL);

    return FALSE;
}

KillingDataPtr NativeAppLifeHandler::GetKillingDataByAppId(const std::string& app_id)
{
    auto it = std::find_if(m_killingList.begin(), m_killingList.end(), [&app_id](KillingDataPtr data) {
        return (data->m_appId == app_id);}
    );
    if (it == m_killingList.end())
        return NULL;

    return (*it);
}

void NativeAppLifeHandler::RemoveKillingData(const std::string& app_id)
{
    auto it = m_killingList.begin();
    while (it != m_killingList.end()) {
        if (app_id == (*it)->m_appId) {
            g_source_remove((*it)->m_timerSource);
            (*it)->m_timerSource = 0;
            it = m_killingList.erase(it);
        } else {
            ++it;
        }
    }
}

PidVector FindChildPids(const std::string& pid)
{
    PidVector pids;
    pids.push_back((pid_t) std::atol(pid.c_str()));

    proc_t **proctab = readproctab(PROC_FILLSTAT);
    if (!proctab) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 1, PMLOGKS("reason", "readproctab_error"), "failed to read proctab");
        return pids;
    }

    size_t idx = 0;
    while (idx != pids.size()) {
        for (proc_t **proc = proctab; *proc; ++proc) {
            pid_t tid = (*proc)->tid;
            pid_t ppid = (*proc)->ppid;
            if (ppid == pids[idx]) {
                pids.push_back(tid);
            }
        }
        ++idx;
    }

    for (proc_t **proc = proctab; *proc; ++proc) {
        free(*proc);
    }

    free(proctab);

    return pids;
}

static std::string PidsToString(const PidVector& pids)
{
    std::string result;
    std::string delim;
    for (pid_t pid : pids) {
        result += delim;
        result += std::to_string(pid);
        delim = " ";
    }
    return result;
}

void NativeAppLifeHandler::ChildProcessWatcher(GPid pid, gint status, gpointer data)
{
    g_spawn_close_pid(pid);
    g_this->HandleClosedPid(boost::lexical_cast<std::string>(pid), status);
}

void NativeAppLifeHandler::HandleClosedPid(const std::string& pid, gint status)
{

    LOG_INFO(MSGID_APPCLOSE, 1, PMLOGKS("closed_pid", pid.c_str()), "");

    NativeClientInfoPtr client = GetNativeClientInfoByPid(pid);
    if (client == nullptr) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 3,
                  PMLOGKS("pid", pid.c_str()),
                  PMLOGKS("reason", "empty_client_info"),
                  PMLOGKS("where", __FUNCTION__), "");
        return;
    }

    std::string app_id = client->AppId();

    RuntimeStatus life_status = AppInfoManager::instance().runtimeStatus(app_id);
    if (RuntimeStatus::CLOSING == life_status) {
        LOG_INFO(MSGID_APPCLOSE, 2,
                 PMLOGKS("pid", pid.c_str()),
                 PMLOGKS("where", "native_process_watcher"), "received event closed by sam");
    } else {
        LOG_INFO(MSGID_APPCLOSE, 2,
                 PMLOGKS("pid", pid.c_str()),
                 PMLOGKS("where", "native_process_watcher"), "received event closed by itself");
    }

    StopTimerToKillApp(app_id);

    LOG_INFO(MSGID_APP_CLOSED, 4,
             PMLOGKS("app_id", app_id.c_str()),
             PMLOGKS("type", "native"),
             PMLOGKS("pid", pid.c_str()),
             PMLOGKFV("exit_status", "%d", status), "");

    RemoveNativeClientInfo(app_id);

    signal_app_life_status_changed(app_id, "", RuntimeStatus::STOP);
    signal_running_app_removed(app_id);

    HandlePendingQOnClosed(app_id);
}

void NativeAppLifeHandler::HandlePendingQOnRegistered(const std::string& app_id)
{

    // handle all pending request as relaunch
    auto pending_item = m_launchPendingQueue.begin();
    while (pending_item != m_launchPendingQueue.end()) {
        if ((*pending_item)->appId() != app_id) {
            ++pending_item;
            continue;
        } else {
            LOG_INFO(MSGID_APPLAUNCH, 2,
                     PMLOGKS("app_id", app_id.c_str()),
                     PMLOGKS("action", "launch_app_waiting_registration"), "");
            launch(*pending_item);
            pending_item = m_launchPendingQueue.erase(pending_item);
        }
    }
}

void NativeAppLifeHandler::HandlePendingQOnClosed(const std::string& app_id)
{

    // handle only one pending request
    AppLaunchingItemPtr pending_item = GetLaunchPendingItem(app_id);
    if (pending_item) {
        RemoveLaunchPendingItem(app_id);
        LOG_INFO(MSGID_APPLAUNCH, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("action", "launch_app_waiting_previous_app_closed"), "");
        launch(pending_item);
    }
}

void NativeAppLifeHandler::CancelLaunchPendingItemAndMakeItDone(const std::string& app_id)
{

    auto pending_item = m_launchPendingQueue.begin();
    while (pending_item != m_launchPendingQueue.end()) {
        if ((*pending_item)->appId() != app_id) {
            ++pending_item;
            continue;
        } else {
            LOG_INFO(MSGID_APPLAUNCH, 2,
                     PMLOGKS("app_id", app_id.c_str()),
                     PMLOGKS("action", "cancel_launch_request"), "");
            (*pending_item)->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "launching_cancled");
            signal_launching_done((*pending_item)->uid());
            pending_item = m_launchPendingQueue.erase(pending_item);
        }
    }
}
