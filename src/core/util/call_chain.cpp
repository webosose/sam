// Copyright (c) 2012-2019 LG Electronics, Inc.
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

#include <algorithm>

#include <boost/bind.hpp>

#include <core/util/call_chain.h>
#include <core/util/jutil.h>
#include <core/util/lsutils.h>

CallItem::CallItem() :
        m_chainData(pbnjson::Object())
{
}

void CallItem::setError(int errorCode, std::string errorText)
{
    m_errInfo.setError(errorCode, errorText);
}

ErrorInfo CallItem::getError() const
{
    return m_errInfo;
}

void CallItem::setChainData(pbnjson::JValue chainData)
{
    m_chainData = chainData.duplicate();
}

pbnjson::JValue CallItem::getChainData() const
{
    return m_chainData.duplicate();
}

void CallItem::setStatus(const std::string& status)
{
    m_status = status;
}

const std::string& CallItem::getStatus() const
{
    return m_status;
}

void CallItem::setMsg(const std::string& msg)
{
    m_msg = msg;
}

const std::string& CallItem::getMsg() const
{
    return m_msg;
}

LSCallItem::LSCallItem(LSHandle *handle, const char *uri, const char *payload) :
        m_handle(handle), m_uri(uri), m_payload(payload)
{
}

bool LSCallItem::Call()
{
    LSErrorSafe lserror;
    ErrorInfo errInfo;

    if (!onBeforeCall()) {
        errInfo.setError(CALLCHANIN_ERR_GENERAL, "Cancelled");
        onError(errInfo);
        return false;
    }

    if (!LSCallOneReply(m_handle, m_uri.c_str(), m_payload.c_str(), LSCallItem::handler, this, NULL, &lserror)) {
        errInfo.setError(CALLCHANIN_ERR_GENERAL, lserror.message);
        onError(errInfo);
        return false;
    }

    return true;
}

bool LSCallItem::onBeforeCall()
{
    return true;
}

bool LSCallItem::onReceiveCall(pbnjson::JValue message)
{
    return true;
}

void LSCallItem::setPayload(const char *payload)
{
    m_payload = payload;
}

bool LSCallItem::handler(LSHandle *lshandle, LSMessage *message, void *user_data)
{
    LSCallItem *call = reinterpret_cast<LSCallItem*>(user_data);
    if (!call)
        return false;

    pbnjson::JValue json = JUtil::parse(LSMessageGetPayload(message), std::string(""));

    bool result = call->onReceiveCall(json);

    call->onFinished(result, call->getError());

    return result;
}

gboolean CallChain::cbAsyncDelete(gpointer data)
{
    CallChain *p = reinterpret_cast<CallChain*>(data);
    if (!p)
        return false;

    delete p;
    return false;
}

CallChain& CallChain::acquire(CallCompleteHandler handler, CallNotifier notifier, void *user_data)
{
    CallChain *chain = new CallChain(handler, notifier, user_data);
    return *chain;
}

CallChain::CallChain(CallCompleteHandler handler, CallNotifier notifier, void *user_data) :
        m_handler(handler), m_notifier(notifier), m_user_data(user_data)
{
}

CallChain::~CallChain()
{
}

CallChain& CallChain::add(CallItemPtr call, bool push_front)
{
    call->onFinished.connect(boost::bind(&CallChain::onCallFinished, this, _1, _2));
    call->onError.connect(boost::bind(&CallChain::onCallError, this, _1));

    if (push_front)
        m_calls.push_front(call);
    else
        m_calls.push_back(call);
    return *this;
}

CallChain& CallChain::add_if(CallItemPtr condition_call, bool expected_result, CallItemPtr target_call)
{
    m_conditions.push_back(std::make_shared<CallCondition>(condition_call, expected_result, target_call));
    return *this;
}

bool CallChain::run(pbnjson::JValue chainData)
{
    return proceed(chainData);
}

bool CallChain::proceed(pbnjson::JValue chainData)
{
    if (m_calls.empty()) {
        ErrorInfo errInfo;
        chainData.put("returnValue", true);
        finish(chainData, errInfo);
        return true;
    }

    CallItemPtr call = m_calls.front();

    call->setChainData(chainData);
    if (!call->Call()) {
        return false;
    }

    return true;
}

void CallChain::finish(pbnjson::JValue chainData, ErrorInfo errInfo)
{
    if (m_handler)
        m_handler(chainData, errInfo, m_user_data);

    g_timeout_add(0, cbAsyncDelete, (gpointer) this);
}

void CallChain::onCallError(ErrorInfo errInfo)
{
    CallItemPtr call = m_calls.front();
    if (!call) {
        pbnjson::JValue result = pbnjson::Object();
        result.put("returnValue", false);
        finish(result, errInfo);
    } else {
        pbnjson::JValue chainData = call->getChainData();
        chainData.put("returnValue", false);
        finish(chainData, errInfo);
    }
}

void CallChain::onCallFinished(bool result, ErrorInfo errInfo)
{
    CallItemPtr call = m_calls.front();
    if (!call) {
        pbnjson::JValue chainData = pbnjson::Object();
        chainData.put("returnValue", false);

        ErrorInfo errCallInfo;
        errCallInfo.setError(CALLCHANIN_ERR_GENERAL, "Callchain broken");
        finish(chainData, errCallInfo);
        return;
    }
    m_calls.pop_front();

    if (m_notifier && !((call->getStatus()).empty()))
        m_notifier(call->getStatus(), call->getMsg(), result, m_user_data);

    if (!errInfo.errorText.empty()) {
        //if errorText is not empty, do not process next callChain.
        pbnjson::JValue chainData = call->getChainData();
        chainData.put("returnValue", false);
        finish(chainData, errInfo);
        return;
    }

    bool processNext = result;

    std::vector<CallConditionPtr>::iterator it = std::find_if(m_conditions.begin(), m_conditions.end(), [=] (const CallConditionPtr p) -> bool {return p->condition_call == call;});
    if (it != m_conditions.end()) {
        if (result == (*it)->expected_result) {
            add((*it)->target_call, true);
            processNext = true;
        }

        m_conditions.erase(it);
    }

    if (processNext) {
        proceed(call->getChainData());
    } else {
        pbnjson::JValue chainData = call->getChainData();
        chainData.put("returnValue", false);
        finish(chainData, errInfo);
    }
}
