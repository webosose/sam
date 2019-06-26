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

#include <ext/stdio_filebuf.h>
#include <boost/lexical_cast.hpp>
#include <lifecycle/AppInfoManager.h>
#include <lifecycle/AppInfoManager.h>
#include <lifecycle/ApplicationErrors.h>
#include <lifecycle/handler/NativeAppLifeHandler.h>
#include "native_interface/NativeAppLifeCycleInterfaceFactory.h"
#include <package/AppDescription.h>
#include <package/PackageManager.h>
#include <setting/Settings.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/LSUtils.h>
#include <util/Utils.h>

static NativeAppLifeHandler* g_this = NULL;

NativeAppLifeHandler::NativeAppLifeHandler()
{
    g_this = this;
}

NativeAppLifeHandler::~NativeAppLifeHandler()
{
}

NativeClientInfoPtr NativeAppLifeHandler::makeNewClientInfo(const std::string& appId)
{
    AppDescPtr appDescPtr = PackageManager::instance().getAppById(appId);
    if (appDescPtr == NULL) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 3,
                  PMLOGKS("app_id", appId.c_str()),
                  PMLOGKS("reason", "not_existing_app"),
                  PMLOGKS("where", __FUNCTION__), "");
        return nullptr;
    }

    NativeClientInfoPtr clientInfoPtr = std::make_shared<NativeClientInfo>(appId);
    clientInfoPtr->setLifeCycleHandler(
        appDescPtr->getNativeInterfaceVersion(),
        &NativeAppLifeCycleInterfaceFactory::getInstance().getInterface(appDescPtr)
    );

    m_activeClients.push_back(clientInfoPtr);

    printNativeClients();

    return clientInfoPtr;
}

NativeClientInfoPtr NativeAppLifeHandler::getNativeClientInfo(const std::string& appId, bool make_new)
{
    auto it = std::find_if(m_activeClients.begin(), m_activeClients.end(), [&appId](NativeClientInfoPtr client) {
        return appId == client->getAppId();
    });

    if (it == m_activeClients.end()) {
        return make_new ? makeNewClientInfo(appId) : nullptr;
    }

    return (*it);
}

NativeClientInfoPtr NativeAppLifeHandler::getNativeClientInfoByPid(const std::string& pid)
{

    auto it = std::find_if(m_activeClients.begin(), m_activeClients.end(), [&pid](NativeClientInfoPtr client) {
        return pid == client->getPid();
    });

    if (it == m_activeClients.end())
        return nullptr;

    return (*it);
}

NativeClientInfoPtr NativeAppLifeHandler::getNativeClientInfoOrMakeNew(const std::string& app_id)
{
    return getNativeClientInfo(app_id, true);
}

void NativeAppLifeHandler::removeNativeClientInfo(const std::string& app_id)
{

    auto it = std::find_if(m_activeClients.begin(), m_activeClients.end(), [&app_id](NativeClientInfoPtr client) {
        return app_id == client->getAppId();
    });
    if (it != m_activeClients.end()) {
        m_activeClients.erase(it);
    }
}

void NativeAppLifeHandler::printNativeClients()
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 1, PMLOGKFV("native_app_clients_total", "%d", (int)m_activeClients.size()), "");
    for (auto client : m_activeClients) {
        LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
                 PMLOGKS("app_id", client->getAppId().c_str()),
                 PMLOGKS("pid", client->getPid().c_str()), "current_client");
    }
}

void NativeAppLifeHandler::launch(LaunchAppItemPtr item)
{
    NativeClientInfoPtr client_info = getNativeClientInfoOrMakeNew(item->getAppId());

    if (client_info == nullptr) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("app_id", item->getAppId().c_str()),
                  PMLOGKS("reason", "no_client"), "%s:%d", __FUNCTION__, __LINE__);
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        signal_launching_done(item->getUid());
        return;
    }

    client_info->getLifeCycleHandler()->launch(client_info, item);
}

void NativeAppLifeHandler::close(CloseAppItemPtr item, std::string& err_text)
{
    NativeClientInfoPtr client_info = getNativeClientInfo(item->getAppId());

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

    client_info->getLifeCycleHandler()->close(client_info, item, err_text);
}

void NativeAppLifeHandler::pause(const std::string& app_id, const pbnjson::JValue& params, std::string& err_text, bool send_life_event)
{
    NativeClientInfoPtr client_info = getNativeClientInfo(app_id);

    if (client_info == nullptr) {
        LOG_INFO(MSGID_APPPAUSE_ERR, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("reason", "no_client"), "%s:%d", __FUNCTION__, __LINE__);
        err_text = "no_handling_info";
        return;
    }

    client_info->getLifeCycleHandler()->pause(client_info, params, err_text, send_life_event);
}

void NativeAppLifeHandler::registerApp(const std::string& appId, LSMessage* lsmsg, std::string& errorText)
{
    NativeClientInfoPtr clientInfo = getNativeClientInfo(appId);

    if (clientInfo == nullptr) {
        LOG_ERROR(MSGID_APPLAUNCH_ERR, 2,
                  PMLOGKS("app_id", appId.c_str()),
                  PMLOGKS("reason", "no_handling_info"), "%s", __FUNCTION__);
        errorText = "no_handling_info";
        return;
    }

    clientInfo->Register(lsmsg);

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);

    if (clientInfo->getInterfaceVersion() == 2)
        payload.put("event", "registered");
    else
        payload.put("message", "registered");

    if (clientInfo->sendEvent(payload) == false) {
        LOG_WARNING(MSGID_APPLAUNCH_ERR, 1, PMLOGKS("app_id", appId.c_str()), "failed_to_send_registered_event");
    }

    LOG_INFO(MSGID_APPLAUNCH, 2,
             PMLOGKS("app_id", appId.c_str()),
             PMLOGKS("status", "connected"), "");

    // update life status
    signal_app_life_status_changed(appId, "", RuntimeStatus::REGISTERED);

    handlePendingQOnRegistered(appId);
}

void NativeAppLifeHandler::addLaunchingItemIntoPendingQ(LaunchAppItemPtr item)
{
    m_launchPendingQueue.push_back(item);
}

LaunchAppItemPtr NativeAppLifeHandler::getLaunchPendingItem(const std::string& app_id)
{
    auto it = std::find_if(m_launchPendingQueue.begin(), m_launchPendingQueue.end(), [&app_id](LaunchAppItemPtr item) {
        return (item->getAppId() == app_id);
    });
    if (it == m_launchPendingQueue.end())
        return nullptr;
    return (*it);
}

void NativeAppLifeHandler::removeLaunchPendingItem(const std::string& app_id)
{
    auto it = std::find_if(m_launchPendingQueue.begin(), m_launchPendingQueue.end(), [&app_id](LaunchAppItemPtr item) {
        return (item->getAppId() == app_id);
    });
    if (it != m_launchPendingQueue.end())
        m_launchPendingQueue.erase(it);
}

bool NativeAppLifeHandler::findPidsAndSendSystemSignal(const std::string& pid, int signame)
{
    PidVector pids = LinuxProcess::FindChildPids(pid);
    return sendSystemSignal(pids, signame);
}

bool NativeAppLifeHandler::sendSystemSignal(const PidVector& pids, int signame)
{
    if (pids.empty()) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS("reason", "empty_pids"),
                  PMLOGKS("where", __FUNCTION__), "");
        return false;
    }

    if (!LinuxProcess::kill_processes(pids, signame)) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 2,
                  PMLOGKS("reason", "seding_signal_error"),
                  PMLOGKS("where", __FUNCTION__), "signame: %d", signame);
        return false;
    }

    return true;
}

void NativeAppLifeHandler::startTimerToKillApp(const std::string& appId, const std::string& pid, const PidVector& allPids, guint timeout)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", appId.c_str()),
             PMLOGKS("status", "start_kill_timer"), "pid: %s", pid.c_str());

    KillingDataPtr targetItem = std::make_shared<KillingData>(appId, pid, allPids);
    targetItem->m_timerSource = g_timeout_add(timeout, NativeAppLifeHandler::killAppOnTimeout, (gpointer) targetItem.get());
    m_killingList.push_back(targetItem);
}

void NativeAppLifeHandler::stopTimerToKillApp(const std::string& app_id)
{
    LOG_INFO(MSGID_NATIVE_APP_HANDLER, 2,
             PMLOGKS("app_id", app_id.c_str()),
             PMLOGKS("status", "cancel_kill_timer"), "");

    KillingDataPtr item = getKillingDataByAppId(app_id);
    if (item == nullptr) {
        return;
    }

    removeKillingData(app_id);
}

gboolean NativeAppLifeHandler::killAppOnTimeout(gpointer user_data)
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
    g_this->sendSystemSignal(killing_item->m_allPids, SIGKILL);

    return FALSE;
}

KillingDataPtr NativeAppLifeHandler::getKillingDataByAppId(const std::string& app_id)
{
    auto it = std::find_if(m_killingList.begin(), m_killingList.end(), [&app_id](KillingDataPtr data) {
        return (data->m_appId == app_id);}
    );
    if (it == m_killingList.end())
        return NULL;

    return (*it);
}

void NativeAppLifeHandler::removeKillingData(const std::string& app_id)
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

void NativeAppLifeHandler::onKillChildProcess(GPid pid, gint status, gpointer data)
{
    g_spawn_close_pid(pid);
    g_this->handleClosedPid(boost::lexical_cast<std::string>(pid), status);
}

void NativeAppLifeHandler::handleClosedPid(const std::string& pid, gint status)
{

    LOG_INFO(MSGID_APPCLOSE, 1, PMLOGKS("closed_pid", pid.c_str()), "");

    NativeClientInfoPtr client = getNativeClientInfoByPid(pid);
    if (client == nullptr) {
        LOG_ERROR(MSGID_APPCLOSE_ERR, 3,
                  PMLOGKS("pid", pid.c_str()),
                  PMLOGKS("reason", "empty_client_info"),
                  PMLOGKS("where", __FUNCTION__), "");
        return;
    }

    std::string app_id = client->getAppId();

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

    stopTimerToKillApp(app_id);

    LOG_INFO(MSGID_APP_CLOSED, 4,
             PMLOGKS("app_id", app_id.c_str()),
             PMLOGKS("type", "native"),
             PMLOGKS("pid", pid.c_str()),
             PMLOGKFV("exit_status", "%d", status), "");

    removeNativeClientInfo(app_id);

    signal_app_life_status_changed(app_id, "", RuntimeStatus::STOP);
    signal_running_app_removed(app_id);

    handlePendingQOnClosed(app_id);
}

void NativeAppLifeHandler::handlePendingQOnRegistered(const std::string& app_id)
{

    // handle all pending request as relaunch
    auto pending_item = m_launchPendingQueue.begin();
    while (pending_item != m_launchPendingQueue.end()) {
        if ((*pending_item)->getAppId() != app_id) {
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

void NativeAppLifeHandler::handlePendingQOnClosed(const std::string& app_id)
{

    // handle only one pending request
    LaunchAppItemPtr pending_item = getLaunchPendingItem(app_id);
    if (pending_item) {
        removeLaunchPendingItem(app_id);
        LOG_INFO(MSGID_APPLAUNCH, 2,
                 PMLOGKS("app_id", app_id.c_str()),
                 PMLOGKS("action", "launch_app_waiting_previous_app_closed"), "");
        launch(pending_item);
    }
}

void NativeAppLifeHandler::cancelLaunchPendingItemAndMakeItDone(const std::string& app_id)
{

    auto pending_item = m_launchPendingQueue.begin();
    while (pending_item != m_launchPendingQueue.end()) {
        if ((*pending_item)->getAppId() != app_id) {
            ++pending_item;
            continue;
        } else {
            LOG_INFO(MSGID_APPLAUNCH, 2,
                     PMLOGKS("app_id", app_id.c_str()),
                     PMLOGKS("action", "cancel_launch_request"), "");
            (*pending_item)->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "launching_cancled");
            signal_launching_done((*pending_item)->getUid());
            pending_item = m_launchPendingQueue.erase(pending_item);
        }
    }
}
