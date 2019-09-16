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

#include <ext/stdio_filebuf.h>
#include <boost/lexical_cast.hpp>
#include <lifecycle/ApplicationErrors.h>
#include <lifecycle/handler/NativeAppLifeHandler.h>
#include <lifecycle/RunningInfoManager.h>
#include <lifecycle/RunningInfoManager.h>
#include <package/AppPackage.h>
#include <package/AppPackageManager.h>
#include "native_interface/NativeAppLifeCycleInterfaceFactory.h"
#include <setting/Settings.h>
#include <util/LSUtils.h>

NativeAppLifeHandler::NativeAppLifeHandler()
{
}

NativeAppLifeHandler::~NativeAppLifeHandler()
{
}

NativeClientInfoPtr NativeAppLifeHandler::makeNewClientInfo(const std::string& appId)
{
    AppPackagePtr appDescPtr = AppPackageManager::getInstance().getAppById(appId);
    if (appDescPtr == NULL) {
        Logger::error(getClassName(), __FUNCTION__, appId, "not_existing_app");
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

NativeClientInfoPtr NativeAppLifeHandler::getNativeClientInfoOrMakeNew(const std::string& appId)
{
    return getNativeClientInfo(appId, true);
}

void NativeAppLifeHandler::removeNativeClientInfo(const std::string& appId)
{

    auto it = std::find_if(m_activeClients.begin(), m_activeClients.end(), [&appId](NativeClientInfoPtr client) {
        return appId == client->getAppId();
    });
    if (it != m_activeClients.end()) {
        m_activeClients.erase(it);
    }
}

void NativeAppLifeHandler::printNativeClients()
{
    Logger::info(getClassName(), __FUNCTION__, Logger::format("native_app_clients_total(%d)", m_activeClients.size()));
    for (auto client : m_activeClients) {
        Logger::info(getClassName(), __FUNCTION__, client->getAppId(), Logger::format("pid(%d)", client->getPid().c_str()));
    }
}

void NativeAppLifeHandler::launch(LaunchAppItemPtr item)
{
    NativeClientInfoPtr client_info = getNativeClientInfoOrMakeNew(item->getAppId());

    if (client_info == nullptr) {
        Logger::error(getClassName(), __FUNCTION__, item->getAppId(), "no_client");
        item->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "internal error");
        EventLaunchingDone(item->getUid());
        return;
    }

    client_info->getLifeCycleHandler()->launch(client_info, item);
}

void NativeAppLifeHandler::close(CloseAppItemPtr item, std::string& errorText)
{
    NativeClientInfoPtr client_info = getNativeClientInfo(item->getAppId());

    if (client_info == nullptr) {
        Logger::info(getClassName(), __FUNCTION__, item->getAppId(), "no_client");
        errorText = "native app is not running";
        return;
    }
    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(item->getAppId());
    if (RuntimeStatus::STOP == runningInfoPtr->getRuntimeStatus()) {
        Logger::info(getClassName(), __FUNCTION__, item->getAppId(), "native app is not running");
        errorText = "native app is not running";
        return;
    }

    client_info->getLifeCycleHandler()->close(client_info, item, errorText);
}

void NativeAppLifeHandler::pause(const std::string& appId, const pbnjson::JValue& params, std::string& errorText, bool send_life_event)
{
    NativeClientInfoPtr client_info = getNativeClientInfo(appId);

    if (client_info == nullptr) {
        Logger::info(getClassName(), __FUNCTION__, appId, "no_client");
        errorText = "no_handling_info";
        return;
    }

    client_info->getLifeCycleHandler()->pause(client_info, params, errorText, send_life_event);
}

void NativeAppLifeHandler::registerApp(const std::string& appId, LSMessage* message, std::string& errorText)
{
    NativeClientInfoPtr clientInfo = getNativeClientInfo(appId);

    if (clientInfo == nullptr) {
        Logger::info(getClassName(), __FUNCTION__, appId, "no_handling_info");
        errorText = "no_handling_info";
        return;
    }

    clientInfo->Register(message);

    pbnjson::JValue payload = pbnjson::Object();
    payload.put("returnValue", true);

    if (clientInfo->getInterfaceVersion() == 2)
        payload.put("event", "registered");
    else
        payload.put("message", "registered");

    if (clientInfo->sendEvent(payload) == false) {
        Logger::warning(getClassName(), __FUNCTION__, appId, "failed_to_send_registered_event");
    }

    Logger::info(getClassName(), __FUNCTION__, appId, "connected");

    // update life status
    EventAppLifeStatusChanged(appId, "", RuntimeStatus::REGISTERED);

    handlePendingQOnRegistered(appId);
}

void NativeAppLifeHandler::addLaunchingItemIntoPendingQ(LaunchAppItemPtr item)
{
    m_launchPendingQueue.push_back(item);
}

LaunchAppItemPtr NativeAppLifeHandler::getLaunchPendingItem(const std::string& appId)
{
    auto it = std::find_if(m_launchPendingQueue.begin(), m_launchPendingQueue.end(), [&appId](LaunchAppItemPtr item) {
        return (item->getAppId() == appId);
    });
    if (it == m_launchPendingQueue.end())
        return nullptr;
    return (*it);
}

void NativeAppLifeHandler::removeLaunchPendingItem(const std::string& appId)
{
    auto it = std::find_if(m_launchPendingQueue.begin(), m_launchPendingQueue.end(), [&appId](LaunchAppItemPtr item) {
        return (item->getAppId() == appId);
    });
    if (it != m_launchPendingQueue.end())
        m_launchPendingQueue.erase(it);
}

bool NativeAppLifeHandler::findPidsAndSendSystemSignal(const std::string& pid, int signame)
{
    PidVector pids = LinuxProcess::findChildPids(pid);
    return sendSystemSignal(pids, signame);
}

bool NativeAppLifeHandler::sendSystemSignal(const PidVector& pids, int signame)
{
    if (pids.empty()) {
        Logger::error(getClassName(), __FUNCTION__, "empty_pids");
        return false;
    }

    if (!LinuxProcess::killProcesses(pids, signame)) {
        Logger::error(getClassName(), __FUNCTION__, "seding_signal_error");
        return false;
    }

    return true;
}

void NativeAppLifeHandler::startTimerToKillApp(const std::string& appId, const std::string& pid, const PidVector& allPids, guint timeout)
{
    Logger::info(getClassName(), __FUNCTION__, appId, "start_kill_timer");
    KillingDataPtr targetItem = std::make_shared<KillingData>(appId, pid, allPids);
    targetItem->m_timerSource = g_timeout_add(timeout, NativeAppLifeHandler::killAppOnTimeout, (gpointer) targetItem.get());
    m_killingList.push_back(targetItem);
}

void NativeAppLifeHandler::stopTimerToKillApp(const std::string& appId)
{
    Logger::info(getClassName(), __FUNCTION__, appId, "cancel_kill_timer");
    KillingDataPtr item = getKillingDataByAppId(appId);
    if (item == nullptr) {
        return;
    }

    removeKillingData(appId);
}

gboolean NativeAppLifeHandler::killAppOnTimeout(gpointer context)
{

    if (context == NULL) {
        Logger::error(NativeAppLifeHandler::getInstance().getClassName(), __FUNCTION__, "null_context");
        return FALSE;
    }

    KillingData* killing_item = static_cast<KillingData*>(context);
    if (killing_item == NULL) {
        Logger::error(NativeAppLifeHandler::getInstance().getClassName(), __FUNCTION__, "null_killing_item");
        return FALSE;
    }

    Logger::info(NativeAppLifeHandler::getInstance().getClassName(), __FUNCTION__, killing_item->m_appId, "kill_process");
    NativeAppLifeHandler::getInstance().sendSystemSignal(killing_item->m_allPids, SIGKILL);

    return FALSE;
}

KillingDataPtr NativeAppLifeHandler::getKillingDataByAppId(const std::string& appId)
{
    auto it = std::find_if(m_killingList.begin(), m_killingList.end(), [&appId](KillingDataPtr data) {
        return (data->m_appId == appId);}
    );
    if (it == m_killingList.end())
        return NULL;

    return (*it);
}

void NativeAppLifeHandler::removeKillingData(const std::string& appId)
{
    auto it = m_killingList.begin();
    while (it != m_killingList.end()) {
        if (appId == (*it)->m_appId) {
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
    NativeAppLifeHandler::getInstance().handleClosedPid(boost::lexical_cast<std::string>(pid), status);
}

void NativeAppLifeHandler::handleClosedPid(const std::string& pid, gint status)
{
    Logger::info(getClassName(), __FUNCTION__, "closed_pid");
    NativeClientInfoPtr client = getNativeClientInfoByPid(pid);
    if (client == nullptr) {
        Logger::error(getClassName(), __FUNCTION__, "empty_client_info");
        return;
    }

    std::string appId = client->getAppId();

    RunningInfoPtr runningInfoPtr = RunningInfoManager::getInstance().getRunningInfo(appId);
    if (RuntimeStatus::CLOSING == runningInfoPtr->getRuntimeStatus()) {
        Logger::info(getClassName(), __FUNCTION__, "received event closed by sam");
    } else {
        Logger::info(getClassName(), __FUNCTION__, "received event closed by itself");
    }

    stopTimerToKillApp(appId);

    Logger::info(getClassName(), __FUNCTION__, appId, Logger::format("exit_status(%d)", status));
    removeNativeClientInfo(appId);

    EventAppLifeStatusChanged(appId, "", RuntimeStatus::STOP);
    EventRunningAppRemoved(appId);

    handlePendingQOnClosed(appId);
}

void NativeAppLifeHandler::handlePendingQOnRegistered(const std::string& appId)
{

    // handle all pending request as relaunch
    auto pending_item = m_launchPendingQueue.begin();
    while (pending_item != m_launchPendingQueue.end()) {
        if ((*pending_item)->getAppId() != appId) {
            ++pending_item;
            continue;
        } else {
            Logger::info(getClassName(), __FUNCTION__, appId, "launch_app_waiting_registration");
            launch(*pending_item);
            pending_item = m_launchPendingQueue.erase(pending_item);
        }
    }
}

void NativeAppLifeHandler::handlePendingQOnClosed(const std::string& appId)
{

    // handle only one pending request
    LaunchAppItemPtr pending_item = getLaunchPendingItem(appId);
    if (pending_item) {
        removeLaunchPendingItem(appId);
        Logger::info(getClassName(), __FUNCTION__, appId, "launch_app_waiting_previous_app_closed");
        launch(pending_item);
    }
}

void NativeAppLifeHandler::cancelLaunchPendingItemAndMakeItDone(const std::string& appId)
{

    auto pending_item = m_launchPendingQueue.begin();
    while (pending_item != m_launchPendingQueue.end()) {
        if ((*pending_item)->getAppId() != appId) {
            ++pending_item;
            continue;
        } else {
            Logger::info(getClassName(), __FUNCTION__, appId, "cancel_launch_request");
            (*pending_item)->setErrCodeText(APP_LAUNCH_ERR_GENERAL, "launching_cancled");
            EventLaunchingDone((*pending_item)->getUid());
            pending_item = m_launchPendingQueue.erase(pending_item);
        }
    }
}
