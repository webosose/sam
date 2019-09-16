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

#include <lifecycle/handler/NativeClientInfo.h>
#include <util/LSUtils.h>
#include <util/Time.h>

NativeClientInfo::NativeClientInfo(const std::string& appId)
    : m_appId(appId),
      m_interfaceVersion(1),
      m_isRegistered(false),
      m_isRegistrationExpired(false),
      m_request(NULL),
      m_registrationCheckTimerSource(0),
      m_registrationCheckStartTime(0),
      m_lifecycleHandler(NULL)
{
    Logger::info(getClassName(), __FUNCTION__, m_appId, "client_info_created");
}

NativeClientInfo::~NativeClientInfo()
{
    if (m_request != NULL)
        LSMessageUnref(m_request);
    stopTimerForCheckingRegistration();

    Logger::info(getClassName(), __FUNCTION__, m_appId, "client_info_removed");
}

void NativeClientInfo::Register(LSMessage* request)
{
    if (request == NULL) {
        // leave warning log
        return;
    }

    if (m_request != NULL) {
        // leave log for release previous connection
        LSMessageUnref(m_request);
        m_request = NULL;
    }

    stopTimerForCheckingRegistration();

    m_request = request;
    LSMessageRef(m_request);
    m_isRegistered = true;
    m_isRegistrationExpired = false;
}

void NativeClientInfo::Unregister()
{
    if (m_request != NULL) {
        LSMessageUnref(m_request);
        m_request = NULL;
    }
    m_isRegistered = false;
}

void NativeClientInfo::startTimerForCheckingRegistration()
{
    stopTimerForCheckingRegistration();

    m_registrationCheckTimerSource = g_timeout_add(TIMEOUT_FOR_REGISTER_V2, NativeClientInfo::checkRegistration, (gpointer) (this));
    m_registrationCheckStartTime = Time::getCurrentTime();
    m_isRegistrationExpired = false;
}

void NativeClientInfo::stopTimerForCheckingRegistration()
{
    if (m_registrationCheckTimerSource != 0) {
        g_source_remove(m_registrationCheckTimerSource);
        m_registrationCheckTimerSource = 0;
    }
}

gboolean NativeClientInfo::checkRegistration(gpointer context)
{
    NativeClientInfo* native_client = static_cast<NativeClientInfo*>(context);

    if (!native_client->isRegistered()) {
        native_client->m_isRegistrationExpired = true;
    }

    native_client->stopTimerForCheckingRegistration();
    return FALSE;
}

void NativeClientInfo::setLifeCycleHandler(int ver, AbsNativeAppLifecycleInterface* handler)
{
    m_interfaceVersion = ver;
    m_lifecycleHandler = handler;
}

bool NativeClientInfo::sendEvent(pbnjson::JValue& payload)
{

    if (!m_isRegistered) {
        Logger::warning(getClassName(), __FUNCTION__, m_appId, "app_is_not_registered");
        return false;
    }

    if (payload.isObject() && payload.hasKey("returnValue") == false) {
        payload.put("returnValue", true);
    }

    LSErrorSafe lserror;
    if (!LSMessageRespond(m_request, payload.stringify().c_str(), &lserror)) {
        Logger::error(getClassName(), __FUNCTION__, m_appId, "respond");
        return false;
    }

    return true;
}
