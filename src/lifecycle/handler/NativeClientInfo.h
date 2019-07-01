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

#ifndef LIFECYCLE_HANDLER_NATIVECLIENTINFO_H_
#define LIFECYCLE_HANDLER_NATIVECLIENTINFO_H_

#include <iostream>
#include <util/Logging.h>
#include <lunaservice.h>
#include <pbnjson.hpp>
#include <boost/function.hpp>
#include "../stage/appitem/LaunchAppItem.h"

class AbsNativeAppLifecycleInterface;

class NativeClientInfo {
public:
    NativeClientInfo(const std::string& app_id);
    ~NativeClientInfo();

    void Register(LSMessage* lsmsg);
    void Unregister();

    void startTimerForCheckingRegistration();
    void stopTimerForCheckingRegistration();
    static gboolean checkRegistration(gpointer user_data);

    void setLifeCycleHandler(int ver, AbsNativeAppLifecycleInterface* handler);
    AbsNativeAppLifecycleInterface* getLifeCycleHandler() const
    {
        return m_lifecycleHandler;
    }
    bool sendEvent(pbnjson::JValue& payload);
    void setAppId(const std::string& app_id)
    {
        m_appId = app_id;
    }
    void setPid(const std::string& pid)
    {
        m_pid = pid;
    }

    const std::string& getAppId() const
    {
        return m_appId;
    }
    const std::string& getPid() const
    {
        return m_pid;
    }
    int getInterfaceVersion() const
    {
        return m_interfaceVersion;
    }
    bool isRegistered() const
    {
        return m_isRegistered;
    }
    bool isRegistrationExpired() const
    {
        return m_isRegistrationExpired;
    }

private:
    static const int TIMEOUT_FOR_REGISTER_V2 = 3000;

    std::string m_appId;
    std::string m_pid;
    int m_interfaceVersion;
    bool m_isRegistered;
    bool m_isRegistrationExpired;
    LSMessage* m_lsmsg;
    guint m_registrationCheckTimerSource;
    double m_registrationCheckStartTime;
    AbsNativeAppLifecycleInterface* m_lifecycleHandler;
};

typedef std::shared_ptr<NativeClientInfo> NativeClientInfoPtr;
typedef boost::function<void(NativeClientInfoPtr, LaunchAppItemPtr)> NativeAppLaunchHandler;

#endif /* LIFECYCLE_HANDLER_NATIVECLIENTINFO_H_ */
