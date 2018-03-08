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

#ifndef CORE_BUS_LUNA_TASK_H_
#define CORE_BUS_LUNA_TASK_H_

#include <luna-service2/lunaservice.h>
#include <memory>
#include <pbnjson.hpp>
#include <string>

#include "core/base/jutil.h"
#include "core/base/logging.h"

class LunaTask {
 public:
  LunaTask(LSHandle* lshandle, const std::string& category, const std::string& method,
      const std::string& caller, LSMessage* lsmsg, const pbnjson::JValue& jmsg)
      : lshandle_(lshandle),
        category_(category),
        method_(method),
        caller_(caller),
        lsmsg_(lsmsg),
        jmsg_(pbnjson::JValue()),
        return_payload_(pbnjson::Object()),
        error_code_(0) {
    if (!jmsg.isNull()) jmsg_ = jmsg.duplicate();
    LSMessageRef(lsmsg_);
  }
  ~LunaTask() {
    if(lsmsg_  == NULL) return;
    LSMessageUnref(lsmsg_);
  }

  void Reply(const pbnjson::JValue& payload) {
    if (!lsmsg_) return;
    if (!LSMessageIsSubscription(lsmsg_)) return;
    (void) LSMessageRespond(lsmsg_, JUtil::jsonToString(payload).c_str(), NULL);
  }
  void ReplyResult() {
    if (!lsmsg_) return;
    if(!return_payload_.hasKey("returnValue") || !return_payload_["returnValue"].isBoolean()) {
      return_payload_.put("returnValue", (error_text_.empty() ? true : false));
    }
    if(!error_text_.empty()) {
      return_payload_.put("errorCode", error_code_);
      return_payload_.put("errorText", error_text_);
      LOG_INFO(MSGID_API_RETURN_FALSE, 4, PMLOGKS("category", category_.c_str()),
                                          PMLOGKS("method", method_.c_str()),
                                          PMLOGKFV("err_code", "%d", error_code_),
                                          PMLOGKS("err_text", error_text_.c_str()), "");
    }
    (void) LSMessageRespond(lsmsg_, JUtil::jsonToString(return_payload_).c_str(), NULL);
  }
  void ReplyResult(const pbnjson::JValue& payload) {
    SetReturnPayload(payload);
    ReplyResult();
  }
  void ReplyResultWithError(int32_t code, const std::string& text) {
    SetError(code, text);
    ReplyResult();
  }

  LSHandle* lshandle() const { return lshandle_; }
  const std::string& category() const { return category_; }
  const std::string& method() const { return method_; }
  const std::string& caller() const { return caller_; }
  LSMessage* lsmsg() const { return lsmsg_; }
  const pbnjson::JValue& jmsg() const { return jmsg_; }
  void set_jmsg(const pbnjson::JValue& params) { jmsg_ = params.duplicate(); }

  void SetReturnPayload(const pbnjson::JValue& payload) { return_payload_ = payload.duplicate(); }
  void SetError(int32_t code, const std::string& text) { error_code_ = code; error_text_ = text; }

 private:
  LSHandle*       lshandle_;
  std::string     category_;
  std::string     method_;
  std::string     caller_;
  LSMessage*      lsmsg_;
  pbnjson::JValue jmsg_;
  pbnjson::JValue return_payload_;
  int32_t         error_code_;
  std::string     error_text_;
};
typedef std::shared_ptr<LunaTask> LunaTaskPtr;

#endif  // CORE_BUS_LUNA_TASK_H_
