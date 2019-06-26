// Copyright (c) 2017-2018 LG Electronics, Inc.
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
#include <memory>
#include <pbnjson.hpp>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <string>


class LunaTask {
public:
    LunaTask(LSHandle* lshandle, const std::string& category, const std::string& method, const std::string& caller, LSMessage* lsmsg, const pbnjson::JValue& jmsg)
        : m_lshandle(lshandle),
          m_category(category),
          m_method(method),
          m_caller(caller),
          m_lsmsg(lsmsg),
          m_jmsg(pbnjson::JValue()),
          m_returnPayload(pbnjson::Object()),
          m_errorCode(0)
    {
        if (!jmsg.isNull())
            m_jmsg = jmsg.duplicate();
        LSMessageRef(m_lsmsg);
    }

    virtual ~LunaTask()
    {
        if (m_lsmsg == NULL)
            return;
        LSMessageUnref(m_lsmsg);
    }

    void reply(const pbnjson::JValue& payload)
    {
        if (!m_lsmsg)
            return;
        if (!LSMessageIsSubscription(m_lsmsg))
            return;
        LSMessageRespond(m_lsmsg, payload.duplicate().stringify().c_str(), NULL);
    }
    void replyResult()
    {
        if (!m_lsmsg)
            return;
        if (!m_returnPayload.hasKey("returnValue") ||
            !m_returnPayload["returnValue"].isBoolean()) {
            m_returnPayload.put("returnValue", (m_errorText.empty() ? true : false));
        }
        if (!m_errorText.empty()) {
            m_returnPayload.put("errorCode", m_errorCode);
            m_returnPayload.put("errorText", m_errorText);
            LOG_INFO(MSGID_API_RETURN_FALSE, 4,
                     PMLOGKS("category", m_category.c_str()),
                     PMLOGKS("method", m_method.c_str()),
                     PMLOGKFV("err_code", "%d", m_errorCode),
                     PMLOGKS("err_text", m_errorText.c_str()), "");
        }
        (void) LSMessageRespond(m_lsmsg, m_returnPayload.stringify().c_str(), NULL);
    }
    void ReplyResult(const pbnjson::JValue& payload)
    {
        setReturnPayload(payload);
        replyResult();
    }
    void replyResultWithError(int32_t code, const std::string& text)
    {
        setError(code, text);
        replyResult();
    }

    LSHandle* lshandle() const
    {
        return m_lshandle;
    }
    const std::string& category() const
    {
        return m_category;
    }
    const std::string& method() const
    {
        return m_method;
    }
    const std::string& caller() const
    {
        return m_caller;
    }
    LSMessage* lsmsg() const
    {
        return m_lsmsg;
    }
    const pbnjson::JValue& jmsg() const
    {
        return m_jmsg;
    }
    void setReturnPayload(const pbnjson::JValue& payload)
    {
        m_returnPayload = payload.duplicate();
    }
    void setError(int32_t code, const std::string& text)
    {
        m_errorCode = code;
        m_errorText = text;
    }

private:
    LSHandle* m_lshandle;
    std::string m_category;
    std::string m_method;
    std::string m_caller;
    LSMessage* m_lsmsg;
    pbnjson::JValue m_jmsg;
    pbnjson::JValue m_returnPayload;
    int32_t m_errorCode;
    std::string m_errorText;
};

typedef std::shared_ptr<LunaTask> LunaTaskPtr;

#endif  // BUS_LUNATASK_H_
