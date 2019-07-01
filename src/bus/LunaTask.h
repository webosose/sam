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

#ifndef BUS_LUNATASK_H_
#define BUS_LUNATASK_H_

#include <luna-service2/lunaservice.h>
#include <luna-service2/lunaservice.hpp>
#include <memory>
#include <pbnjson.hpp>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <string>

using namespace std;
using namespace pbnjson;

class LunaTask {
public:
    LunaTask(LSHandle* lshandle, LS::Message& request, LSMessage* lsMessage)
        : m_lshandle(lshandle),
          m_message(request),
          m_lsMessage(lsMessage),
          m_responsePayload(pbnjson::Object()),
          m_errorCode(0),
          m_errorText("")
    {
        m_requestPayload = JDomParser::fromString(m_message.getPayload());
    }

    virtual ~LunaTask()
    {
    }

    void reply()
    {
        if (!m_responsePayload.hasKey("returnValue") ||
            !m_responsePayload["returnValue"].isBoolean()) {
            m_responsePayload.put("returnValue", (m_errorText.empty() ? true : false));
        }
        if (!m_errorText.empty()) {
            m_responsePayload.put(LOG_KEY_ERRORCODE, m_errorCode);
            m_responsePayload.put(LOG_KEY_ERRORTEXT, m_errorText);
            writeErrorLog(m_errorText);
        }
        m_message.respond(m_responsePayload.stringify().c_str());
    }

    void replyResult(const pbnjson::JValue& responsePayload)
    {
        m_responsePayload = responsePayload.duplicate();
        reply();
    }

    void replyResultWithError(int32_t code, const std::string& text)
    {
        setError(code, text);
        reply();
    }

    LSHandle* getLSHandle() const
    {
        return m_lshandle;
    }
    const char* category() const
    {
        return m_message.getCategory();
    }
    const std::string method() const
    {
        if (m_message.getMethod())
            return m_message.getMethod();
        return "";
    }
    const std::string caller() const
    {
        std::string caller = "";

        if (m_message.getApplicationID() != nullptr){
            caller = m_message.getApplicationID();
        }

        if (caller.empty()) {
            caller = m_message.getSenderServiceName();
        }
        return caller;
    }
    LSMessage* lsmsg() const
    {
        return m_lsMessage;
    }
    const pbnjson::JValue& getRequestPayload() const
    {
        return m_requestPayload;
    }
    void setError(int32_t code, const std::string& text)
    {
        m_errorCode = code;
        m_errorText = text;
    }

    LS::Message& getMessage()
    {
        return m_message;
    }

    void writeInfoLog(const string& status)
    {
        LOG_INFO(MSGID_API_REQUEST, 4,
                 PMLOGKS("category", m_message.getCategory()),
                 PMLOGKS("method", m_message.getMethod()),
                 PMLOGKS("caller", this->caller().c_str()),
                 PMLOGKS("status", status.c_str()), "");
        LOG_DEBUG("params:%s", m_requestPayload.stringify().c_str());
    }

    void writeErrorLog(const string& errorText)
    {
        LOG_WARNING(MSGID_API_REQUEST, 4,
                    PMLOGKS("category", m_message.getCategory()),
                    PMLOGKS("method", m_message.getMethod()),
                    PMLOGKS("caller", this->caller().c_str()),
                    PMLOGKS("errorText", errorText.c_str()), "");
    }

private:
    LSHandle* m_lshandle;
    LS::Message m_message;
    LSMessage* m_lsMessage;

    pbnjson::JValue m_requestPayload;
    pbnjson::JValue m_responsePayload;

    int32_t m_errorCode;
    std::string m_errorText;
};

typedef std::shared_ptr<LunaTask> LunaTaskPtr;

class LunaTaskFactory {
public:
    LunaTaskFactory() {}
    virtual ~LunaTaskFactory() {}

    static LunaTaskPtr createLunaTask()
    {
        return nullptr;
    }
};

#endif  // BUS_LUNATASK_H_
