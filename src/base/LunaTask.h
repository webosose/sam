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

typedef std::shared_ptr<LunaTask> LunaTaskPtr;
typedef boost::function<void(LunaTaskPtr)> APICallback;

const std::string SYS_LAUNCHING_UID = "alertId";

class LunaTask {
public:
    LunaTask(LSHandle* sh, LS::Message& request, JValue& requestPayload, LSMessage* message)
        : m_handle(sh),
          m_request(request),
          m_token(0),
          m_requestPayload(requestPayload),
          m_responsePayload(pbnjson::Object()),
          m_params(pbnjson::Object()),
          m_errorCode(0),
          m_errorText(""),
          m_instanceId(""),
          m_pid(""),
          m_reason(""),
          m_preload(""),
          m_showSplash(true),
          m_showSpinner(true),
          m_keepAlive(false),
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

    void reply(const pbnjson::JValue& responsePayload)
    {
        m_responsePayload = responsePayload.duplicate();
        JValueUtil::getValue(m_responsePayload, "errorText", m_errorText);
        JValueUtil::getValue(m_responsePayload, "errorCode", m_errorCode);
        reply();
    }

    void reply(int32_t errorCode, const std::string& errorText)
    {
        setErrorCodeAndText(errorCode, errorText);
        reply();
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
    void resetToken()
    {
        m_token = 0;
    }

    LS::Call& getCall()
    {
        return m_call;
    }
    void setCall(LS::Call& call)
    {
        m_call = std::move(call);
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

    int getErrorCode() const
    {
        return m_errorCode;
    }
    const std::string& getErrorText() const
    {
        return m_errorText;
    }
    void setErrorCodeAndText(int errorCode, std::string errorText)
    {
        m_errorCode = errorCode;
        m_errorText = errorText;
    }

    const std::string& getInstanceId() const
    {
        return m_instanceId;
    }
    void setInstanceId(const std::string& instanceId)
    {
        m_instanceId = instanceId;
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

    bool isShowSplash() const
    {
        return m_showSplash;
    }
    void setNoSplash(bool v)
    {
        m_showSplash = v;
    }

    bool isShowSpinner() const
    {
        return m_showSpinner;
    }
    void setSpinner(bool v)
    {
        m_showSpinner = v;
    }

    bool isKeepAlive() const
    {
        return m_keepAlive;
    }
    void setKeepAlive(bool v)
    {
        m_keepAlive = v;
    }

    double getTotalTime() const
    {
        return m_stopTime- m_startTime;
    }

    void stopTime()
    {
        m_stopTime = Time::getCurrentTime();
    }

    APICallback getAPICallback()
    {
        return m_APICallback;
    }
    void setAPICallback(APICallback callback)
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

    LSHandle* m_handle;
    LS::Message m_request;
    LSMessageToken m_token;
    LS::Call m_call;

    pbnjson::JValue m_requestPayload;
    pbnjson::JValue m_responsePayload;
    pbnjson::JValue m_params;

    int32_t m_errorCode;
    string m_errorText;

    string m_instanceId;
    string m_pid;

    string m_caller;
    string m_callerId;
    string m_callerPid;
    string m_reason;
    string m_preload;

    bool m_showSplash;
    bool m_showSpinner;
    bool m_keepAlive;

    double m_startTime;
    double m_stopTime;

    APICallback m_APICallback;
    string m_nextStep;
};

#endif  // BASE_LUNATASK_H_
