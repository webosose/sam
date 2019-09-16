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

#ifndef LIFECYCLE_STAGE_APPITEM_APPITEM_H_
#define LIFECYCLE_STAGE_APPITEM_APPITEM_H_

#include <iostream>

using namespace std;

class AppItem {
public:
    AppItem(const std::string& appId, const std::string& display, const std::string& pid);
    virtual ~AppItem();

    const std::string& getUid() const
    {
        return m_uid;
    }

    const std::string& getAppId() const
    {
        return m_appId;
    }

    const std::string& getDisplay() const
    {
        return m_display;
    }

    void setPid(const std::string& pid)
    {
        m_pid = pid;
    }
    const std::string& getPid() const
    {
        return m_pid;
    }

    void setCallerId(const std::string& id)
    {
        m_callerId = id;
    }
    const std::string& getCallerId() const
    {
        return m_callerId;
    }

    void setCallerPid(const std::string& pid)
    {
        m_callerPid = pid;
    }
    const std::string& getCallerPid() const
    {
        return m_callerPid;
    }

    void setReason(const std::string& launch_reason)
    {
        m_reason = launch_reason;
    }
    const std::string& getReason() const
    {
        return m_reason;
    }

private:
    string m_uid;
    string m_appId;
    string m_display;
    string m_pid;

    string m_callerId;
    string m_callerPid;
    string m_reason;
};

#endif /* LIFECYCLE_STAGE_APPITEM_APPITEM_H_ */
