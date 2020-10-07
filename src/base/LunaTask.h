// Copyright (c) 2017-2020 LG Electronics, Inc.
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
          m_launchPointId(""),
          m_appId(""),
          m_request(request),
          m_token(0),
          m_requestPayload(requestPayload),
          m_responsePayload(pbnjson::Object()),
          m_errorCode(ErrCode_NOERROR),
          m_errorText(""),
          m_reason("")
    {
        JValueUtil::getValue(m_requestPayload, "instanceId", m_instanceId);
        JValueUtil::getValue(m_requestPayload, "launchPointId", m_launchPointId);
        JValueUtil::getValue(m_requestPayload, "id", m_appId);
    }

    virtual ~LunaTask()
    {

    }

    const string& getInstanceId() const
    {
        return m_instanceId;
    }
    void setInstanceId(const string& instanceId)
    {
        m_instanceId = instanceId;
    }

    const string& getLaunchPointId() const
    {
        return m_launchPointId;
    }
    void setLaunchPointId(const string& launchPointId)
    {
        m_launchPointId = launchPointId;
    }

    const string& getAppId() const
    {
        return m_appId;
    }
    void setAppId(const string& appId)
    {
        m_appId = appId;
    }

    const string& getId() const
    {
        if (!m_instanceId.empty())
            return m_instanceId;
        if (!m_launchPointId.empty())
            return m_launchPointId;
        return m_appId;
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
            return m_requestPayload["params"].duplicate();
        else
            return pbnjson::Object();
    }
    void setParams(JValue params)
    {
        m_requestPayload.put("params", params.duplicate());
    }

    void setErrCodeAndText(int errorCode, string errorText)
    {
        m_errorCode = errorCode;
        m_errorText = errorText;
        m_responsePayload.put("returnValue", false);
        Logger::warning("LunaTask", __FUNCTION__, Logger::format("errorCode(%d) errorText(%s)", errorCode, errorText.c_str()));
    }

    int getDisplayId();
    void setDisplayId(const int displayId);

    const string getCaller() const
    {
        if (m_request.getApplicationID() != nullptr) {
            return m_request.getApplicationID();
        } else if (m_request.getSenderServiceName() != nullptr){
            return m_request.getSenderServiceName();
        } else {
            return "unknown";
        }
    }

    bool isLaunchedHidden()
    {
        bool launchedHidden = false;
        JValueUtil::getValue(m_requestPayload, "params", "launchedHidden", launchedHidden);
        return launchedHidden;
    }

    const string& getReason()
    {
        if (!m_reason.empty())
            return m_reason;

        JValueUtil::getValue(m_requestPayload, "reason", m_reason);
        if (m_reason.empty()) {
            m_reason = getCaller();
        }
        return m_reason;
    }
    void setReason(const string& reason)
    {
        m_reason = reason;
    }

    void setSuccessCallback(LunaTaskCallback callback)
    {
        m_successCallback = callback;
    }
    bool hasSuccessCallback()
    {
        return !m_successCallback.empty();
    }
    void success(LunaTaskPtr lunaTask)
    {
        if (!m_successCallback.empty()) {
            m_successCallback(lunaTask);
        }
    }

    void setErrorCallback(LunaTaskCallback callback)
    {
        m_errorCallback = callback;
    }
    bool hasErrorCallback()
    {
        return !m_successCallback.empty();
    }
    void error(LunaTaskPtr lunaTask)
    {
        if (!m_errorCallback.empty()) {
            m_errorCallback(lunaTask);
        }
    }

    const string& getNextStep() const
    {
        return m_nextStep;
    }
    void setNextStep(const string& next)
    {
        m_nextStep = next;
    }

    bool isDevmodeRequest()
    {
        return (strcmp(m_request.getCategory(), "/dev") == 0);
    }

    void toAPIJson(JValue& json)
    {
        if (json.isNull())
            json = pbnjson::Object();

        json.put("caller", getCaller());
        json.put("kind", this->getRequest().getKind());
    }

    void fillIds(JValue& json)
    {
        json.put("instanceId", getInstanceId());
        json.put("launchPointId", getLaunchPointId());
        json.put("appId", getAppId());
        if (getDisplayId() != -1)
            json.put("displayId", getDisplayId());
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
    string m_launchPointId;
    string m_appId;

    LS::Message m_request;
    LSMessageToken m_token;

    JValue m_requestPayload;
    JValue m_responsePayload;

    int m_errorCode;
    string m_errorText;

    string m_reason;

    LunaTaskCallback m_successCallback;
    LunaTaskCallback m_errorCallback;

    string m_nextStep;
};

#endif  // BASE_LUNATASK_H_
