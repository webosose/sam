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
#include <util/Utils.h>

NativeClientInfo::NativeClientInfo(const std::string& app_id)
    : m_appId(app_id),
      m_interfaceVersion(1),
      m_isRegistered(false),
      m_isRegistrationExpired(false),
      m_lsmsg(NULL),
      m_registrationCheckTimerSource(0),
      m_registrationCheckStartTime(0),
      m_lifecycleHandler(NULL)
{
    LOG_INFO(MSGID_NATIVE_CLIENT_INFO, 1, PMLOGKS("app_id", m_appId.c_str()), "client_info_created");
}

NativeClientInfo::~NativeClientInfo()
{
    if (m_lsmsg != NULL)
        LSMessageUnref(m_lsmsg);
    stopTimerForCheckingRegistration();

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

    stopTimerForCheckingRegistration();

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

void NativeClientInfo::startTimerForCheckingRegistration()
{
    stopTimerForCheckingRegistration();

    m_registrationCheckTimerSource = g_timeout_add(TIMEOUT_FOR_REGISTER_V2, NativeClientInfo::checkRegistration, (gpointer) (this));
    m_registrationCheckStartTime = getCurrentTime();
    m_isRegistrationExpired = false;
}

void NativeClientInfo::stopTimerForCheckingRegistration()
{
    if (m_registrationCheckTimerSource != 0) {
        g_source_remove(m_registrationCheckTimerSource);
        m_registrationCheckTimerSource = 0;
    }
}

gboolean NativeClientInfo::checkRegistration(gpointer user_data)
{
    NativeClientInfo* native_client = static_cast<NativeClientInfo*>(user_data);

    if (!native_client->isRegistered()) {
        native_client->m_isRegistrationExpired = true;
    }

    native_client->stopTimerForCheckingRegistration();
    return FALSE;
}

void NativeClientInfo::setLifeCycleHandler(int ver, INativeApp* handler)
{
    m_interfaceVersion = ver;
    m_lifecycleHandler = handler;
}

bool NativeClientInfo::sendEvent(pbnjson::JValue& payload)
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
