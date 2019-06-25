// Copyright (c) 2012-2018 LG Electronics, Inc.
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

#ifndef CALLCHAIN_H
#define CALLCHAIN_H

#include <deque>
#include <memory>
#include <vector>
#include <vector>

#include <boost/signals2.hpp>
#include <glib.h>
#include <lifecycle/ApplicationErrors.h>
#include <luna-service2/lunaservice.h>
#include <pbnjson.hpp>


class CallChain;
class CallItem {
    friend class CallChain;
public:
    CallItem();
    virtual ~CallItem()
    {
    }

    virtual bool Call() = 0;

    boost::signals2::signal<void(bool, ErrorInfo errInfo)> onFinished;
    boost::signals2::signal<void(ErrorInfo errInfo)> onError;

protected:
    void setError(int errorCode, std::string errorText);
    ErrorInfo getError() const;

    void setChainData(pbnjson::JValue value);
    pbnjson::JValue getChainData() const;

    void setStatus(const std::string& status);
    const std::string& getStatus() const;

    void setMsg(const std::string& msg);
    const std::string& getMsg() const;

private:
    ErrorInfo m_errInfo;
    pbnjson::JValue m_chainData;
    std::string m_status;
    std::string m_msg;
};

class LSCallItem: public CallItem {
public:
    LSCallItem(LSHandle *handle, const char *uri, const char *payload);
    virtual ~LSCallItem()
    {
    }

    virtual bool Call();

protected:
    virtual bool onBeforeCall();
    virtual bool onReceiveCall(pbnjson::JValue message);
    void setPayload(const char *payload);

private:
    static bool handler(LSHandle *lshandle, LSMessage *message, void *user_data);

private:
    LSHandle *m_handle;
    std::string m_uri;
    std::string m_payload;
};

class CallChain {
    typedef std::shared_ptr<CallItem> CallItemPtr;
    typedef std::function<void(pbnjson::JValue, ErrorInfo, void*)> CallCompleteHandler;
    typedef std::function<void(const std::string&, const std::string&, bool, void*)> CallNotifier;

    struct CallCondition {
        CallCondition(CallItemPtr _condition_call, bool _expected_result, CallItemPtr _target_call)
            : m_conditionCall(_condition_call),
              m_expectedResult(_expected_result),
              m_targetCall(_target_call)
        {
        }

        CallItemPtr m_conditionCall;
        bool m_expectedResult;
        CallItemPtr m_targetCall;
    };

    typedef std::shared_ptr<CallCondition> CallConditionPtr;

public:
    static CallChain& acquire(CallCompleteHandler handler = nullptr, CallNotifier notifier = nullptr, void *user_data = NULL);

    CallChain& add(CallItemPtr call, bool push_front = false);
    CallChain& addIf(CallItemPtr condition_call, bool expected_result, CallItemPtr target_call);

    bool run(pbnjson::JValue chainData = pbnjson::Object());

private:
    CallChain(CallCompleteHandler handler, CallNotifier notifier, void *user_data);
    virtual ~CallChain();

    bool proceed(pbnjson::JValue chainData);
    void finish(pbnjson::JValue chainData, ErrorInfo errInfo);

    void onCallFinished(bool result, ErrorInfo errInfo);
    void onCallError(ErrorInfo errInfo);

    void setNotifier(CallNotifier notifier)
    {
        m_notifier = notifier;
    }
    static gboolean cbAsyncDelete(gpointer data);

private:
    std::deque<CallItemPtr> m_calls;
    std::vector<CallConditionPtr> m_conditions;

    CallCompleteHandler m_handler;
    CallNotifier m_notifier;
    void *m_user_data;
};

#endif
