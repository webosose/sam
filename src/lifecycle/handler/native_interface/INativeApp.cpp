// Copyright (c) 2019 LG Electronics, Inc.
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

#include "../NativeAppLifeHandler.h"
#include "setting/Settings.h"
#include <boost/lexical_cast.hpp>
#include <lifecycle/handler/native_interface/INativeApp.h>
#include "util/LinuxProcess.h"

INativeApp::INativeApp()
{
}

INativeApp::~INativeApp()
{
}

void INativeApp::addLaunchHandler(RuntimeStatus status, NativeAppLaunchHandler handler)
{
    m_launchHandlerMap[status] = handler;
}

void INativeApp::launch(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    // Check launch condition
    if (checkLaunchCondition(item) == false) {
        NativeAppLifeHandler::getInstance().signal_launching_done(item->getUid());
        return;
    }

    RuntimeStatus life_status = AppInfoManager::instance().runtimeStatus(item->getAppId());
    if (!m_launchHandlerMap.count(life_status)) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", item->getAppId().c_str()),
                  PMLOGKS("reason", "invalid_status_to_handle"),
                  PMLOGKS("where", __FUNCTION__), "");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        return;
    }

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS("app_id", item->getAppId().c_str()),
             PMLOGKS("status", "start_launch_handler"), "life_status: %d", (int )life_status);

    NativeAppLaunchHandler handler = m_launchHandlerMap[life_status];
    handler(client, item);
}

void INativeApp::close(NativeClientInfoPtr client, CloseAppItemPtr item, std::string& err_text)
{
    closeAsPolicy(client, item, err_text);
}

void INativeApp::pause(NativeClientInfoPtr client, const pbnjson::JValue& params, std::string& err_text, bool send_life_event)
{
    pauseAsPolicy(client, params, err_text, send_life_event);
}

void INativeApp::launchAsCommon(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    if (item == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "null_poiner"),
                  PMLOGKS("where", __FUNCTION__), "");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        return;
    }

    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    AppDescPtr appDescPtr = PackageManager::instance().getAppById(item->getAppId());
    if (appDescPtr == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("reason", "null_description"),
                  PMLOGKS("where", __FUNCTION__), "");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        NativeAppLifeHandler::getInstance().signal_launching_done(item->getUid());
        return;
    }

    // construct the path for launching
    std::string path = appDescPtr->getEntryPoint();
    if (path.find("file://", 0) != std::string::npos)
        path = path.substr(7);

    // [ Template method ]
    // Handle Differently based on Ver1/Ver2 policy
    std::string str_params = makeForkArguments(item, appDescPtr);

    const char* fork_params[10] = { 0, };
    const char* jailer_type = "";
    bool isNojailApp = SettingsImpl::instance().isInNoJailApps(appDescPtr->getAppId());

    if (AppType::AppType_Native_AppShell == appDescPtr->getAppType()) {
        fork_params[0] = SettingsImpl::instance().m_appshellRunnerPath.c_str();
        fork_params[1] = "--appid";
        fork_params[2] = appDescPtr->getAppId().c_str();
        fork_params[3] = "--folder";
        fork_params[4] = appDescPtr->getFolderPath().c_str();
        fork_params[5] = "--params";
        fork_params[6] = str_params.c_str();
        fork_params[7] = NULL;
        LOG_INFO(MSGID_APPLAUNCH, 4,
                 PMLOGKS("appshellRunnderPath", SettingsImpl::instance().m_appshellRunnerPath.c_str()),
                 PMLOGKS("app_id", appDescPtr->getAppId().c_str()),
                 PMLOGKS("app_folder_path", appDescPtr->getFolderPath().c_str()),
                 PMLOGJSON("params", str_params.c_str()), "launch with appshell_runner");
    } else if (AppType::AppType_Native_Qml == appDescPtr->getAppType()) {
        fork_params[0] = SettingsImpl::instance().m_qmlRunnerPath.c_str();
        fork_params[1] = str_params.c_str();
        fork_params[2] = NULL;

        LOG_INFO(MSGID_APPLAUNCH, 3,
                 PMLOGKS("app_id", appDescPtr->getAppId().c_str()),
                 PMLOGKS("app_path", path.c_str()),
                 PMLOGJSON("params", str_params.c_str()), "launch with qml_runner");
    } else if (SettingsImpl::instance().m_isJailMode && !isNojailApp) {
        if (AppTypeByDir::AppTypeByDir_Dev == appDescPtr->getTypeByDir()) {
            jailer_type = "native_devmode";
        } else {
            switch (appDescPtr->getAppType()) {
            case AppType::AppType_Native: {
                jailer_type = "native";
                break;
            }
            case AppType::AppType_Native_Builtin: {
                jailer_type = "native_builtin";
                break;
            }
            case AppType::AppType_Native_Mvpd: {
                jailer_type = "native_mvpd";
                break;
            }
            default:
                jailer_type = "default";
                break;
            }
        }

        fork_params[0] = SettingsImpl::instance().m_jailerPath.c_str();
        fork_params[1] = "-t";
        fork_params[2] = jailer_type;
        fork_params[3] = "-i";
        fork_params[4] = appDescPtr->getAppId().c_str();
        fork_params[5] = "-p";
        fork_params[6] = appDescPtr->getFolderPath().c_str();
        fork_params[7] = path.c_str();
        fork_params[8] = str_params.c_str();
        fork_params[9] = NULL;

        // This log shows whether native app's launched via Jailer or not
        // Do not remove this log, until jailer become stable
        LOG_INFO(MSGID_APPLAUNCH, 6,
                 PMLOGKS("jailer_path", SettingsImpl::instance().m_jailerPath.c_str()),
                 PMLOGKS("jailer_type", jailer_type),
                 PMLOGKS("app_id", appDescPtr->getAppId().c_str()),
                 PMLOGKS("app_folder_path", appDescPtr->getFolderPath().c_str()),
                 PMLOGKS("app_path", path.c_str()), PMLOGJSON("params", str_params.c_str()), "launch with jail");
    } else {
        // This log shows whether native app's launched via Jailer or not
        // Do not remove this log, until jailer become stable
        fork_params[0] = path.c_str();
        fork_params[1] = str_params.c_str();
        fork_params[2] = NULL;

        LOG_INFO(MSGID_APPLAUNCH, 3,
                 PMLOGKS("app_id", appDescPtr->getAppId().c_str()),
                 PMLOGKS("app_path", path.c_str()),
                 PMLOGJSON("params", str_params.c_str()), "launch as root");

    }

    if (item->getPreload().empty())
        NativeAppLifeHandler::getInstance().signal_app_life_status_changed(item->getAppId(), item->getUid(), RuntimeStatus::LAUNCHING);
    else
        NativeAppLifeHandler::getInstance().signal_app_life_status_changed(item->getAppId(), item->getUid(), RuntimeStatus::PRELOADING);

    int pid = LinuxProcess::fork_process(fork_params, NULL);

    if (pid <= 0) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("app_id", appDescPtr->getAppId().c_str()),
                  PMLOGKS("path", path.c_str()), "forked_pid: %d", pid);
        NativeAppLifeHandler::getInstance().signal_app_life_status_changed(item->getAppId(), item->getUid(), RuntimeStatus::STOP);
        item->setErrCodeText(APP_ERR_NATIVE_SPAWN, "Failed to launch process");
        NativeAppLifeHandler::getInstance().signal_launching_done(item->getUid());
        return;
    }

    // set watcher for the child's
    g_child_watch_add(pid, (GChildWatchFunc) NativeAppLifeHandler::onKillChildProcess, NULL);

    double now = getCurrentTime();
    double collapse = now - item->launchStartTime();
    LOG_INFO(MSGID_APP_LAUNCHED, 5,
             PMLOGKS("app_id", appDescPtr->getAppId().c_str()),
             PMLOGKS("type", "native"),
             PMLOGKFV("pid", "%d", pid),
             PMLOGKFV("start_time", "%f", item->launchStartTime()),
             PMLOGKFV("collapse_time", "%f", collapse), "");

    std::string new_pid = boost::lexical_cast<std::string>(pid);
    item->setPid(new_pid);
    client->setPid(new_pid);
    client->startTimerForCheckingRegistration();
    AppInfoManager::instance().setLastLaunchTime(item->getAppId(), now);

    NativeAppLifeHandler::getInstance().signal_running_app_added(item->getAppId(), new_pid, "");
    NativeAppLifeHandler::getInstance().signal_app_life_status_changed(item->getAppId(), item->getUid(), RuntimeStatus::RUNNING);
    NativeAppLifeHandler::getInstance().signal_launching_done(item->getUid());
}

void INativeApp::relaunchAsCommon(NativeClientInfoPtr client, LaunchAppItemPtr item)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", client->getAppId().c_str()),
             PMLOGKS("start_native_handler", __FUNCTION__),
             "ver: %d", client->getInterfaceVersion());

    pbnjson::JValue payload = pbnjson::Object();
    makeRelaunchParams(item, payload);

    NativeAppLifeHandler::getInstance().signal_app_life_status_changed(item->getAppId(), item->getUid(), RuntimeStatus::LAUNCHING);

    if (client->sendEvent(payload) == false) {
        LOG_WARNING(MSGID_APPCLOSE_ERR, 1, PMLOGKS("app_id", client->getAppId().c_str()), "failed_to_send_relaunch_event");
    }

    NativeAppLifeHandler::getInstance().signal_launching_done(item->getUid());
}

////////////////////////////////////////////////////////////////
// NativeAppLifeCycleInterface version1




