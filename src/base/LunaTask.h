// Copyright (c) 2017-2019 LG Electronics, Inc.
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

#ifndef BASE_LUNATASK_H_
#define BASE_LUNATASK_H_

#include <iostream>
#include <memory>
#include <list>
#include <boost/function.hpp>
#include <string>

#include <luna-service2/lunaservice.hpp>
#include <pbnjson.hpp>

#include "util/Logger.h"
#include "util/JValueUtil.h"
#include "util/Time.h"

using namespace std;
using namespace pbnjson;

class LunaTask;
class LunaTaskList;

typedef std::shared_ptr<LunaTask> LunaTaskPtr;
typedef boost::function<void(LunaTaskPtr)> LunaTaskCallback;

class LunaTask {
friend class LunaTaskList;
public:
    LunaTask(LSHandle* sh, LS::Message& request, JValue& requestPayload, LSMessage* message)
        : m_handle(sh),
          m_request(request),
          m_token(0),
          m_requestPayload(requestPayload),
          m_responsePayload(pbnjson::Object()),
          m_params(pbnjson::Object()),
          m_errorCode(ErrCode_UNKNOWN),
          m_errorText(""),
          m_pid(""),
          m_reason(""),
          m_preload(""),
          m_stopTime(0)
    {
        if (m_request.getApplicationID() != nullptr){
            m_caller = m_request.getApplicationID();
        }

        if (m_caller.empty()) {
            m_caller = m_request.getSenderServiceName();
        }

        size_t index = m_caller.find(" ");
        if (std::string::npos != index) {
            m_callerId = m_caller.substr(0, index);
            m_callerPid = m_caller.substr(index + 1);
        }
        m_startTime = Time::getCurrentTime();
    }

    virtual ~LunaTask()
    {

    }

    LSHandle* getHandle() const
    {
        return m_handle;
    }

    LS::Message& getRequest()
    {
        return m_request;
    }

    LSMessage* getMessage()
    {
        return m_request.get();
    }

    LSMessageToken getToken() const
    {
        return m_token;
    }
    void setToken(LSMessageToken token)
    {
        m_token = token;
    }

    const pbnjson::JValue& getRequestPayload() const
    {
        return m_requestPayload;
    }

    pbnjson::JValue& getResponsePayload()
    {
        return m_responsePayload;
    }

    pbnjson::JValue& getParams()
    {
        return m_params;
    }

    void setErrCodeAndText(int errorCode, std::string errorText)
    {
        m_errorCode = errorCode;
        m_errorText = errorText;
        m_responsePayload.put("returnValue", false);
    }

    const std::string getInstanceId() const
    {
        string instanceId = "";
        JValueUtil::getValue(m_requestPayload, "instanceId", instanceId);
        return instanceId;
    }

    const std::string getLaunchPointId() const
    {
        string launchPointId = "";
        JValueUtil::getValue(m_requestPayload, "launchPointId", launchPointId);
        return launchPointId;
    }

    const std::string getAppId() const
    {
        string appId = "";
        JValueUtil::getValue(m_requestPayload, "id", appId);
        return appId;
    }

    const std::string& getPid() const
    {
        return m_pid;
    }
    void setPid(const std::string& pid)
    {
        m_pid = pid;
    }

    const std::string getCaller() const
    {
        return m_caller;
    }

    const std::string& getCallerId() const
    {
        return m_callerId;
    }

    const std::string& getCallerPid() const
    {
        return m_callerPid;
    }

    const std::string& getReason() const
    {
        return m_reason;
    }
    void setReason(const std::string& reason)
    {
        m_reason = reason;
    }

    const std::string& getPreload() const
    {
        return m_preload;
    }
    void setPreload(const std::string& preload)
    {
        m_preload = preload;
    }

    bool isKeepAlive() const
    {
        bool keepAlive = false;
        JValueUtil::getValue(m_requestPayload, "keepAlive", keepAlive);
        return keepAlive;
    }

    double getTotalTime() const
    {
        return m_stopTime- m_startTime;
    }

    void stopTime()
    {
        m_stopTime = Time::getCurrentTime();
    }

    LunaTaskCallback getAPICallback()
    {
        return m_APICallback;
    }
    void setAPICallback(LunaTaskCallback callback)
    {
        m_APICallback = callback;
    }

    const std::string& getNextStep() const
    {
        return m_nextStep;
    }
    void setNextStep(const std::string& next)
    {
        m_nextStep = next;
    }

    void toJson(JValue& json)
    {
        if (json.isNull())
            json = pbnjson::Object();
        json.put("caller", getCaller());
        json.put("kind", this->getRequest().getKind());
    }

private:
    LunaTask& operator=(const LunaTask& lunaTask) = delete;
    LunaTask(const LunaTask& lunaTask) = delete;

    void reply()
    {
        bool returnValue = true;
        if (!m_errorText.empty() && !m_responsePayload.hasKey("errorText")) {
            m_responsePayload.put("errorText", m_errorText);
            returnValue = false;
        }
        if (m_errorCode != 0 && !m_responsePayload.hasKey("errorCode")) {
            m_responsePayload.put("errorCode", m_errorCode);
            returnValue = false;
        }
        m_responsePayload.put("returnValue", returnValue);
        m_request.respond(m_responsePayload.stringify().c_str());
    }

    LSHandle* m_handle;
    LS::Message m_request;
    LSMessageToken m_token;

    pbnjson::JValue m_requestPayload;
    pbnjson::JValue m_responsePayload;
    pbnjson::JValue m_params;

    int m_errorCode;
    string m_errorText;

    string m_pid;

    string m_caller;
    string m_callerId;
    string m_callerPid;
    string m_reason;
    string m_preload;

    double m_startTime;
    double m_stopTime;

    LunaTaskCallback m_APICallback;
    string m_nextStep;
};

#endif  // BASE_LUNATASK_H_
