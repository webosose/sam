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

typedef shared_ptr<LunaTask> LunaTaskPtr;
typedef boost::function<void(LunaTaskPtr)> LunaTaskCallback;

class LunaTask {
friend class LunaTaskList;
public:
    LunaTask(LS::Message& request, JValue& requestPayload, LSMessage* message)
        : m_instanceId(""),
          m_request(request),
          m_token(0),
          m_requestPayload(requestPayload),
          m_responsePayload(pbnjson::Object()),
          m_errorCode(ErrCode_UNKNOWN),
          m_errorText(""),
          m_reason("")
    {
        m_startTime = Time::getCurrentTime();
    }

    virtual ~LunaTask()
    {

    }

    const string getInstanceId()
    {
        if (!m_instanceId.empty())
            return m_instanceId;

        JValueUtil::getValue(m_requestPayload, "instanceId", m_instanceId);
        return m_instanceId;
    }
    void setInstanceId(const string& instanceId)
    {
        m_instanceId = instanceId;
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

    const JValue& getRequestPayload() const
    {
        return m_requestPayload;
    }

    JValue& getResponsePayload()
    {
        return m_responsePayload;
    }

    JValue getParams()
    {
        if (m_requestPayload.hasKey("params"))
            return m_requestPayload["params"];
        else
            return pbnjson::Object();
    }

    void setErrCodeAndText(int errorCode, string errorText)
    {
        m_errorCode = errorCode;
        m_errorText = errorText;
        m_responsePayload.put("returnValue", false);
        Logger::warning("LunaTask", __FUNCTION__, Logger::format("errorCode(%d) errorText(%s)", errorCode, errorText.c_str()));
    }

    const string getLaunchPointId() const
    {
        string launchPointId = "";
        JValueUtil::getValue(m_requestPayload, "launchPointId", launchPointId);
        return launchPointId;
    }

    const string getId()
    {
        string appId = getAppId();
        string launchPointId = getLaunchPointId();
        string instanceId = getInstanceId();

        if (!instanceId.empty())
            return instanceId;
        if (!launchPointId.empty())
            return launchPointId;
        if (!appId.empty())
            return appId;
        return "unknown";
    }

    const string getAppId() const
    {
        string appId = "";
        JValueUtil::getValue(m_requestPayload, "id", appId);
        return appId;
    }

    const int getDisplayId() const;

    const string getCaller() const
    {
        if (m_request.getApplicationID() != nullptr) {
            return m_request.getApplicationID();
        } else {
            return m_request.getSenderServiceName();
        }
    }

    const string getWebprocessId() const
    {
        // This API only valid when sender is application
        string webprocessid = m_request.getSenderServiceName();
        size_t index = webprocessid.find("-");
        if (string::npos == index) {
            return "";
        }
        return webprocessid.substr(index + 1);
    }

    const string& getReason()
    {
        if (!m_reason.empty())
            return m_reason;

        JValueUtil::getValue(m_requestPayload, "params", "reason", m_reason);
        if (m_reason.empty()) {
            m_reason = "normal";
        }
        return m_reason;
    }
    void setReason(const string& reason)
    {
        m_reason = reason;
    }

    double getTimeStamp() const
    {
        double now = Time::getCurrentTime();
        return (now - m_startTime);
    }

    LunaTaskCallback getAPICallback()
    {
        return m_APICallback;
    }
    void setAPICallback(LunaTaskCallback callback)
    {
        m_APICallback = callback;
    }

    const string& getNextStep() const
    {
        return m_nextStep;
    }
    void setNextStep(const string& next)
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

    string m_instanceId;
    LS::Message m_request;
    LSMessageToken m_token;

    JValue m_requestPayload;
    JValue m_responsePayload;

    int m_errorCode;
    string m_errorText;

    string m_reason;

    double m_startTime;

    LunaTaskCallback m_APICallback;
    string m_nextStep;
};

#endif  // BASE_LUNATASK_H_
